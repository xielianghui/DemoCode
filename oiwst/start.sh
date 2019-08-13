#!/bin/bash

dbhost="192.168.0.204"
dbport="1433"
username="sa"
password="intel.com.123"
dbname="oiw25691"

make
nohup sudo ./oiwst -H $dbhost -p $dbport -U $username -P $password -D $dbname > error.log &
#自动回车
sleep 1
#enter=$"\n"
#senter=$(echo -e $enter)
#echo $senter
echo "start success!"

