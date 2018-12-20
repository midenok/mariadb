/*
   Copyright (c) 2000, 2016, Oracle and/or its affiliates.
   Copyright (c) 2009, 2016, MariaDB

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

/* File that includes common types used globally in MariaDB */

#ifndef SQL_TYPES_INCLUDED
#define SQL_TYPES_INCLUDED

typedef ulonglong sql_mode_t;
typedef int64 query_id_t;


/*
  Use FRAC_NONE when the value needs no rounding nor truncation,
  because it is already known not to haveany fractional digits outside
  of the requested precision.
*/

enum date_modes {
  TIME_CONV_NONE              = 0U,
  TIME_FRAC_NONE              = 0U,
  TIME_FUZZY_DATES            = 1U,
  TIME_TIME_ONLY              = 4U,
  TIME_INTERVAL_hhmmssff      = 8U,
  TIME_INTERVAL_DAY           = 16U,
  TIME_FRAC_TRUNCATE          = 32U,
  TIME_FRAC_ROUND             = 64U,
  TIME_NO_ZERO_IN_DATE        = 1UL << 23,
  TIME_NO_ZERO_DATE           = 1UL << 24,
  TIME_INVALID_DATES          = 1UL << 25
};


static constexpr ulonglong
  RANGE0_LAST                 (TIME_INTERVAL_DAY);

// An often used combination
static constexpr ulonglong
  TIME_NO_ZEROS               (TIME_NO_ZERO_IN_DATE|TIME_NO_ZERO_DATE);

// Flags understood by str_to_xxx, number_to_xxx, check_date
static constexpr ulonglong
  TIME_MODE_FOR_XXX_TO_DATE   (TIME_NO_ZERO_IN_DATE |
                               TIME_NO_ZERO_DATE    |
                               TIME_INVALID_DATES);

/*
  "fuzzydate" with strict data type control.
  Used as a parameter to get_date() and represents a mixture of:
  - data type conversion flags
  - fractional second rounding flags
  Please keep "explicit" in constructors and conversion methods.
*/
class date_mode_t
{
protected:
  friend class date_conv_mode_t;
  friend class time_round_mode_t;
  date_modes m_mode;

public:

  // Constructors
  date_mode_t(ulonglong fuzzydate)
   :m_mode(date_modes(fuzzydate))
  { }

  // Conversion operators
  explicit operator ulonglong() const
  {
    return m_mode;
  }
  explicit operator bool() const
  {
    return m_mode != 0;
  }
  // Unary operators
  ulonglong operator~() const
  {
    return ~m_mode;
  }
  bool operator!() const
  {
    return !m_mode;
  }

  // Dyadic bitwise operators
  date_mode_t operator&(const date_mode_t other) const
  {
    return date_mode_t(m_mode & other.m_mode);
  }

  date_mode_t operator|(const date_mode_t other) const
  {
    return date_mode_t(m_mode | other.m_mode);
  }

  bool operator==(const date_mode_t other) const
  {
    return m_mode == other.m_mode;
  }

  // Dyadic bitwise assignment operators
  date_mode_t &operator&=(const date_mode_t other)
  {
    m_mode= date_modes(m_mode & other.m_mode);
    return *this;
  }

  date_mode_t &operator|=(const date_mode_t other)
  {
    m_mode= date_modes(m_mode | other.m_mode);
    return *this;
  }
};


/*
  "fuzzydate" with strict data type control.
  Represents a mixture of *only* data type conversion flags, without rounding.
  Please keep "explicit" in constructors and conversion methods.
*/
class date_conv_mode_t : public date_mode_t
{
public:

  /*
    BIT-OR for all known values. Let's have a separate enum for it.
    - We don't put this value "value_t", to avoid handling it in switch().
    - We don't put this value as a static const inside the class,
      because "gdb" would display it every time when we do "print"
      for a time_round_mode_t value.
    - We can't put into into a function returning this value, because
      it's not allowed to use functions in static_assert.
  */
  static constexpr ulonglong KNOWN_MODES= TIME_FUZZY_DATES |
                       TIME_TIME_ONLY | TIME_INTERVAL_hhmmssff | TIME_INTERVAL_DAY |
                       TIME_NO_ZERO_IN_DATE | TIME_NO_ZERO_DATE | TIME_INVALID_DATES;
public:
  // Constructors
  date_conv_mode_t(ulonglong fuzzydate)
   : date_mode_t(fuzzydate)
  { }


  date_conv_mode_t(date_mode_t src)
    : date_mode_t(src.m_mode & KNOWN_MODES) {}
};


/*
  Fractional rounding mode for temporal data types.
*/
class time_round_mode_t : public date_mode_t
{
public:
  // BIT-OR for all known values. See comments in time_conv_mode_t.
  static constexpr ulonglong KNOWN_MODES= TIME_FRAC_TRUNCATE | TIME_FRAC_ROUND;

public:
  // Constructors
  time_round_mode_t(ulonglong mode)
   :date_mode_t(mode)
  {
    DBUG_ASSERT(m_mode == TIME_FRAC_NONE ||
                m_mode == TIME_FRAC_TRUNCATE ||
                m_mode == TIME_FRAC_ROUND);
  }
  time_round_mode_t(date_mode_t src)
    :date_mode_t(src.m_mode & KNOWN_MODES) {}

  ulonglong mode() const
  {
    return m_mode;
  }
};

inline
date_mode_t operator| (ulonglong mode, date_mode_t b)
{
  return date_mode_t(mode) | b;
}

inline
date_mode_t operator& (ulonglong mode, date_mode_t b)
{
  return date_mode_t(mode) & b;
}

#endif
