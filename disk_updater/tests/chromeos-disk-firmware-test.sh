#!/bin/bash

# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Chrome OS Disk Firmware Update Script test harness
# Use bash to be able to source the script to test with parameters and
# use array:
# !bash -x ./scripts/chromeos-disk-firmware-test.sh > /tmp/test 2>&1
#
# Works only in the chromium chroot.

export LC_ALL=C
DISK_TEMP=$(mktemp -d)


# source the script to test: The test script is in bash to
# allow paramters to be taken into account.
. scripts/chromeos-disk-firmware-update.sh \
  --tmp_dir "${DISK_TEMP}" \
  --fw_package_dir "tests/test_fw_dir" \
  --hdparm ':' \
  --test

# Overwrite funtions that call hdparm
# Read the identify for files
declare -a hdparm_files
declare -a hdparm_rc
declare -i id_idx
declare -i hdparm_fw_rc=0

unset disk_get_sata_devices
disk_get_sata_devices() {
  echo "sda"
}

unset disk_hdparm_info
disk_hdparm_info() {
  local rc=${hdparm_rc[${id_idx}]}
  if [ ${rc} -eq 0 ]; then
    cat "tests/${hdparm_files[${id_idx}]}.hdparm"
  fi
  : $(( id_idx += 1))
  return ${rc}
}

prepare_test() {
  id_idx=0
  find "${DISK_TEMP}" -mindepth 1 -delete
}

run_test() {
  main > "${DISK_TEMP}/result"
}

check_test() {
  local test_id=$1
  local exp_result=$2_${test_id}
  local test_exp_rc=$3
  local test_rc=$4
  if [ ${test_exp_rc} -ne ${test_rc} ]; then
    echo "Expected ${test_exp_rc}, got ${test_rc}"
    exit 1
  fi
  diff "${DISK_TEMP}/result" "tests/${exp_result}"
  if [ $? -ne 0 ]; then
    echo "test_${test_id} failed"
    exit 1
  fi
}

# Test 1: Good update
# A disk that matches the upgrade file
prepare_test
hdparm_files=(
  'LITEONIT_LSS_32L6G_HP-DS51702'
  ''
  ''
  'LITEONIT_LSS_32L6G_HP-DS51704'
  'LITEONIT_LSS_32L6G_HP-DS51704'
)
hdparm_rc=(0 10 10 0 0)

run_test
check_test 1 disk_upgraded 0 $?
echo PASS 1

# Test 2: Disk is not part of the upgrade
prepare_test
hdparm_files=( 'LITEONIT_LSS_16L6G_HP-DS41702')
hdparm_rc=(0)

run_test
check_test 2 disk_good 0 $?
echo PASS 2

# Test 3: Disk is not part of the upgrade
prepare_test
hdparm_files=('LITEONIT_LSS_32L6G_HP-DS51704')
hdparm_rc=(0)

run_test
check_test 3 disk_good 0 $?
echo PASS 3

# Test 4: Disk not supported by hdparm
prepare_test
hdparm_files=()
hdparm_rc=(10)

run_test
check_test 4 disk_absent 10 $?
echo PASS 4

# Test 5: Invalid package, a file missing
prepare_test
hdparm_files=('SAMSUNG_MZAPF032HCFV-000H1')
hdparm_rc=(0)

run_test
check_test 5 file_missing 1 $?
echo PASS 5


rm -rf "${DISK_TEMP}"
