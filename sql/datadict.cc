/* Copyright (c) 2010, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1335  USA */

#include "mariadb.h"
#include "datadict.h"
#include "sql_priv.h"
#include "sql_base.h"
#include "sql_class.h"
#include "sql_table.h"
#include "ha_sequence.h"
#include "discover.h"

static const char * const tmp_fk_prefix= "#sqlf";
static const char * const bak_ext= ".fbk";

static int read_string(File file, uchar**to, size_t length)
{
  DBUG_ENTER("read_string");

  /* This can't use MY_THREAD_SPECIFIC as it's used on server start */
  if (!(*to= (uchar*) my_malloc(length+1,MYF(MY_WME))) ||
      mysql_file_read(file, *to, length, MYF(MY_NABP)))
  {
     my_free(*to);
    *to= 0;
    DBUG_RETURN(1);
  }
  *((char*) *to+length)= '\0'; // C-style safety
  DBUG_RETURN (0);
}


/**
  Check type of .frm if we are not going to parse it.

  @param[in]  thd               The current session.
  @param[in]  path              path to FRM file.
  @param[in/out] engine_name    table engine name (length < NAME_CHAR_LEN)

  engine_name is a LEX_CSTRING, where engine_name->str must point to
  a buffer of at least NAME_CHAR_LEN+1 bytes.
  If engine_name is 0, then the function will only test if the file is a
  view or not

  @param[out] is_sequence  1 if table is a SEQUENCE, 0 otherwise

  @retval  TABLE_TYPE_UNKNOWN   error  - file can't be opened
  @retval  TABLE_TYPE_NORMAL    table
  @retval  TABLE_TYPE_SEQUENCE  sequence table
  @retval  TABLE_TYPE_VIEW      view
*/

Table_type dd_frm_type(THD *thd, char *path, LEX_CSTRING *engine_name,
                       bool *is_sequence)
{
  File file;
  uchar header[40];     //"TYPE=VIEW\n" it is 10 characters
  size_t error;
  Table_type type= TABLE_TYPE_UNKNOWN;
  uchar dbt;
  DBUG_ENTER("dd_frm_type");

  *is_sequence= 0;

  if ((file= mysql_file_open(key_file_frm, path, O_RDONLY | O_SHARE, MYF(0)))
      < 0)
    DBUG_RETURN(TABLE_TYPE_UNKNOWN);

  /*
    We return TABLE_TYPE_NORMAL if we can open the .frm file. This allows us
    to drop a bad .frm file with DROP TABLE
  */
  type= TABLE_TYPE_NORMAL;

  /*
    Initialize engine name in case we are not able to find it out
    The cast is safe, as engine_name->str points to a usable buffer.
   */
  if (engine_name)
  {
    engine_name->length= 0;
    ((char*) (engine_name->str))[0]= 0;
  }

  if (unlikely((error= mysql_file_read(file, (uchar*) header, sizeof(header), MYF(MY_NABP)))))
    goto err;

  if (unlikely((!strncmp((char*) header, "TYPE=VIEW\n", 10))))
  {
    type= TABLE_TYPE_VIEW;
    goto err;
  }

  /* engine_name is 0 if we only want to know if table is view or not */
  if (!engine_name)
    goto err;

  if (!is_binary_frm_header(header))
    goto err;

  dbt= header[3];

  if (((header[39] >> 4) & 3) == HA_CHOICE_YES)
  {
    DBUG_PRINT("info", ("Sequence found"));
    *is_sequence= 1;
  }

  /* cannot use ha_resolve_by_legacy_type without a THD */
  if (thd && dbt < DB_TYPE_FIRST_DYNAMIC)
  {
    handlerton *ht= ha_resolve_by_legacy_type(thd, (enum legacy_db_type)dbt);
    if (ht)
    {
      *engine_name= hton2plugin[ht->slot]->name;
      goto err;
    }
  }

  /* read the true engine name */
  {
    MY_STAT state;  
    uchar *frm_image= 0;
    uint n_length;

    if (mysql_file_fstat(file, &state, MYF(MY_WME)))
      goto err;

    if (mysql_file_seek(file, 0, SEEK_SET, MYF(MY_WME)))
      goto err;

    if (read_string(file, &frm_image, (size_t)state.st_size))
      goto err;

    if ((n_length= uint4korr(frm_image+55)))
    {
      uint record_offset= uint2korr(frm_image+6)+
                      ((uint2korr(frm_image+14) == 0xffff ?
                        uint4korr(frm_image+47) : uint2korr(frm_image+14)));
      uint reclength= uint2korr(frm_image+16);

      uchar *next_chunk= frm_image + record_offset + reclength;
      uchar *buff_end= next_chunk + n_length;
      uint connect_string_length= uint2korr(next_chunk);
      next_chunk+= connect_string_length + 2;
      if (next_chunk + 2 < buff_end)
      {
        uint len= uint2korr(next_chunk);
        if (len <= NAME_CHAR_LEN)
        {
          /*
            The following cast is safe as the caller has allocated buffer
            and it's up to this function to generate the name.
          */
          strmake((char*) engine_name->str, (char*)next_chunk + 2,
                  engine_name->length= len);
        }
      }
    }

    my_free(frm_image);
  }

  /* Probably a table. */
err:
  mysql_file_close(file, MYF(MY_WME));
  DBUG_RETURN(type);
}


/*
  Regenerate a metadata locked table.

  @param  thd   Thread context.
  @param  db    Name of the database to which the table belongs to.
  @param  name  Table name.

  @retval  FALSE  Success.
  @retval  TRUE   Error.
*/

bool dd_recreate_table(THD *thd, const char *db, const char *table_name)
{
  HA_CREATE_INFO create_info;
  char path_buf[FN_REFLEN + 1];
  DBUG_ENTER("dd_recreate_table");

  /* There should be a exclusive metadata lock on the table. */
  DBUG_ASSERT(thd->mdl_context.is_lock_owner(MDL_key::TABLE, db, table_name,
                                             MDL_EXCLUSIVE));
  create_info.init();
  build_table_filename(path_buf, sizeof(path_buf) - 1,
                       db, table_name, "", 0);
  /* Attempt to reconstruct the table. */
  DBUG_RETURN(ha_create_table(thd, path_buf, db, table_name, &create_info, NULL, 0));
}

size_t dd_extra2_len(const uchar **pos, const uchar *end)
{
  size_t length= *(*pos)++;
  if (length)
    return length;

  if ((*pos) + 2 >= end)
    return 0;
  length= uint2korr(*pos);
  (*pos)+= 2;
  if (length < 256 || *pos + length > end)
    return 0;
  return length;
}


bool Extra2_info::read(const uchar *frm_image, size_t frm_size)
{
  read_size= uint2korr(frm_image + 4);

  if (frm_size < FRM_HEADER_SIZE + read_size)
    return true;

  const uchar *pos= frm_image + 64;

  DBUG_ENTER("read_extra2");

  if (*pos == '/')   // old frm had '/' there
    DBUG_RETURN(false);

  const uchar *e2end= pos + read_size;
  while (pos + 3 <= e2end)
  {
    extra2_frm_value_type type= (extra2_frm_value_type)*pos++;
    size_t length= dd_extra2_len(&pos, e2end);
    if (!length)
      DBUG_RETURN(true);
    switch (type) {
      case EXTRA2_TABLEDEF_VERSION:
        if (version.str) // see init_from_sql_statement_string()
        {
          if (length != version.length)
            DBUG_RETURN(true);
        }
        else
        {
          version.str= pos;
          version.length= length;
        }
        break;
      case EXTRA2_ENGINE_TABLEOPTS:
        if (options.str)
          DBUG_RETURN(true);
        options.str= pos;
        options.length= length;
        break;
      case EXTRA2_DEFAULT_PART_ENGINE:
        engine.set((const char*)pos, length);
        break;
      case EXTRA2_GIS:
        if (gis.str)
          DBUG_RETURN(true);
        gis.str= pos;
        gis.length= length;
        break;
      case EXTRA2_PERIOD_FOR_SYSTEM_TIME:
        if (system_period.str || length != 2 * frm_fieldno_size)
          DBUG_RETURN(true);
        system_period.str = pos;
        system_period.length= length;
        break;
      case EXTRA2_FIELD_FLAGS:
        if (field_flags.str)
          DBUG_RETURN(true);
        field_flags.str= pos;
        field_flags.length= length;
        break;
      case EXTRA2_APPLICATION_TIME_PERIOD:
        if (application_period.str)
          DBUG_RETURN(true);
        application_period.str= pos;
        application_period.length= length;
        break;
      case EXTRA2_FIELD_DATA_TYPE_INFO:
        if (field_data_type_info.str)
          DBUG_RETURN(true);
        field_data_type_info.str= pos;
        field_data_type_info.length= length;
        break;
      case EXTRA2_FOREIGN_KEY_INFO:
        if (foreign_key_info.str)
          DBUG_RETURN(true);
        foreign_key_info.str= pos;
        foreign_key_info.length= length;
        break;
      default:
        /* abort frm parsing if it's an unknown but important extra2 value */
        if (type >= EXTRA2_ENGINE_IMPORTANT)
          DBUG_RETURN(true);
    }
    pos+= length;
  }
  if (pos != e2end)
    DBUG_RETURN(true);

  DBUG_ASSERT(store_size() == read_size);
  DBUG_RETURN(false);
}

/*
  write the length as
  if (  0 < length <= 255)      one byte
  if (256 < length < 65535)    zero byte, then two bytes, low-endian
*/
uchar *
extra2_write_len(uchar *pos, size_t len)
{
  if (len <= 255)
    *pos++= (uchar)len;
  else
  {
    DBUG_ASSERT(len <= 0xffff - FRM_HEADER_SIZE - 8);
    *pos++= 0;
    int2store(pos, len);
    pos+= 2;
  }
  return pos;
}

uchar *
extra2_write_str(uchar *pos, const LEX_CSTRING &str)
{
  pos= extra2_write_len(pos, str.length);
  memcpy(pos, str.str, str.length);
  return pos + str.length;
}

uchar *
extra2_write_field_properties(uchar *pos, List<Create_field> &create_fields)
{
  List_iterator<Create_field> it(create_fields);
  *pos++= EXTRA2_FIELD_FLAGS;
  /*
   always first 2  for field visibility
  */
  pos= extra2_write_len(pos, create_fields.elements);
  while (Create_field *cf= it++)
  {
    uchar flags= cf->invisible;
    if (cf->flags & VERS_UPDATE_UNVERSIONED_FLAG)
      flags|= VERS_OPTIMIZED_UPDATE;
    *pos++= flags;
  }
  return pos;
}


uchar *
Extra2_info::write(uchar *frm_image, size_t frm_size)
{
  // FIXME: what to do with frm_size here (and in read())?
  uchar *pos;
  /* write the extra2 segment */
  pos = frm_image + FRM_HEADER_SIZE;
  compile_time_assert(EXTRA2_TABLEDEF_VERSION != '/');

  if (version.str)
    pos= extra2_write(pos, EXTRA2_TABLEDEF_VERSION, version);

  if (engine.str)
    pos= extra2_write(pos, EXTRA2_DEFAULT_PART_ENGINE, engine);

  if (options.str)
    pos= extra2_write(pos, EXTRA2_ENGINE_TABLEOPTS, options);

  if (gis.str)
    pos= extra2_write(pos, EXTRA2_GIS, gis);

  if (field_data_type_info.str)
    pos= extra2_write(pos, EXTRA2_FIELD_DATA_TYPE_INFO, field_data_type_info);

  if (foreign_key_info.str)
    pos= extra2_write(pos, EXTRA2_FOREIGN_KEY_INFO, foreign_key_info);

  if (system_period.str)
    pos= extra2_write(pos, EXTRA2_PERIOD_FOR_SYSTEM_TIME, system_period);

  if (application_period.str)
    pos= extra2_write(pos, EXTRA2_APPLICATION_TIME_PERIOD, application_period);

  if (field_flags.str)
    pos= extra2_write(pos, EXTRA2_FIELD_FLAGS, field_flags);

  write_size= pos - frm_image - FRM_HEADER_SIZE;
  DBUG_ASSERT(write_size == store_size());
  DBUG_ASSERT(write_size <= 0xffff - FRM_HEADER_SIZE - 4);

#if 0
  int4store(pos, filepos); // end of the extra2 segment
#endif
  return pos;
}


bool TABLE_SHARE::fk_write_shadow_frm_impl(const char *shadow_path)
{
  const uchar * frm_src;
  uchar * frm_dst;
  uchar * pos;
  size_t frm_size;
  Extra2_info extra2;
  Foreign_key_io foreign_key_io;

  if (read_frm_image(&frm_src, &frm_size))
    return true;

  Scope_malloc frm_src_freer(frm_src); // read_frm_image() passed ownership to us

  if (frm_size < FRM_HEADER_SIZE + FRM_FORMINFO_SIZE)
  {
frm_err:
    char path[FN_REFLEN + 1];
    strxmov(path, normalized_path.str, reg_ext, NullS);
    my_error(ER_NOT_FORM_FILE, MYF(0), path);
    return true;
  }

  if (!is_binary_frm_header(frm_src))
    goto frm_err;

  if (extra2.read(frm_src, frm_size))
  {
    my_printf_error(ER_CANT_CREATE_TABLE,
                    "Cannot create table %`s: "
                    "Read of extra2 section failed.",
                    MYF(0), table_name.str);
    return true;
  }

  const uchar * const rest_src= frm_src + FRM_HEADER_SIZE + extra2.read_size;
  const size_t rest_size= frm_size - FRM_HEADER_SIZE - extra2.read_size;
  size_t forminfo_off= uint4korr(rest_src);

  foreign_key_io.store(foreign_keys, referenced_keys);
  extra2.foreign_key_info= foreign_key_io.lex_custring();
  if (!extra2.foreign_key_info.length)
    extra2.foreign_key_info.str= NULL;
  else if (extra2.foreign_key_info.length > 0xffff - FRM_HEADER_SIZE - 8)
  {
    my_printf_error(ER_CANT_CREATE_TABLE,
                    "Cannot create table %`s: "
                    "Building the foreign key info image failed.",
                    MYF(0), table_name.str);
    return true;
  }

  const size_t extra2_increase= extra2.store_size() - extra2.read_size;
  frm_size+= extra2_increase;

  if (frm_size > FRM_MAX_SIZE)
  {
    my_error(ER_TABLE_DEFINITION_TOO_BIG, MYF(0), table_name.str);
    return true;
  }

  Scope_malloc frm_dst_freer(frm_dst, frm_size, MY_WME);
  if (!frm_dst)
    return true;

  memcpy((void *)frm_dst, (void *)frm_src, FRM_HEADER_SIZE);

  if (!(pos= extra2.write(frm_dst, frm_size)))
  {
    my_printf_error(ER_CANT_CREATE_TABLE,
                    "Cannot create table %`s: "
                    "Write of extra2 section failed.",
                    MYF(0), table_name.str);
    return true;
  }

  forminfo_off+= extra2_increase;
  int4store(pos, forminfo_off);
  pos+= 4;
  int2store(frm_dst + 4, extra2.write_size);
  int2store(frm_dst + 6, FRM_HEADER_SIZE + extra2.write_size + 4); // Position to key information
  int4store(frm_dst + 10, frm_size);
  memcpy((void *)pos, rest_src + 4, rest_size - 4);

  if (writefrm(shadow_path, db.str, table_name.str, false, frm_dst, frm_size))
    return true;

  return false;
}

static void release_log_entries(DDL_LOG_MEMORY_ENTRY *log_entry)
{
  while (log_entry)
  {
    release_ddl_log_memory_entry(log_entry);
    log_entry= log_entry->next_active_log_entry;
  }
}

static bool write_log_replace_delete_frm(uint next_entry,
                                         const char *from_path,
                                         const char *to_path,
                                         bool replace_flag)
{
  DDL_LOG_ENTRY ddl_log_entry;
  DDL_LOG_MEMORY_ENTRY *log_entry;
  DDL_LOG_MEMORY_ENTRY *first_log_entry; // FIXME: ???
  DDL_LOG_MEMORY_ENTRY *exec_log_entry= NULL;

  if (replace_flag)
    ddl_log_entry.action_type= DDL_LOG_REPLACE_ACTION;
  else
    ddl_log_entry.action_type= DDL_LOG_DELETE_ACTION;
  ddl_log_entry.next_entry= next_entry;
  ddl_log_entry.handler_name= reg_ext;
  ddl_log_entry.name= to_path;
  if (replace_flag) // FIXME: remove if not needed
    ddl_log_entry.from_name= from_path;
  if (ERROR_INJECT("fail_log_replace_delete_1", "crash_log_replace_delete_1"))
    return true;
  Mutex_lock lock_gdl(&LOCK_gdl);
  if (write_ddl_log_entry(&ddl_log_entry, &log_entry))
  {
error:
    release_log_entries(log_entry); // FIXME: it was first_log_entry
    my_error(ER_DDL_LOG_ERROR, MYF(0));
    return true;
  }
  if (ERROR_INJECT("fail_log_replace_delete_2", "crash_log_replace_delete_2"))
    goto error;
  if (write_execute_ddl_log_entry(log_entry->entry_pos,
                                    FALSE, &exec_log_entry))
    goto error;
  if (ERROR_INJECT("fail_log_replace_delete_3", "crash_log_replace_delete_3"))
    goto error;
  // FIXME: release on finish
  return false;
}

bool TABLE_SHARE::fk_write_shadow_frm()
{
  char shadow_path[FN_REFLEN + 1];
  build_table_shadow_filename(shadow_path, sizeof(shadow_path) - 1,
                              db, table_name, tmp_fk_prefix);
  if (write_log_replace_delete_frm(0, NULL, shadow_path, false))
    return true;
  bool err= fk_write_shadow_frm_impl(shadow_path);
  if (ERROR_INJECT("fail_fk_write_shadow", "crash_fk_write_shadow"))
    return true;
  return err;
}

bool fk_backup_frm(Table_name table)
{
  MY_STAT stat_info;
  DBUG_ASSERT(0 != strcmp(reg_ext, bak_ext));
  char path[FN_REFLEN + 1];
  char bak_name[FN_REFLEN + 1];
  char frm_name[FN_REFLEN + 1];
  build_table_filename(path, sizeof(path), table.db.str,
                       table.name.str, "", 0);
  strxnmov(frm_name, sizeof(frm_name), path, reg_ext, NullS);
  strxnmov(bak_name, sizeof(bak_name), path, bak_ext, NullS);
  if (mysql_file_stat(key_file_frm, bak_name, &stat_info, MYF(0)))
  {
    my_error(ER_FILE_EXISTS_ERROR, MYF(0), bak_name);
    return true;
  }
  if (mysql_file_rename(key_file_frm, frm_name, bak_name, MYF(MY_WME)))
    return true;
  return false;
}

bool TABLE_SHARE::fk_backup_frm()
{
  return ::fk_backup_frm({db, table_name});
}

bool fk_install_shadow_frm(Table_name old_name, Table_name new_name)
{
  MY_STAT stat_info;
  char shadow_path[FN_REFLEN + 1];
  char path[FN_REFLEN + 1];
  char shadow_frm_name[FN_REFLEN + 1];
  char frm_name[FN_REFLEN + 1];
  build_table_shadow_filename(shadow_path, sizeof(shadow_path) - 1,
                              old_name.db, old_name.name, tmp_fk_prefix);
  build_table_filename(path, sizeof(path), new_name.db.str,
                       new_name.name.str, "", 0);
  strxnmov(shadow_frm_name, sizeof(shadow_frm_name), shadow_path, reg_ext, NullS);
  strxnmov(frm_name, sizeof(frm_name), path, reg_ext, NullS);
  if (!mysql_file_stat(key_file_frm, shadow_frm_name, &stat_info, MYF(MY_WME)))
    return true;
  if (mysql_file_rename(key_file_frm, shadow_frm_name, frm_name, MYF(MY_WME)))
    return true;
  return false;
}

bool TABLE_SHARE::fk_install_shadow_frm()
{
  return ::fk_install_shadow_frm({db, table_name}, {db, table_name});
}

void fk_drop_shadow_frm(Table_name table)
{
  char shadow_path[FN_REFLEN+1];
  char shadow_frm_name[FN_REFLEN+1];
  build_table_shadow_filename(shadow_path, sizeof(shadow_path) - 1,
                              table.db, table.name, tmp_fk_prefix);
  strxnmov(shadow_frm_name, sizeof(shadow_frm_name), shadow_path, reg_ext, NullS);
  mysql_file_delete(key_file_frm, shadow_frm_name, MYF(0));
}

void TABLE_SHARE::fk_drop_shadow_frm()
{
  ::fk_drop_shadow_frm({db, table_name});
}

void fk_drop_backup_frm(Table_name table)
{
  char path[FN_REFLEN + 1];
  char bak_name[FN_REFLEN + 1];
  build_table_filename(path, sizeof(path), table.db.str,
                       table.name.str, "", 0);
  strxnmov(bak_name, sizeof(bak_name), path, bak_ext, NullS);
  mysql_file_delete(key_file_frm, bak_name, MYF(0));
}

void TABLE_SHARE::fk_drop_backup_frm()
{
  ::fk_drop_backup_frm({db, table_name});
}
