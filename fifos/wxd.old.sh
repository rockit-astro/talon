#!/bin/sh
time=`date "+%H:%M:%S %d/%m/%Y"`
#time=`datetojd ${now} | cut -f 2,2 -d ":" | cut -f 2,2 -d " "`
result=`telnet rebei.oadm.cat 6666 2>&1` 

windspeed=`echo ${result} | cut -f 17,17 -d " " | cut -f 3,3 -d ":" | gawk '{printf "%d",$0}'`
winddir=`echo ${result} | cut -f 18,18 -d " "| cut -f 3,3 -d ":" | gawk '{printf "%d",$0}'`
temp=`echo ${result} |  cut -f 13,13 -d " " | cut -f 3,3 -d ":"`
humidity=`echo ${result} | cut -f 16,16 -d " "| cut -f 3,3 -d ":"`
pressure=`echo ${result} | cut -f 19,19 -d " " | cut -f 3,3 -d ":"`
rain=`echo ${result} | cut -f 22,22 -d " " | cut -f 3,3 -d ":" | gawk '{if($0=="FALSE")print "0";if($0=="TRUE")print "1"}'`

echo "${time} ${windspeed} ${winddir} ${temp} ${humidity} ${pressure} ${rain} -----"

