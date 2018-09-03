create or replace table log_tbl(log varchar(4));

create or replace procedure log(s varchar(4))
  insert into log_tbl values(s);
