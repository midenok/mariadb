#ifndef SQL_VTD_INCLUDED
#define SQL_VTD_INCLUDED

#include "table.h"
#include "unireg.h"
#include <mysqld_error.h>

class THD;

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
  int init(THD *thd, enum thr_lock_type lock_type);
  int write_row();

private:
  TABLE_LIST tl;
};

#endif // SQL_VTD_INCLUDED
