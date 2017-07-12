#!/bin/bash
# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

DISK_TEMP_TEMPLATE=chromeos-disk-metrics.XXXXXX
DISK_TEMP=$(mktemp -d --tmpdir "${DISK_TEMP_TEMPLATE}")

. chromeos-disk-metrics --test

declare -i id

# Mock output command
metrics_client() {
  echo "metrics_client: $*"
}

run_test() {
  local name=$1
  local out="${DISK_TEMP}/${name}.out"
  local exp_result
  id=$2
  exp_result="tests/${name}_${id}.golden"
  ${name} > "${out}"
  diff "${out}" "${exp_result}"
  if [ $? -ne 0 ]; then
    echo "test_${name} failed"
    exit 1
  fi
}

STORAGE_INFO_FILE="tests/storage_info_1.txt"
run_test sata_disk_metrics 1
run_test emmc_disk_metrics 1

STORAGE_INFO_FILE="tests/storage_info_2.txt"
run_test sata_disk_metrics 2
run_test emmc_disk_metrics 2

rm -rf "${DISK_TEMP}"
