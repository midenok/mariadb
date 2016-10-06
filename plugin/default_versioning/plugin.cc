/* Copyright (C) 2013 Percona and Sergey Vojtovich

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#define MYSQL_SERVER
#include <sql_class.h>
#include <table.h>
#include <sql_show.h>
#include <mysql/plugin.h>


static MYSQL_THDVAR_BOOL(enable,
       PLUGIN_VAR_NOCMDOPT,
       "Enables/disables default versioning for all newly "
       "created tables",
       NULL, NULL, FALSE);


static struct st_mysql_daemon default_versioning_info_descriptor=
{ MYSQL_DAEMON_INTERFACE_VERSION };

static my_bool is_enabled(MYSQL_THD thd)
{
  DBUG_ENTER("default_versioning is_enabled");
  const my_bool ret = THDVAR(thd, enable);
  DBUG_RETURN(ret);
}

extern my_bool (*sys_ver_every_new_table)(THD *thd);
static int default_versioning_init(void *p __attribute__((unused)))
{
  DBUG_ENTER("default_versioning_plugin_init");
  DBUG_ASSERT(!sys_ver_every_new_table);
  sys_ver_every_new_table = is_enabled;
  DBUG_RETURN(0);
}
static int default_versioning_deinit(void *arg __attribute__((unused)))
{
  DBUG_ENTER("default_versioning_plugin_deinit");
  // FIXME: XYZ: This is probably not thread safe, since some thread
  //   can be accessing plugin data while plugin is being released.
  sys_ver_every_new_table = NULL;
  DBUG_RETURN(0);
}

static struct st_mysql_sys_var *default_versioning_vars[]=
{
  MYSQL_SYSVAR(enable),
  NULL
};

maria_declare_plugin(default_versioning)
{
  MYSQL_DAEMON_PLUGIN,
  &default_versioning_info_descriptor,
  "Default_Versioning",
  "NatSys Lab.",
  "Default Versioning Plugin",
  PLUGIN_LICENSE_GPL,
  default_versioning_init,
  default_versioning_deinit,
  0x0100,
  NULL,
  default_versioning_vars,
  "1.0",
  MariaDB_PLUGIN_MATURITY_STABLE
}
maria_declare_plugin_end;
