#!/bin/sh
# Copyright 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This runs from the factory install/reset shim. This MUST be run
# from USB, in developer mode. This script contains functions to erase
# securly the disk and verify it has been erased properly.

# if used inside the test harness, 2 variables are defined:
# TEST_DELAY: to reduce the amount of time checking for the emmc to be ready
#             again,
# TEST_FIO_OUTPUT: to send the output of fio to a known file. By default,
#             if the file is generated with $(mktemp fio_output_XXXXXX)

# Check if a specific security function is supported
# Parameters:
# - device to query
# - function to query
# Return true if the function is supported, false if not supported or unknown
is_security_function_supported() {
  # List 10 lines below Security header, look if function is not preceded
  # by "not"
  hdparm -I "$1" | grep -A10 "^Security" | grep -q "^[[:blank:]]\+$2$"
}

# Return useful bits of the MMC status
#
# Return some status bits that indicates if the device has completed
# outstanding commands.
#
# if 'mmc status get' fails returns 0, which is an invalid status.
get_mmc_status() {
  local status
  status=$(mmc status get "$1" | sed -nre 's/^SEND_STATUS response: (.*)/\1/p')
  # The state is defined in chapter 6.13 of eMMC rev 5.
  # Ideally, we should check that all the error bits to be set to 0.
  # Now, reading in more details, eMMC device are not garantee to be always
  # valid (see X or R mode) or some bit are reserved.
  # Therefore, we limit only to flags that are always valid:
  # bit 6: EXCEPTION_EVENT: set to 0
  # bit 8: READY_FOR_DATA: set to 0 (1 while sanitizing)
  # bit 9-12: CURRENT_STATE: set to 4 (Tran), set to 7 (Prg while sanitizing)
  printf "0x%08x\n" $((status & 0x00001F40))
}


# Erase a mmc device using firmware functions
#
# Ask the device to trim all sectors.
# Then, ask the device to physically erase all trimmed sectors.
secure_erase_mmc() {
  local disk="$1"
  local delay=${TEST_DELAY:-5}
  local count
  local secure
  local rc

  # Mark all location as unused - try secure first.
  for secure in "-s" " "; do
    blkdiscard "${secure}" "${disk}"
    rc=$?
    if [ ${rc} -eq 0 ]; then
      break
    fi
  done
  if [ ${rc} -ne 0 ]; then
    echo "security not supported, just doing overwrite"
    return 0
  fi


  # Physically erase unused locations.
  mmc_orig_status=$(get_mmc_status "${disk}")
  if [ "${mmc_orig_status}" != "0x00000900" ]; then
    echo "Not ready for sanitize: status ${mmc_orig_status}."
    return 1
  fi
  mmc sanitize "${disk}"

  count=120  # wait up to 10 minutes
  mmc_status="0xffffffff"
  while [ "${mmc_status}" != "${mmc_orig_status}" -a ${count} -gt 0 ]; do
    sleep "${delay}"
    mmc_status=$(get_mmc_status "${disk}")
    : $(( count -= 1))
  done

  if [ "${mmc_status}" != "${mmc_orig_status}" ]; then
    echo "Device is stuck sanitizing: status ${mmc_status}."
    return 1
  fi
}

# Erase an ATA device using internal firmware function
#
# To trigger the ATA SECURE ERASE function, the disk must be
# in security mode SEC4 (aka locked) or SEC5 (aka secured).
# Disks are usually in SEC1 (unsecured).
# First put the disk in SEC5 then Erase it, that put it back in SEC1.
secure_erase_sata() {
  local disk="$1"
  local temp_password="chromeos"
  local sec_mode="--security-erase"

  is_security_function_supported "${disk}" "supported"
  if [ $? -eq 0 ]; then
    hdparm --user-master u --security-set-pass \
        "${temp_password}" "${disk}" || return $?

    # Check what is supported: Enhanced or just Normal
    # Try enhanced secure erase method: enhanced method scrubs the SSD further.
    is_security_function_supported "${disk}" "supported: enhanced erase"
    if [ $? -eq 0 ]; then
      sec_mode="--security-erase-enhanced"
    fi
    hdparm --user-master u "${sec_mode}" \
        "${temp_password}" "${disk}" || return $?
  else
    echo "security not supported, just doing overwrite"
  fi
}

# Erase an NVMe device using nvme format
#
# We first secure_erase with crypto mode, if failed, then we try to do with
# user data mode with a timeout proportion to the size of the device.
secure_erase_nvme() {
  local disk="$1"
  local ses_user="1"  # 0: no secure, 1: user data erase, 2: cryptographic erase
  local ses_crypto="2"
  local one_gb=$((1024 * 1024 * 1024))  # 1gb size in bytes
  local base_timeout=$((60 * 1000))  # base timeout for 1 minute

  # Format with crypto mode
  nvme format "${disk}" --ses "${ses_crypto}" && return 0

  local dev_size="$(lsblk --bytes --noheadings --output SIZE "${disk}")"
  # Let timeout prportion to the size of device
  local timeout="$((${base_timeout} + ${dev_size} * 10 * 1000 / ${one_gb}))"

  # Format with userdata mode
  nvme format "${disk}" --ses "${ses_user}" --timeout "${timeout}" || return $?
}

# Erase a device using its internal firmware function
#
# Arguments:
#  disk: the block device to erase ("/dev/sda", "/dev/mmcblk0")
# Returns:
#  0 if the erase is either not supported or completed
#  !0 if the erase process could not complete or failed.
secure_erase() {
  local disk="$1"
  local disk_type=$(get_device_type "${disk}")
  # Identify if mmc or sata.
  case "$disk_type" in
    MMC)
      secure_erase_mmc "${disk}"
    ;;
    ATA)
      secure_erase_sata "${disk}"
    ;;
    NVME)
      secure_erase_nvme "${disk}"
    ;;
    *)
      echo "Unable to identify the type of disk: -${disk_type}-"
      return 1
  esac
}

# Use fio to write/verify a pattern
#
# The first and last 1M of the disk are zeroed, the rest is written
# with a random patter fio can verify latter.
#
# Argument
#  disk: the device to erase
#  disk_size: the size of the device
#  disk_op: "write" to write over the SSD, "verify" to check the SSD
#             has been overwritten properly.
# Returns:
#  fio error code if fio could run, 1 otherwise.
perform_fio_op() {
  # Globals, used by factory_secure.fio
  local disk="$1"
  local disk_size="$2"
  local disk_op="$3"

  local dev_main_area_end=$(( ${disk_size} - 1048576 ))
  local block_size=1048576
  local fio_err=0
  local fio_output="${TEST_FIO_OUTPUT}"
  local fio_regex='/^(secure|fio)/s/.* err= *([[:digit:]]+).*/\1/p'
  local input

  export FIO_DEV="${disk}"
  export FIO_DEV_MAIN_AREA_SIZE=$(( ${disk_size} - 2097152 ))

  if [ -z "${fio_output}" ]; then
    fio_output="$(mktemp fio_output_XXXXXX)"
  fi
  case "$disk_op" in
    write)
      export FIO_VERIFY_ONLY=0
      # Erase the begining an the end of the drive. Write random first
      # to ensure the data is scrambled.
      for input in "urandom" "zero"; do
        dd bs="${block_size}" of="${disk}" oflag=dsync iflag=fullblock \
            if=/dev/${input} count=1
        dd bs="${block_size}" of="${disk}" oflag=dsync iflag=fullblock \
            if=/dev/${input} seek=$(( dev_main_area_end / ${block_size} ))
      done
      ;;
    verify)
      export FIO_VERIFY_ONLY=1
      ;;
    *)
      echo "Unsupported operation: -${disk_op}-"
      return 1
  esac

  local fio_script="$(mktemp fio_config_XXXXXX)"
  cat >"${fio_script}" <<HERE
; Copyright 2014 The Chromium Authors. All rights reserved.
; Use of this source code is governed by a BSD-style license that can be
; found in the LICENSE file.
;
; Erase the drive with a pattern, except for the begining and the end.
;
[secure]
filename=\${FIO_DEV}
ioengine=libaio
iodepth=32
direct=1
readwrite=write
bs=256k

offset=1m
size=\${FIO_DEV_MAIN_AREA_SIZE}
do_verify=1
verify=md5
verify_only=\${FIO_VERIFY_ONLY}
HERE
  # Write a pattern on the media for future verification.
  # fio configuration file use DEV, DEV_MAIN_AREA_SIZE and VERIFY_ONLY
  fio "${fio_script}" --output "${fio_output}"
  rm -f "${fio_script}"
  fio_err=$(sed -nr "${fio_regex}" "${fio_output}")
  if [ -z "${fio_err}" ]; then
    echo "-- output of fio not understood --"
    cat "${fio_output}"
    fio_err=1
  elif [ ${fio_err} -ne 0 ]; then
    cat "${fio_output}"
    if [ $FIO_VERIFY_ONLY -eq 0 ]; then
      echo "-- writing pattern failed --"
      echo "The storage device is not working properly."
    else
      # Check if we fail to read the device, or the pattern is wrong.
      echo "-- verifying pattern failed --"
      if [ ${fio_err} -eq 84 ]; then
        echo -n "The storage device has either been tampered with or "
        echo "not securely erased properly."
      else
        echo "Storage device broken: unable to read some sector from it."
      fi
    fi
  else
    rm "${fio_output}"
  fi
  return ${fio_err}
}

