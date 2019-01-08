#!/bin/sh

# Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script can be called during startup to trash the stateful partition
# and possibly reset the other root filesystem

. /usr/share/misc/chromeos-common.sh

SCRIPT="$0"

# stateful partition isn't around for logging, so dump to the screen:
set -x

PRESERVED_TAR="/tmp/preserve.tar"
STATE_PATH="/mnt/stateful_partition"

wait_for_tty() {
  local device="$1"
  local loop=0
  until stty -F "${device}"; do
    if [ ${loop} -ge 30 ]; then
      dlog "timed out waiting for ${device}"
      break
    fi
    sleep 0.1
    : $(( loop += 1 ))
  done
}

wipe_block_dev() {
  local dev="$1"
  # Wipe the filesystem size if we can determine it. Full partition wipe
  # takes a long time on 16G SSD or rotating media.
  if dumpe2fs "${dev}" ; then
    FS_BLOCKS=$(dumpe2fs -h "${dev}" | grep "Block count" | sed "s/^.*:\W*//")
    FS_BLOCKSIZE=$(dumpe2fs -h "${dev}" | grep "Block size" | sed "s/^.*:\W*//")
  else
    FS_BLOCKS=$(numsectors "${dev}")
    FS_BLOCKSIZE=$(blocksize "${dev}")
  fi
  BLOCKS_4M=$((4 * 1024 * 1024 / $FS_BLOCKSIZE))  # 4MiB in sectors
  FULL_BLKS=$(($FS_BLOCKS / $BLOCKS_4M))
  REMAINDER_SECTORS=$(($FS_BLOCKS % $BLOCKS_4M))

  if type pv; then
    # Opening a TTY device with no one else attached will allocate and reset
    # terminal attributes. To prevent user input echo and ctrl-breaks, we need
    # to allocate and configure the terminal before using tty for output.
    # Note &2 is currently used for debug messages dumping so we can't redirect
    # subshell by 2>"${TTY}".
    ( trap '' INT QUIT KILL TSTP TERM
      wait_for_tty "${TTY}"
      stty -F "${TTY}" raw -echo -cread || :
      exec 0</dev/null || :
      pv -etp -s $((4 * 1024 * 1024 * ${FULL_BLKS})) /dev/zero 2>"${TTY}" |
        dd of="${dev}" bs=4M count=${FULL_BLKS} iflag=fullblock oflag=sync
    )
  else
    dd if=/dev/zero of="${dev}" bs=4M count=${FULL_BLKS}
  fi
  dd if=/dev/zero of="${dev}" bs=${FS_BLOCKSIZE} count=${REMAINDER_SECTORS} \
    seek=$(($FULL_BLKS * $BLOCKS_4M))
}

# Faster, but less thorough data destruction.
wipe_block_dev_fast() {
  local dev="$1"
  dd if=/dev/zero of="${dev}" bs=4M count=1
}

# Get the specified information from the specified UBI volume.
#  $1 the field name such as "reserved_for_bad", or "data_bytes".
#  $2 the volume number such as "1" or "1_0".
get_ubi_var() {
  local field="$1"
  local part="$2"
  cat "/sys/class/ubi/ubi${part}/${field}"
}

get_ubi_reserved_ebs() {
  get_ubi_var reserved_for_bad "$@"
}

get_ubi_volume_size() {
  local part="$1"
  get_ubi_var data_bytes "${part}_0"
}

# Calculate the maximum number of bad blocks per 1024 blocks for UBI.
#  $1 partition number
calculate_ubi_max_beb_per_1024() {
  local part_no mtd_size eb_size nr_blocks
  part_no="$1"
  # The max beb per 1024 is on the total device size, not the partition size.
  mtd_size=$(cat /sys/class/mtd/mtd0/size)
  eb_size=$(cat /sys/class/mtd/mtd0/erasesize)
  nr_blocks=$((mtd_size / eb_size))
  reserved_ebs=$(get_ubi_reserved_ebs "${part_no}")
  echo $((reserved_ebs * 1024 / nr_blocks))
}

# Wipe a UBI device.
#  $1 the device node. This must be in the form /dev/ubiX_0 or /dev/ubiblockX_0.
wipe_ubi_dev() {
  local dev phy_dev part_no part_name
  dev="$1"
  part_no=$(echo "${dev}" | sed -e 's#/dev/ubi\(block\)\?\([0-9]\)_0#\2#')
  phy_dev="/dev/ubi${part_no}"
  # We only wipe state and root partitions in this function.
  case "${part_no}" in
    "${PARTITION_NUM_STATE}")
      part_name="STATE"
      ;;
    "${PARTITION_NUM_ROOT_A}")
      part_name="ROOT_A"
      ;;
    "${PARTITION_NUM_ROOT_B}")
      part_name="ROOT_B"
      ;;
    *)
      part_name="UNKNOWN_${part_no}"
      echo "Do not know how to name UBI partition number ${part_no}."
      ;;
  esac

  if [ ! -c "${phy_dev}" ]; then
    # Try to attach the volume to obtain info about it.
    ubiattach -m "${part_no}" -d "${part_no}"
  fi

  local max_beb_per_1024 volume_size
  max_beb_per_1024=$(calculate_ubi_max_beb_per_1024 "${part_no}")
  volume_size=$(get_ubi_volume_size "${part_no}")

  ubidetach -d "${part_no}"
  ubiformat -y -e 0 "/dev/mtd${part_no}"

  # We need to attach so that we could set max beb/1024 and create a volume.
  # After a volume is created, we don't need to specify max beb/1024 anymore.
  ubiattach -d "${part_no}" -m "${part_no}" \
            --max-beb-per1024 "${max_beb_per_1024}"

  ubimkvol -s "${volume_size}" -N "${part_name}" "${phy_dev}"
}

# Wipe an MTD device.
#  $1 the device node. This must be /dev/mtdX or /dev/ubiX_0.
wipe_mtd_dev() {
  local dev="$1"
  case "${dev}" in
    "/dev/mtd"*)
      flash_erase "${dev}" 0 0
      ;;
    "/dev/ubi"*)
      wipe_ubi_dev "${dev}"
      ;;
    *)
      echo "Do not know how to wipe ${dev}."
      ;;
  esac
}

# Perform media-dependent wipe of the device. IS_MTD flag controls if the wipe
# should be an MTD wipe or a block device wipe.
#  $1 the device, such as /dev/sda3, /dev/ubi5_0
wipedev() {
  local dev="$1"
  if [ "${IS_MTD}" = "1" ]; then
    wipe_mtd_dev "${dev}"
  elif [ "${FAST_WIPE}" = "fast" ]; then
    wipe_block_dev_fast "${dev}"
  else
    wipe_block_dev "${dev}"
  fi
}

# Make sure the active kernel is still bootable after being wiped.
# The system may be in AU state that active kernel does not have "successful"
# bit set to 1 (only tries).
ensure_bootable_kernel() {
  local kernel_num="$1"
  local dst="$2"
  local active_flag="$(cgpt show -S -i "${kernel_num}" "${dst}")"
  local priority="$(cgpt show -P -i "${kernel_num}" "${dst}")"

  if [ "${active_flag}" -lt 1 ]; then
    cgpt add -i "${kernel_num}" "${dst}" -S 1
  fi
  if [ "${priority}" -lt 1 ]; then
    cgpt prioritize -i "${kernel_num}" "${dst}" -P 3
  fi
  sync
}

# Wipe non-active kernel and root partitions unless |KEEPIMG| is set. This is
# used in the factory to remove the test image before shipping the device.
potentially_wipe_non_active_kernel_and_root() {
  if [ -z "${KEEPIMG}" ]; then
    ensure_bootable_kernel "${KERNEL_PART_NUM}" "${ROOT_DISK}"
    wipedev ${OTHER_ROOT_DEV}
    wipedev ${OTHER_KERNEL_DEV}
  fi
}

create_stateful_file_system() {
  if [ "${IS_MTD}" = "1" ]; then
    mkfs.ubifs -y -x none -R 0 "${STATE_DEV}"
  else
    mkfs.ext4 "${STATE_DEV}"
    # TODO(wad) tune2fs.
  fi
}

restore_preserved_files() {
  # If there were preserved files, restore them.
  if [ -e $PRESERVED_TAR ]; then
    tar xfp ${PRESERVED_TAR} -C ${STATE_PATH}
    touch /mnt/stateful_partition/unencrypted/.powerwash_completed
  fi

  # Restore the log file
  clobber-log --restore "${SCRIPT}" "$@"
}

# Removes the following keys from the VPD when transitioning between developer
# and verified mode. Do not do this for a safe wipe.
#   first_active_omaha_ping_sent
#   recovery_count
remove_vpd_keys() {
  local key=""
  if [ "${SAFE_WIPE}" != "safe" ]; then
    for key in "recovery_count" "first_active_omaha_ping_sent"; do
      # Do not report failures as the key might not even exist in the VPD.
      vpd -i RW_VPD -d "${key}"
    done
  fi
}

main() {
  # Make sure the stateful partition has been unmounted.
  umount -n "${STATE_PATH}"

  # Destroy user data: wipe the stateful partition.
  wipedev "${STATE_DEV}"
  create_stateful_file_system

  # Mount the fresh image for last minute additions.
  mount -n "${STATE_DEV}" "${STATE_PATH}"
  restore_preserved_files

  # Destroy less sensitive data.
  remove_vpd_keys
  potentially_wipe_non_active_kernel_and_root
}

main "$@"
