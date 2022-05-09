# Include file to decrease test code duplication

let $keep_include_silent= 1;
let $grep_script= ALTER;
let $binlog_file=master-bin.000001;

--eval $create_statement
--eval $insert_statement
--let $dbug_flag= `select @@session.debug_dbug`
--echo # $dbug_flag: BEFORE crash

--list_files_write_file $DATADIR.files.txt $DATADIR/test
--replace_result #p# #P# #sp# #SP#
--cat_file $DATADIR.files.txt
--remove_file $DATADIR.files.txt
SHOW CREATE TABLE t1;
--sorted_result
SELECT * FROM t1;
--exec echo "wait" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
--disable_reconnect
# CR_SERVER_LOST
--error 2013
--eval $crash_statement

--echo # $dbug_flag: AFTER crash
--list_files_write_file $DATADIR.files.txt $DATADIR/test
--replace_result #p# #P# #sp# #SP# #tmp# #TMP#
--replace_regex /sql-exchange.*\./sql-exchange./ /sql-shadow-[0-9a-f]*-/sql-shadow-/
--cat_file $DATADIR.files.txt
--remove_file $DATADIR.files.txt
--exec echo "restart" > $MYSQLTEST_VARDIR/tmp/mysqld.1.expect
--enable_reconnect
--source include/wait_until_connected_again.inc

--echo # $dbug_flag: AFTER recovery
--list_files_write_file $DATADIR.files.txt $DATADIR/test
--replace_result #p# #P# #sp# #SP#
--cat_file $DATADIR.files.txt
--remove_file $DATADIR.files.txt
SHOW CREATE TABLE t1;
--sorted_result
SELECT * FROM t1;
--source include/show_binlog_events.inc
DROP TABLE t1;
