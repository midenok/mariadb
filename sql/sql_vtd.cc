#include "sql_vtd.h"
#include "sql_base.h"
#include "sql_class.h"

int VTD_table::init(THD *thd, thr_lock_type lock_type)
{
  /* We are relying on init_one_table zeroing out the TABLE_LIST structure. */
  tl.init_one_table(C_STRING_WITH_LEN("mysql"),
                    C_STRING_WITH_LEN("innodb_vtd"),
                    NULL, lock_type);
  tl.open_type= OT_BASE_ONLY;
  if (lock_type >= TL_WRITE_ALLOW_WRITE)
    tl.updating= 1;
  if (open_and_lock_tables(thd, &tl, TRUE, 0))
    return 1;
  return 0;
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
