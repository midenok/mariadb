#ifndef VERS_UTILS_INCLUDED
#define VERS_UTILS_INCLUDED

#include "table.h"
#include "sql_class.h"
#include "vers_string.h"

class MDL_auto_lock
{
  THD *thd;
  TABLE_LIST &table;
  bool error;

public:
  MDL_auto_lock(THD *_thd, TABLE_LIST &_table) :
    thd(_thd), table(_table)
  {
    DBUG_ASSERT(thd);
    table.mdl_request.init(MDL_key::TABLE, table.db, table.table_name, MDL_EXCLUSIVE, MDL_EXPLICIT);
    error= thd->mdl_context.acquire_lock(&table.mdl_request, thd->variables.lock_wait_timeout);
  }
  ~MDL_auto_lock()
  {
    if (!error)
    {
      DBUG_ASSERT(table.mdl_request.ticket);
      thd->mdl_context.release_lock(table.mdl_request.ticket);
      table.mdl_request.ticket= NULL;
    }
  }
  bool acquire_error() const { return error; }
};


class Open_tables_auto_backup
{
  THD *thd;
  Open_tables_backup backup;

public:
  Open_tables_auto_backup(THD *_thd) :
    thd(_thd)
  {
    thd->reset_n_backup_open_tables_state(&backup);
  }
  ~Open_tables_auto_backup()
  {
    thd->restore_backup_open_tables_state(&backup);
  }
};


class Local_da : public Diagnostics_area
{
  uint sql_error;
  Diagnostics_area *saved_da;

public:
  THD *thd;

  Local_da(THD *_thd, uint _sql_error= 0) :
    Diagnostics_area(_thd->query_id, false, true),
    sql_error(_sql_error),
    saved_da(_thd->get_stmt_da()),
    thd(_thd)
  {
    thd->set_stmt_da(this);
  }
  ~Local_da()
  {
    if (saved_da)
      finish();
  }
  void finish()
  {
    DBUG_ASSERT(saved_da && thd);
    thd->set_stmt_da(saved_da);
    if (is_error())
      my_error(sql_error ? sql_error : sql_errno(), MYF(0), message());
    if (warn_count() > error_count())
      saved_da->copy_non_errors_from_wi(thd, get_warning_info());
    saved_da= NULL;
  }
};

class key_buf_t
{
  uchar* buf;

  key_buf_t(const key_buf_t&); // disabled
  key_buf_t& operator= (const key_buf_t&); // disabled

public:
  key_buf_t() : buf(NULL)
  {}

  ~key_buf_t()
  {
    if (buf)
      my_free(buf);
  }

  bool allocate(size_t alloc_size)
  {
    DBUG_ASSERT(!buf);
    buf= static_cast<uchar *>(my_malloc(alloc_size, MYF(0)));
    if (!buf)
    {
      my_message(ER_VERS_VTMD_ERROR, "failed to allocate key buffer", MYF(0));
      return true;
    }
    return false;
  }

  operator uchar* ()
  {
    DBUG_ASSERT(buf);
    return reinterpret_cast<uchar *>(buf);
  }
};

template <typename T>
struct Pair
{
  T first;
  T second;
  Pair(T a, T b) : first(a), second(b) {}
};

typedef Pair<int> IntPair;

#endif // VERS_UTILS_INCLUDED
