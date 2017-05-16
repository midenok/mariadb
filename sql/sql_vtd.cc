#include "sql_vtd.h"
#include "sql_base.h"
#include "sql_class.h"

void VTD_table::init(THD *thd)
{
  thr_lock_type lock_type= TL_WRITE;
  /* We are relying on init_one_table zeroing out the TABLE_LIST structure. */
  tl.init_one_table(MYSQL_SCHEMA_NAME.str, MYSQL_SCHEMA_NAME.length,
                    C_STRING_WITH_LEN("user"),
                    NULL, lock_type);
  tl.open_type= OT_BASE_ONLY;
  if (lock_type >= TL_WRITE_ALLOW_WRITE)
    tl.updating= 1;
}

bool VTD_table::write_as_log(THD *thd)
{
  TABLE_LIST table_list;
  TABLE *table;
  bool result= TRUE;
  bool need_close= FALSE;
  bool need_rnd_end= FALSE;
  Open_tables_backup open_tables_backup;
  ulonglong save_thd_options;


  save_thd_options= thd->variables.option_bits;
  thd->variables.option_bits&= ~OPTION_BIN_LOG;

  table_list.init_one_table(MYSQL_SCHEMA_NAME.str, MYSQL_SCHEMA_NAME.length,
                            VERS_VTD_NAME.str, VERS_VTD_NAME.length,
                            VERS_VTD_NAME.str,
                            TL_WRITE_CONCURRENT_INSERT);

  if (!(table= open_log_table(thd, &table_list, &open_tables_backup)))
    goto err;

  need_close= TRUE;

  if (table->file->extra(HA_EXTRA_MARK_AS_LOG_TABLE) ||
      table->file->ha_rnd_init_with_error(0))
    goto err;

  need_rnd_end= TRUE;

  /* Honor next number columns if present */
  table->next_number_field= table->found_next_number_field;

  if (table->file->ha_write_row(table->record[0]))
    goto err;

  result= FALSE;

err:
  if (need_rnd_end)
  {
    table->file->ha_rnd_end();
    table->file->ha_release_auto_increment();
  }

  if (need_close)
    close_log_table(thd, &open_tables_backup);

  thd->variables.option_bits= save_thd_options;
  return result;
}

int VTD_table::write_row()
{
  int error;
  TABLE *table= tl.table;
  DBUG_ASSERT(table);
  table->use_all_columns();
  restore_record(table, s->default_values);
  if ((error= table->file->ha_write_row(table->record[0])))
  {
    DBUG_PRINT("info", ("error inserting row"));
    goto table_error;
  }

  /* all ok */
  return 0;

table_error:
  DBUG_PRINT("info", ("table error"));
  table->file->print_error(error, MYF(0));
  return 1;
}
