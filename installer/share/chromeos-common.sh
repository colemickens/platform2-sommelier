#!/bin/sh
# Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# This contains common constants and functions for installer scripts. This must
# evaluate properly for both /bin/bash and /bin/sh, since it's used both to
# create the initial image at compile time and to install or upgrade a running
# image.

# The GPT tables describe things in terms of 512-byte sectors, but some
# filesystems prefer 4096-byte blocks. These functions help with alignment
# issues.

PARTITION_NUM_STATE=1
PARTITION_NUM_KERN_A=2
PARTITION_NUM_ROOT_A=3
PARTITION_NUM_KERN_B=4
PARTITION_NUM_ROOT_B=5
PARTITION_NUM_KERN_C=6
PARTITION_NUM_ROOT_C=7
PARTITION_NUM_OEM=8
PARTITION_NUM_RWFW=11
PARTITION_NUM_EFI_SYSTEM=12

# This returns the size of a file or device in 512-byte sectors, rounded up if
# needed.
# Invoke as: subshell
# Args: FILENAME
# Return: whole number of sectors needed to fully contain FILENAME
numsectors() {
  if [ -b "${1}" ]; then
    dev=${1##*/}
    if [ -e /sys/block/$dev/size ]; then
      cat /sys/block/$dev/size
    else
      part=${1##*/}
      block=$(get_block_dev_from_partition_dev "${1}")
      block=${block##*/}
      cat /sys/block/$block/$part/size
    fi
  else
    local bytes=$(stat -c%s "$1")
    local sectors=$(( $bytes / 512 ))
    local rem=$(( $bytes % 512 ))
    if [ $rem -ne 0 ]; then
      sectors=$(( $sectors + 1 ))
    fi
    echo $sectors
  fi
}

# Locate the cgpt tool. It should already be installed in the build chroot,
# but some of these functions may be invoked outside the chroot (by
# image_to_usb or similar), so we need to find it.
GPT=""

locate_gpt() {
  if [ -z "$GPT" ]; then
    if [ -x "${DEFAULT_CHROOT_DIR:-}/usr/bin/cgpt" ]; then
      GPT="${DEFAULT_CHROOT_DIR:-}/usr/bin/cgpt"
    else
      GPT=$(which cgpt 2>/dev/null) || /bin/true
      if [ -z "$GPT" ]; then
        echo "can't find cgpt tool" 1>&2
        exit 1
      fi
    fi
  fi
}

# Read GPT table to find the starting location of a specific partition.
# Invoke as: subshell
# Args: DEVICE PARTNUM
# Returns: offset (in sectors) of partition PARTNUM
partoffset() {
  sudo $GPT show -b -i $2 $1
}

# Read GPT table to find the size of a specific partition.
# Invoke as: subshell
# Args: DEVICE PARTNUM
# Returns: size (in sectors) of partition PARTNUM
partsize() {
  sudo $GPT show -s -i $2 $1
}

# Extract the whole disk block device from the partition device.
# This works for /dev/sda3 (-> /dev/sda) as well as /dev/mmcblk0p2
# (-> /dev/mmcblk0).
get_block_dev_from_partition_dev() {
  local partition=$1
  if ! (expr match "$partition" ".*[0-9]$" >/dev/null) ; then
    echo "Invalid partition name: $partition" >&2
    exit 1
  fi
  # Removes any trailing digits.
  local block=$(echo "$partition" | sed -e 's/[0-9]*$//')
  # If needed, strip the trailing 'p'.
  if (expr match "$block" ".*[0-9]p$" >/dev/null); then
    echo "${block%p}"
  else
    echo "$block"
  fi
}

# Extract the partition number from the partition device.
# This works for /dev/sda3 (-> 3) as well as /dev/mmcblk0p2 (-> 2).
get_partition_number() {
  local partition=$1
  if ! (expr match "$partition" ".*[0-9]$" >/dev/null) ; then
    echo "Invalid partition name: $partition" >&2
    exit 1
  fi
  # Extract the last digit.
  echo "$partition" | sed -e 's/^.*\([0-9]\)$/\1/'
}

# Construct a partition device name from a whole disk block device and a
# partition number.
# This works for [/dev/sda, 3] (-> /dev/sda3) as well as [/dev/mmcblk0, 2]
# (-> /dev/mmcblk0p2).
make_partition_dev() {
  local block=$1
  local num=$2
  # If the disk block device ends with a number, we add a 'p' before the
  # partition number.
  if (expr match "$block" ".*[0-9]$" >/dev/null) ; then
    echo "${block}p${num}"
  else
    echo "${block}${num}"
  fi
}

# Return the type of device.
#
# The type can be:
# MMC, SD for device managed by the MMC stack
# ATA for ATA disk
# OTHER for other devices.
get_device_type() {
  local dev="$(basename "$1")"
  local vdr
  local type_file
  local vendor_file
  type_file="/sys/block/${dev}/device/type"
  # To detect device managed by the MMC stack
  case $(readlink -f "${type_file}") in
    *mmc*)
      cat "${type_file}"
      ;;
    *usb*)
      # Now if it contains 'usb', it is managed through
      # a USB controller.
      echo "USB"
      ;;
    *target*)
      # Other SCSI devices.
      # Check if it is an ATA device.
      vdr="$(cat "/sys/block/${dev}/device/vendor")"
      if [ "${vdr%% *}" = "ATA" ]; then
        echo "ATA"
      else
        echo "OTHER"
      fi
      ;;
    *)
      echo "OTHER"
  esac
}

# ATA disk have ATA as vendor.
# They may not contain ata in their device path if behind a SAS
# controller.
# Exclude disks with size 0, it means they did not spin up properly.
list_fixed_ata_disks() {
  local sd
  local remo
  local vdr
  local size

  for sd in /sys/block/sd*; do
    if [ ! -r "${sd}/size" ]; then
      continue
    fi
    size=$(cat "${sd}/size")
    remo=$(cat "${sd}/removable")
    vdr=$(cat "${sd}/device/vendor")
    if [ "${vdr%% *}" = "ATA" -a ${remo:-0} -eq 0 -a ${size:-0} -gt 0 ]; then
      echo "${sd##*/}"
    fi
  done
}

# We assume we only have eMMC devices, not removable MMC devices.
# also, do not consider special hardware partitions non the eMMC, like boot.
# These devices are built on top of the eMMC sysfs path:
# /sys/block/mmcblk0 -> .../mmc_host/.../mmc0:0001/.../mmcblk0
# /sys/block/mmcblk0boot0 -> .../mmc_host/.../mmc0:0001/.../mmcblk0/mmcblk0boot0
# /sys/block/mmcblk0boot1 -> .../mmc_host/.../mmc0:0001/.../mmcblk0/mmcblk0boot1
# /sys/block/mmcblk0rpmb -> .../mmc_host/.../mmc0:0001/.../mmcblk0/mmcblk0rpmb
#
# Their device link points back to mmcblk0, not to the hardware
# device (mmc0:0001). Therefore there is no type in their device link.
# (it should be /device/device/type)
list_fixed_mmc_disks() {
  local mmc
  local type_file
  for mmc in /sys/block/mmcblk*; do
    type_file="${mmc}/device/type"
    if [ -r "${type_file}" ]; then
      if [ "$(cat "${type_file}")" = "MMC" ]; then
        echo "${mmc##*/}"
      fi
    fi
  done
}

# Find the drive to install based on the build write_cgpt.sh
# script. If not found, return ""
get_fixed_dst_drive() {
  local dev
  if [ -z "${DEFAULT_ROOTDEV}" ]; then
    dev=""
  else
    # No " here, the variable may contain wildcards.
    dev="/dev/$(basename ${DEFAULT_ROOTDEV})"
    if [ ! -b "${dev}" ]; then
      # The device is not found
      dev=""
    fi
  fi
  echo "${dev}"
}

legacy_offset_size_export() {
  # Exports all the variables that install_gpt did previously.
  # This should disappear eventually, but it's here to make existing
  # code work for now.

  START_STATEFUL=$(partoffset $1 ${PARTITION_NUM_STATE})
  START_KERN_A=$(partoffset $1 ${PARTITION_NUM_KERN_A})
  START_ROOTFS_A=$(partoffset $1 ${PARTITION_NUM_ROOT_A})
  START_KERN_B=$(partoffset $1 ${PARTITION_NUM_KERN_B})
  START_ROOTFS_B=$(partoffset $1 ${PARTITION_NUM_ROOT_B})
  START_OEM=$(partoffset $1 ${PARTITION_NUM_OEM})
  START_RWFW=$(partoffset $1 ${PARTITION_NUM_RWFW})
  START_ESP=$(partoffset $1 ${PARTITION_NUM_EFI_SYSTEM})

  NUM_STATEFUL_SECTORS=$(partsize $1 ${PARTITION_NUM_STATE})
  NUM_KERN_SECTORS=$(partsize $1 ${PARTITION_NUM_KERN_A})
  NUM_ROOTFS_SECTORS=$(partsize $1 ${PARTITION_NUM_ROOT_A})
  NUM_OEM_SECTORS=$(partsize $1 ${PARTITION_NUM_OEM})
  NUM_RWFW_SECTORS=$(partsize $1 ${PARTITION_NUM_RWFW})
  NUM_ESP_SECTORS=$(partsize $1 ${PARTITION_NUM_EFI_SYSTEM})

  STATEFUL_IMG_SECTORS=$(partsize $1 ${PARTITION_NUM_STATE})
  KERNEL_IMG_SECTORS=$(partsize $1 ${PARTITION_NUM_KERN_A})
  ROOTFS_IMG_SECTORS=$(partsize $1 ${PARTITION_NUM_ROOT_A})
  OEM_IMG_SECTORS=$(partsize $1 ${PARTITION_NUM_OEM})
  ESP_IMG_SECTORS=$(partsize $1 ${PARTITION_NUM_EFI_SYSTEM})
}

edit_mbr() {
  locate_gpt
  local start_esp=$(partoffset "$1" ${PARTITION_NUM_EFI_SYSTEM})
  local num_esp_sectors=$(partsize "$1" ${PARTITION_NUM_EFI_SYSTEM})
  sfdisk "${1}" <<EOF
unit: sectors

disk1 : start=   $start_esp, size=    $num_esp_sectors, Id= c, bootable
disk2 : start=   1, size=    1, Id= ee
EOF
}

install_hybrid_mbr() {
  # Creates a hybrid MBR which points the MBR partition 1 to GPT
  # partition 12 (ESP). This is useful on ARM boards that boot
  # from MBR formatted disks only.
  #
  # Currently, this code path is used principally to install to
  # SD cards using chromeos-install run from inside the chroot.
  # In that environment, `sfdisk` can be racing with udev, leading
  # to EBUSY when it calls BLKRRPART for the target disk.  We avoid
  # the conflict by using `udevadm settle`, so that udev goes first.
  # cf. crbug.com/343681.

  echo "Creating hybrid MBR"
  if ! edit_mbr "${1}"; then
    udevadm settle
    blockdev --rereadpt "${1}"
  fi
}
