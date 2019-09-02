#!/bin/bash

meta_gen_path=$1
meta_files="find $meta_gen_path -maxdepth 4 \( -name \
'config_request_metadata_*.h' \) -and -not -name '.*'"
file_list=$(eval "$meta_files")

cust_version=rev:0.3.0
final_file=$2"/custgen.config_request.h"

echo "$final_file"
echo '//this file is auto-generated; do not modify it!' > "$final_file"
echo '#define MY_CUST_VERSION "'$cust_version'"' >> "$final_file"
echo '#define MY_CUST_FTABLE_FILE_LIST "'$file_list'"' >> "$final_file"
echo '#define MY_CUST_FTABLE_FINAL_FILE "'$final_file'"' >> "$final_file"
for x in $file_list; do echo "$x" | awk -F/ '{print "#include<"$(NF-4)\
 "/" $(NF-3) "/" $(NF-2) "/" $(NF-1) "/" $NF ">";}' >> "$final_file"; done
