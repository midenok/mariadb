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

extern my_bool sys_ver_forced;
extern my_bool sys_ver_hidden;

static MYSQL_SYSVAR_BOOL(forced,
       sys_ver_forced,
       PLUGIN_VAR_NOCMDARG,
       "Every newly created table will be system versioned",
       NULL, NULL, TRUE);

static MYSQL_SYSVAR_BOOL(hidden,
       sys_ver_hidden,
       PLUGIN_VAR_NOCMDARG,
       "SHOW and DESCRIBE commands will hide fileds related to system versioning",
       NULL, NULL, TRUE);

static struct st_mysql_daemon default_versioning_info_descriptor=
{ MYSQL_DAEMON_INTERFACE_VERSION };

static int default_versioning_init(void *p __attribute__((unused)))
{
  DBUG_ENTER("default_versioning_plugin_init");
  DBUG_RETURN(0);
}
static int default_versioning_deinit(void *arg __attribute__((unused)))
{
  DBUG_ENTER("default_versioning_plugin_deinit");
  sys_ver_forced = FALSE;
  sys_ver_hidden = FALSE;
  DBUG_RETURN(0);
}

static struct st_mysql_sys_var *default_versioning_vars[]=
{
  MYSQL_SYSVAR(forced),
  MYSQL_SYSVAR(hidden),
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
