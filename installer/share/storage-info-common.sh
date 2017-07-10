#!/bin/sh

# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# This script provides various data about the internal disk to show on the
# chrome://system page. It will run once on startup to dump output to the file
# /var/log/storage_info.txt which will be read later by debugd when the user
# opens the chrome://system page.
. /usr/share/misc/chromeos-common.sh

SSD_CMD_0="hdparm -I"
SSD_CMD_1_NORMAL="smartctl -x"
SSD_CMD_1_ALTERNATE="smartctl -a -f brief"
SSD_CMD_MAX=1

# This match SanDisk SSD U100/i100 with any size with version *.xx.* when x < 54
# Seen Error with U100 10.52.01 / i100 CS.51.00 / U100 10.01.04.
MODEL_BLACKLIST_0="SanDisk_SSD_[iU]100.*"
VERSION_BLACKLIST_0="(CS|10)\.([01234].|5[0123])\..*"
MODEL_BLACKLIST_1="SanDisk_SDSA5GK-.*"
VERSION_BLACKLIST_1="CS.54.06"
BLACKLIST_MAX=1

MMC_NAME_0="cid"
MMC_NAME_1="csd"
MMC_NAME_2="date"
MMC_NAME_3="enhanced_area_offset"
MMC_NAME_4="enhanced_area_size"
MMC_NAME_5="erase_size"
MMC_NAME_6="fwrev"
MMC_NAME_7="hwrev"
MMC_NAME_8="manfid"
MMC_NAME_9="name"
MMC_NAME_10="oemid"
MMC_NAME_11="preferred_erase_size"
MMC_NAME_12="prv"
MMC_NAME_13="raw_rpmb_size_mult"
MMC_NAME_14="rel_sectors"
MMC_NAME_15="serial"
MMC_NAME_MAX=15

NVME_CMD_0="smartctl -x"
NVME_CMD_MAX=0


# get_ssd_model - Return the model name of an ATA device.
#
# inputs:
#   output of hdparm -i command.
#
# outputs:
#   the model name of the device, sanitized of space and punctuation.
get_ssd_model() {
  echo "$1" | sed -e "s/^.*Model=//g" -e "s/,.*//g" -e "s/ /_/g"
}

# get_ssd_version - Return the firmware version of an ATA device.
#
# inputs:
#   output of hdparm -i command.
#
# outputs:
#   the version of the device firmware, sanitized of space and punctuation.
get_ssd_version() {
  echo "$1" | sed -e "s/^.*FwRev=//g" -e "s/,.*//g" -e "s/ /_/g"
}

# is_blacklist - helper function for is_ssd_blacklist.
#
# inputs:
#   the information from the device.
#   the blacklist element to match against.
is_blacklist() {
  echo "$1" | grep -Eq "$2"
}

# is_ssd_blacklist - Return true is the device is blacklisted.
#
# inputs:
#   model : model of the ATA device.
#   version : ATA device firmware version.
#
# outputs:
#   True if the device belongs into the script blacklist.
#   When an ATA device is in the blacklist, only a subset of the ATA SMART
#   output is displayed.
is_ssd_blacklist() {
  local model="$1"
  local version="$2"
  local model_blacklist
  local version_blacklist
  local i

  for i in $(seq 0 ${BLACKLIST_MAX}); do
    eval model_blacklist=\${MODEL_BLACKLIST_${i}}
    if is_blacklist "${model}" "${model_blacklist}"; then
      eval version_blacklist=\${VERSION_BLACKLIST_${i}}
      if is_blacklist "${version}" "${version_blacklist}"; then
        return 0
      fi
    fi
  done
  return 1
}

# print_ssd_info - Print SATA device information
#
# inputs:
#   device name for instance sdb.
print_ssd_info() {
  # BUG: On some machines, smartctl -x causes SSD error (crbug.com/328587).
  # We need to check model and firmware version of the SSD to avoid this bug.

  # SSD model and firmware version is on the same line in hdparm result.
  local hdparm_result="$(hdparm -i "/dev/$1" | grep "Model=")"
  local model="$(get_ssd_model "${hdparm_result}")"
  local version="$(get_ssd_version "${hdparm_result}")"
  local ssd_cmd
  local i

  if is_ssd_blacklist "${model}" "${version}"; then
    SSD_CMD_1=${SSD_CMD_1_ALTERNATE}
  else
    SSD_CMD_1=${SSD_CMD_1_NORMAL}
  fi

  for i in $(seq 0 ${SSD_CMD_MAX}); do
    # Use eval for variable indirection.
    eval ssd_cmd=\${SSD_CMD_${i}}
    echo "$ ${ssd_cmd} /dev/$1"
    ${ssd_cmd} "/dev/$1"
    echo ""
  done
}

# print_mmc_info - Print eMMC device information
#
# inputs:
#   device name for instance mmcblk0.
print_mmc_info() {
  local mmc_name
  local mmc_path
  local mmc_result
  local i

  for i in $(seq 0 ${MMC_NAME_MAX}); do
    eval mmc_name=\${MMC_NAME_${i}}
    mmc_path="/sys/block/$1/device/${mmc_name}"
    mmc_result="$(cat "${mmc_path}" 2>/dev/null)"
    printf "%-20s | %s\n" "${mmc_name}" "${mmc_result}"
  done

  mmc extcsd read "/dev/$1"
}

# print_nvme - Print NVMe device information
#
# inputs:
#   device name for instance nvme0n1.
print_nvme_info() {
  local nvme_cmd
  local mvme_dev="/dev/$1"
  local i

  for i in $(seq 0 ${NVME_CMD_MAX}); do
    # Use eval for variable indirection.
    eval nvme_cmd=\${NVME_CMD_${i}}
    echo "$ ${nvme_cmd} ${mvme_dev}"
    ${nvme_cmd} "${mvme_dev}"
    echo ""
  done
}

# get_storage_info - Print device information.
#
# Print device information for all fixed devices in the system.
get_storage_info() {
  local dev

  for dev in $(list_fixed_ata_disks); do
    print_ssd_info "${dev}"
  done

  for dev in $(list_fixed_mmc_disks); do
    print_mmc_info "${dev}"
  done

  for dev in $(list_fixed_nvme_disks); do
    print_nvme_info "${dev}"
  done
}
