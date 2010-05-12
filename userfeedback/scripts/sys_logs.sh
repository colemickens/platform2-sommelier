#!/bin/sh
#
# Copyright 2010 Google Inc. All Rights Reserved.
# Author: rkc@google.com (Rahul Chaturvedi)

if ! [ -e "${1}" ]
then
  log_utils_file="../etc/sys_log_utils.lst"
else
  log_utils_file=${1}
fi

# Do not exit in case a script wasn't found
set +e
for i in `cat ${log_utils_file}`
do
  log=`${i} 2>/dev/null`
  if [ $? -eq 0 ]
  then
    echo "${i}"='"""'"${log}"'"""'
  fi
done
set -e

