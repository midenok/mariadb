#ifndef VTMD_INCLUDED
#define VTMD_INCLUDED

#include "table.h"
#include "unireg.h"
#include <mysqld_error.h>
#include "my_sys.h"

#include "vers_utils.h"

class THD;

typedef Dynamic_array<IntPair> CMMD_map;

struct MD_naming
{
  LString templ;
  LString suffix;
  MD_naming(LString _templ, LString _suffix) :
    templ(_templ),
    suffix(_suffix) {}
};

template <const MD_naming &names>
class MD_table
{
protected:
  Open_tables_backup ot_backup_;
  TABLE_LIST md_tl_;
  TABLE_LIST &about_tl_;
  SString_t md_table_name_;

private:
  MD_table(const MD_table&); // prohibit copying references

public:
  MD_table(TABLE_LIST &about) :
    about_tl_(about)
  {
    md_tl_.table= NULL;
  }

  bool create(THD *thd);
  bool open(Local_da &local_da, bool *created= NULL);
  bool add_suffix(TABLE_LIST &arg, SString_t &result)
  {
    return arg.vers_add_suffix(names.suffix, result);
  }
  bool update_table_name()
  {
    return add_suffix(about_tl_, md_table_name_);
  }
};

struct VTMD_update_args
{
  const char* archive_name;
  CMMD_map* cmmd;
  VTMD_update_args(const char* _archive_name= NULL, CMMD_map* _colmap= NULL) :
    archive_name(_archive_name),
    cmmd(_colmap) {}
  bool has_cmmd()
  {
    return cmmd ? cmmd->elements() > 0 : false;
  }
};

extern const MD_naming VERS_VTMD_NAMING;
extern const MD_naming VERS_CMMD_NAMING;

class VTMD_table : public MD_table<VERS_VTMD_NAMING>
{
public:
  enum {
    FLD_START= 0,
    FLD_END,
    FLD_NAME,
    FLD_ARCHIVE_NAME,
    FLD_CMMD,
    FIELD_COUNT
  };

  enum {
    IDX_TRX_END= 0,
    IDX_ARCHIVE_NAME
  };

  VTMD_table(TABLE_LIST &about) :
    MD_table(about)
  {}

  bool find_record(ulonglong sys_trx_end, bool &found);
  bool update(THD *thd, VTMD_update_args args= VTMD_update_args());
  bool setup_select(THD *thd);

  static void archive_name(THD *thd, const char *table_name, char *new_name, size_t new_name_size);
  void archive_name(THD *thd, char *new_name, size_t new_name_size) const
  {
    archive_name(thd, about_tl_.table_name, new_name, new_name_size);
  }

  bool find_archive_name(THD *thd, String &out);
  static bool get_archive_tables(THD *thd, const char *db, size_t db_length,
                                 Dynamic_array<String> &result);
};

class VTMD_exists : public VTMD_table
{
protected:
  handlerton *hton;

public:
  bool exists;

public:
  VTMD_exists(TABLE_LIST &_about) :
    VTMD_table(_about),
    hton(NULL),
    exists(false)
  {}

  bool check_exists(THD *thd); // returns error status
};

class VTMD_rename : public VTMD_exists
{
  SString_t vtmd_new_name;

public:
  VTMD_rename(TABLE_LIST &_about) :
    VTMD_exists(_about)
  {}

  bool try_rename(THD *thd, LString new_db, LString new_alias,
                  VTMD_update_args args= VTMD_update_args());
  bool revert_rename(THD *thd, LString new_db);

private:
  bool move_archives(THD *thd, LString &new_db);
  bool move_table(THD *thd, SString_fs &table_name, LString &new_db);
};

class VTMD_drop : public VTMD_exists
{
  char archive_name_[NAME_CHAR_LEN];

public:
  VTMD_drop(TABLE_LIST &_about) :
    VTMD_exists(_about)
  {
    *archive_name_= 0;
  }

  const char* archive_name(THD *thd)
  {
    VTMD_table::archive_name(thd, archive_name_, sizeof(archive_name_));
    return archive_name_;
  }

  const char* archive_name() const
  {
    DBUG_ASSERT(*archive_name_);
    return archive_name_;
  }

  bool update(THD *thd)
  {
    DBUG_ASSERT(*archive_name_);
    return VTMD_exists::update(thd, archive_name_);
  }
};


class CMMD_table : public MD_table<VERS_CMMD_NAMING>
{
public:
  enum {
    FLD_START= 0,
    FLD_CURRENT,
    FLD_LATER,
    FIELD_COUNT
  };

  CMMD_table(TABLE_LIST &about) :
    MD_table(about)
  {}

  bool update(Local_da &local_da, ulonglong sys_trx_start, CMMD_map &cmmd);
};


inline
bool
VTMD_exists::check_exists(THD *thd)
{
  if (update_table_name())
    return true;

  exists= ha_table_exists(thd, about_tl_.db, md_table_name_, &hton);

  if (exists && !hton)
  {
    my_printf_error(ER_VERS_VTMD_ERROR, "`%s.%s` handlerton empty!", MYF(0),
                        about_tl_.db, md_table_name_.ptr());
    return true;
  }
  return false;
}

#endif // VTMD_INCLUDED
