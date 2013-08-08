#!/bin/bash

MY_LOGFILE=".my.make.log"

make 2>&1 | tee $MY_LOGFILE 
echo "=========================== Warning ==============================="
cat $MY_LOGFILE | grep  "arning"
rm .my.make.log
