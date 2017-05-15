#include "table.h"
#include <mysqld_error.h>

class VTD_table
{
 public:
  /* Check if the table exists after an attempt to open it was made.
     Some tables, such as the host table in MySQL 5.6.7+ are missing. */
  bool table_exists() const { return tl.table; };

  /* Return the underlying TABLE handle. */
  TABLE* table() const
  {
    return tl.table;
  }

  /** Check if the table was opened, issue an error otherwise. */
  int no_such_table() const
  {
    if (table_exists())
      return 0;

    my_error(ER_NO_SUCH_TABLE, MYF(0), tl.db, tl.alias);
    return 1;
  }

  VTD_table()
  {
    bzero(&tl, sizeof(tl));
  };

  /* Initialization sequence. This should be called
     after all table-specific initialization is performed. */
  void init(enum thr_lock_type lock_type, bool is_optional)
  {
    /* We are relying on init_one_table zeroing out the TABLE_LIST structure. */
    tl.init_one_table(C_STRING_WITH_LEN("mysql"),
                      C_STRING_WITH_LEN("user"),
                      NULL, lock_type);
    tl.open_type= OT_BASE_ONLY;
    if (lock_type >= TL_WRITE_ALLOW_WRITE)
      tl.updating= 1;
    if (is_optional)
      tl.open_strategy= TABLE_LIST::OPEN_IF_EXISTS;
  }

private:
  TABLE_LIST tl;
};

