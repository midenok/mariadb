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
#include "sql_class.h"
#include "sql_table.h"
#include "ha_sequence.h"

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


bool TABLE_SHARE::dd_check_frm()
{
  const uchar* frm_image;
  size_t frm_length;
  Extra2_info extra2;

  if (read_frm_image(&frm_image, &frm_length))
    return true;

  Scope_alloc free_frm_image((void *)frm_image);

  if (frm_length < FRM_HEADER_SIZE + FRM_FORMINFO_SIZE)
    return true;

  if (!is_binary_frm_header(frm_image))
    return true;

  uint extra2_len= uint2korr(frm_image + 4);

  if (frm_length < FRM_HEADER_SIZE + extra2_len)
    return true;

  if (extra2.read(frm_image, extra2_len))
    return true;

  size_t len= extra2.length();

  return false;
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


bool Extra2_info::read(const uchar *frm_image, size_t extra2_length)
{
  const uchar *pos= frm_image + 64;

  DBUG_ENTER("read_extra2");

  reset();

  if (*pos != '/')   // old frm had '/' there
  {
    const uchar *e2end= pos + extra2_length;
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
  }
  DBUG_RETURN(false);
}

