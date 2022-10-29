select left(concat(date,"T",time),19) as datetime,size,name from "ls -Q -1 -o --full-time Test/*.csv | sed 's/\\s\\+/,/g' |"(perm,iaa,ib,size,date,time,zone,name,a,b) file;

