# To be used with partition mgm commands like
# ALTER TABLE t1 COALESCE/REBUILD/REORGANIZE PARTITION.
--echo # Error recovery change partition (REORGANIZE/REBUILD/COALESCE
--echo # or ADD HASH PARTITION).
SET @save_dbug=@@debug_dbug;
SET SESSION debug_dbug="+d,fail_change_partition_7";
--source suite/parts/inc/partition_fail.inc
SET SESSION debug_dbug=@save_dbug;
