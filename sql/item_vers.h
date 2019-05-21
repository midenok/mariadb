#ifndef ITEM_VERS_INCLUDED
#define ITEM_VERS_INCLUDED
/* Copyright (c) 2017, MariaDB Corporation.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA */


/* System Versioning items */

#ifdef USE_PRAGMA_INTERFACE
#pragma interface			/* gcc class implementation */
#endif

class Item_func_trt_ts: public Item_datetimefunc
{
  TR_table::field_id_t trt_field;
public:
  Item_func_trt_ts(THD *thd, Item* a, TR_table::field_id_t _trt_field);
  const char *func_name() const
  {
    if (trt_field == TR_table::FLD_BEGIN_TS)
    {
      return "trt_begin_ts";
    }
    return "trt_commit_ts";
  }
  bool get_date(THD *thd, MYSQL_TIME *res, date_mode_t fuzzydate);
  Item *get_copy(THD *thd)
  { return get_item_copy<Item_func_trt_ts>(thd, this); }
  bool fix_length_and_dec()
  { fix_attributes_datetime(decimals); return FALSE; }
};

class Item_func_trt_id : public Item_longlong_func
{
  TR_table::field_id_t trt_field;
  bool backwards;

  longlong get_by_trx_id(ulonglong trx_id);
  longlong get_by_commit_ts(MYSQL_TIME &commit_ts, bool backwards);

public:
  Item_func_trt_id(THD *thd, Item* a, TR_table::field_id_t _trt_field, bool _backwards= false);
  Item_func_trt_id(THD *thd, Item* a, Item* b, TR_table::field_id_t _trt_field);

  const char *func_name() const
  {
    switch (trt_field)
    {
    case TR_table::FLD_TRX_ID:
      return "trt_trx_id";
    case TR_table::FLD_COMMIT_ID:
        return "trt_commit_id";
    case TR_table::FLD_ISO_LEVEL:
      return "trt_iso_level";
    default:
      DBUG_ASSERT(0);
    }
    return NULL;
  }

  bool fix_length_and_dec()
  {
    bool res= Item_int_func::fix_length_and_dec();
    max_length= 20;
    return res;
  }

  longlong val_int();
  Item *get_copy(THD *thd)
  { return get_item_copy<Item_func_trt_id>(thd, this); }
};

class Item_func_trt_trx_sees : public Item_bool_func
{
protected:
  bool accept_eq;

public:
  Item_func_trt_trx_sees(THD *thd, Item* a, Item* b);
  const char *func_name() const
  {
    return "trt_trx_sees";
  }
  longlong val_int();
  Item *get_copy(THD *thd)
  { return get_item_copy<Item_func_trt_trx_sees>(thd, this); }
};

class Item_func_trt_trx_sees_eq :
  public Item_func_trt_trx_sees
{
public:
  Item_func_trt_trx_sees_eq(THD *thd, Item* a, Item* b) :
    Item_func_trt_trx_sees(thd, a, b)
  {
    accept_eq= true;
  }
  const char *func_name() const
  {
    return "trt_trx_sees_eq";
  }
};


class Item_vers_sys_field_setter : public Item_datetimefunc
{
protected:
  Field *field;

public:
  Item_vers_sys_field_setter(THD *thd): Item_datetimefunc(thd), field(NULL) {}
  virtual bool is_fixed() const { return true; }
  void set_field(Field *in) { field= in; }
  TABLE *table()
  {
    DBUG_ASSERT(field);
    DBUG_ASSERT(field->vers_sys_field());
    DBUG_ASSERT(field->table);
    DBUG_ASSERT(field->table->versioned());
    return field->table;
  }

  const Type_handler *type_handler() const { return &type_handler_timestamp2; }
  int save_in_field(Field *field, bool no_conversions)
  {
    DBUG_ASSERT(0);
    return true;
  }
  longlong val_int()
  {
    DBUG_ASSERT(0);
    return 0;
  }
  double val_real()
  {
    DBUG_ASSERT(0);
    return 0;
  }
  String *val_str(String *to)
  {
    DBUG_ASSERT(0);
    return NULL;
  }
  my_decimal *val_decimal(my_decimal *to)
  {
    DBUG_ASSERT(0);
    return NULL;
  }
  bool get_date(THD *thd, MYSQL_TIME *ltime, date_mode_t fuzzydate)
  {
    DBUG_ASSERT(0);
    return true;
  }
  bool val_native(THD *thd, Native *to)
  {
    DBUG_ASSERT(0);
    return true;
  }
  void set_value(const Timestamp_or_zero_datetime &value)
  {
    DBUG_ASSERT(0);
  }
};

class Item_vers_row_start_setter: public Item_vers_sys_field_setter
{
public:
  Item_vers_row_start_setter(THD *thd): Item_vers_sys_field_setter(thd) {}
  const char *func_name() const
  { return "vers_row_start_setter"; }
  bool get_date(THD *thd, MYSQL_TIME *res, date_mode_t fuzzydate);
  Item *get_copy(THD *thd)
  { return get_item_copy<Item_vers_row_start_setter>(thd, this); }
  bool fix_length_and_dec()
  { fix_attributes_datetime(decimals); return FALSE; }
  int save_in_field(Field *field, bool no_conversions);
};

class Item_vers_row_end_setter: public Item_vers_sys_field_setter
{
public:
  Item_vers_row_end_setter(THD *thd): Item_vers_sys_field_setter(thd) {}
  const char *func_name() const
  { return "vers_row_end_setter"; }
  bool get_date(THD *thd, MYSQL_TIME *res, date_mode_t fuzzydate);
  Item *get_copy(THD *thd)
  { return get_item_copy<Item_vers_row_end_setter>(thd, this); }
  bool fix_length_and_dec()
  { fix_attributes_datetime(decimals); return FALSE; }
  int save_in_field(Field *field, bool no_conversions);
};

#endif /* ITEM_VERS_INCLUDED */
