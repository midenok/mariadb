#ifndef SQL_ALLOC_INCLUDED
#define SQL_ALLOC_INCLUDED
/* Copyright (c) 2000, 2012, Oracle and/or its affiliates.
   Copyright (c) 2017, 2018, MariaDB Corporation.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <my_sys.h>                    /* alloc_root, MEM_ROOT, TRASH */

THD *thd_get_current_thd();

/* mysql standard class memory allocator */

class Sql_alloc
{
public:
  static void *operator new(size_t size) throw ()
  {
    return thd_alloc(thd_get_current_thd(), size);
  }
  static void *operator new[](size_t size) throw ()
  {
    return thd_alloc(thd_get_current_thd(), size);
  }
  static void *operator new[](size_t size, MEM_ROOT *mem_root) throw ()
  { return alloc_root(mem_root, size); }
  static void *operator new(size_t size, MEM_ROOT *mem_root) throw()
  { return alloc_root(mem_root, size); }
  static void operator delete(void *ptr, size_t size) { TRASH_FREE(ptr, size); }
  static void operator delete(void *, MEM_ROOT *){}
  static void operator delete[](void *, MEM_ROOT *)
  { /* never called */ }
  static void operator delete[](void *ptr, size_t size) { TRASH_FREE(ptr, size); }
#ifdef HAVE_valgrind
  bool dummy_for_valgrind;
  inline Sql_alloc() :dummy_for_valgrind(0) {}
#else
  inline Sql_alloc() {}
#endif
  inline ~Sql_alloc() {}
};

template <class T> struct Mem_root_allocator
{
  typedef T value_type;
  Mem_root_allocator(MEM_ROOT &mem_root) : mem_root(mem_root){};
  template <class U> Mem_root_allocator(const Mem_root_allocator<U> &) {}
  T *allocate(std::size_t n) { return alloc_root(&mem_root, n); }
  void deallocate(T *p, std::size_t) { dealloc_root(p); }
  const MEM_ROOT &get_mem_root() const { return mem_root; }

private:
  MEM_ROOT &mem_root;
};
template <class T, class U>
bool operator==(const Mem_root_allocator<T> &lhs,
                const Mem_root_allocator<U> &rhs)
{
  return &lhs.mem_root == &rhs.mem_root;
}
template <class T, class U>
bool operator!=(const Mem_root_allocator<T> &lhs,
                const Mem_root_allocator<U> &rhs)
{
  return &lhs.mem_root != &rhs.mem_root;
}
#endif /* SQL_ALLOC_INCLUDED */
