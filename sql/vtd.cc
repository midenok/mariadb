#include "vtd.h"
#include "sql_base.h"
#include "sql_class.h"

bool VTD_table::write_row(THD *thd)
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

  /* check that all columns exist */
  if (table->s->fields < 6)
    goto err;

  table->field[TRX_ID_START]->store((longlong) 0, TRUE);
  table->field[TRX_ID_START]->set_notnull();
  table->field[TRX_ID_END]->store((longlong) 1, TRUE);
  table->field[TRX_ID_END]->set_notnull();
  table->field[OLD_NAME]->set_null();
  table->field[NAME]->store(STRING_WITH_LEN("name"), system_charset_info);
  table->field[NAME]->set_notnull();
  table->field[FRM_IMAGE]->set_null();
  table->field[COL_RENAMES]->set_null();

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
