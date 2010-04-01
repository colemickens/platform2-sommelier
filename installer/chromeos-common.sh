# Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# This contains common constants and functions for installer scripts. This must
# evaluate properly for both /bin/bash and /bin/sh, since it's used both to
# create the initial image at compile time and to install or upgrade a running
# image.

# Here are the GUIDs we'll be using to identify various partitions.
STATEFUL_GUID='ebd0a0a2-b9e5-4433-87c0-68b6b72699c7'
KERN_GUID='fe3a2a5d-4f32-41a7-b725-accc3285a309'
ROOTFS_GUID='3cb8e202-3b7e-47dd-8a3c-7ff2a13cfcec'
ESP_GUID='28732ac1-1ff8-d211-ba4b-00a0c93ec93b'


# The GPT tables describe things in terms of 512-byte sectors, but some
# filesystems prefer 4096-byte blocks. These functions help with alignment
# issues.

# This returns the size of a file or device in 512-byte sectors, rounded up if
# needed.
# Invoke as: subshell
# Args: FILENAME
# Return: whole number of sectors needed to fully contain FILENAME
numsectors() {
  case $1 in
  /dev/*[0-9])
    dnum=${1##*/}
    dev=${dnum%%[0-9]*}
    cat /sys/block/$dev/$dnum/size 
    ;;
  /dev/*)
    dev=${1##*/}
    cat /sys/block/$dev/size 
    ;;
  *)
    local bytes=$(stat -c%s "$1")
    local sectors=$(( $bytes / 512 ))
    local rem=$(( $bytes % 512 ))
    if [ $rem -ne 0 ]; then
      sectors=$(( $sectors + 1 ))
    fi
    echo $sectors
    ;;
  esac    
}

# Round a number of 512-byte sectors up to an integral number of 4096-byte
# blocks.
# Invoke as: subshell
# Args: SECTORS
# Return: Next largest multiple-of-8 sectors (ex: 4->8, 33->40, 32->32)
roundup() {
  local num=$1
  local rem=$(( $num % 8 ))

  if (( $rem )); then
    num=$(($num + 8 - $rem))
  fi
  echo $num
}

# Truncate a number of 512-byte sectors down to an integral number of 4096-byte
# blocks.
# Invoke as: subshell
# Args: SECTORS
# Return: Next smallest multiple-of-8 sectors (ex: 4->0, 33->32, 32->32)
rounddown() {
  local num=$1
  local rem=$(( $num % 8 ))

  if (( $rem )); then
    num=$(($num - $rem))
  fi
  echo $num
}


# We need to locate the gpt tool. It should already be installed in the build
# chroot, but some of these functions may be invoked outside the chroot (by
# image_to_usb or similar), so we need to find it.
GPT=$(which gpt 2>/dev/null) || /bin/true
if [ -z "$GPT" ]; then
  if [ -x "${DEFAULT_CHROOT_DIR:-}/usr/bin/gpt" ]; then
    GPT="${DEFAULT_CHROOT_DIR:-}/usr/bin/gpt"
  else
    echo "can't find gpt tool" 1>&2
    exit 1
  fi
fi


# This installs a GPT into the specified device or file, using the given
# components. If the target is a block device or the FORCE_FULL arg is "true"
# we'll do a full install. Otherwise, it'll be just enough to boot.
# Invoke as: command (not subshell)
# Args: TARGET ROOTFS_IMG KERNEL_IMG STATEFUL_IMG PMBRCODE FORCE_FULL
# Return: nothing
# Side effects: Sets these global variables describing the GPT partitions
#   (all untis are 512-byte sectors):
#   NUM_KERN_SECTORS
#   NUM_ROOTFS_SECTORS
#   NUM_STATEFUL_SECTORS
#   NUM_RESERVED_SECTORS
#   START_KERN_A
#   START_STATEFUL
#   START_ROOTFS_A
#   START_KERN_B
#   START_ROOTFS_B
#   START_RESERVED
install_gpt() {
  local outdev=$1
  local rootfs_img=$2
  local kernel_img=$3
  local stateful_img=$4
  local pmbrcode=$5
  local force_full="${6:-}"

  # The gpt tool requires a fixed-size target to work on, so we may have to
  # create a file of the appropriate size. Let's figure out what that size is
  # now. The partition layout is gonna look something like this:
  #
  #   PMBR (512 bytes)
  #   Primary GPT Header (512 bytes)
  #   Primary GPT Table (16K)
  #   Kernel A                                        partition 2
  #   Kernel B                                        partition 4
  #   reserved space for additional partitions        partition 6,7,8,...
  #   Stateful partition (as large as possible)       partition 1
  #   Rootfs B                                        partition 5
  #   Rootfs A                                        partition 3
  #   Secondary GPT Table (16K)
  #   Secondary GPT Header (512 bytes)
  #
  # Please refer to the official ChromeOS documentation for the details and
  # explanation behind the layout and partition numbering scheme. The short
  # version is that 1) we want everything above partition 3 to be optional when
  # we're creating a bootable USB key, 2) we want to be able to add new
  # partitions later without breaking current scripts, and 3) we may need to
  # increase the size of the rootfs during an upgrade, which means shrinking
  # the size of the stateful partition.
  #
  # One nonobvious contraint is that the ext2-based filesystems typically use
  # 4096-byte blocks. We'll need a little padding at each end of the disk to
  # align the useable space to that size boundary.

  # Here are the size limits that we're currently requiring
  local max_kern_sectors=32768        # 16M
  local max_rootfs_sectors=2097152    # 1G
  local max_reserved_sectors=131072   # 64M
  local min_stateful_sectors=262144   # 128M, expands to fill available space

  local num_pmbr_sectors=1
  local num_gpt_hdr_sectors=1
  local num_gpt_table_sectors=32              # 16K
  local num_footer_sectors=$(($num_gpt_hdr_sectors + $num_gpt_table_sectors))
  local num_header_sectors=$(($num_pmbr_sectors + $num_footer_sectors))

  local start_useful=$(roundup $num_header_sectors)

  # What are we doing?
  if [[ -b "$outdev" || "$force_full" = "true" ]]; then
    # Block device, need to be root.
    if [[ -b "$outdev" ]]; then
      local sudo=sudo
    else
      local sudo=""
    fi

    # Full install, use max sizes and create both A & B images.
    NUM_KERN_SECTORS=$max_kern_sectors
    NUM_ROOTFS_SECTORS=$max_rootfs_sectors
    NUM_RESERVED_SECTORS=$max_reserved_sectors

    # Where do things go?
    START_KERN_A=$start_useful
    START_KERN_B=$(($START_KERN_A + $NUM_KERN_SECTORS))
    START_RESERVED=$(($START_KERN_B + $NUM_KERN_SECTORS))
    START_STATEFUL=$(($START_RESERVED + $NUM_RESERVED_SECTORS))

    local total_sectors=$(numsectors $outdev)
    local start_gpt_footer=$(($total_sectors - $num_footer_sectors))
    local end_useful=$(rounddown $start_gpt_footer)

    START_ROOTFS_A=$(($end_useful - $NUM_ROOTFS_SECTORS))
    START_ROOTFS_B=$(($START_ROOTFS_A - $NUM_ROOTFS_SECTORS))

    NUM_STATEFUL_SECTORS=$(($START_ROOTFS_B - $START_STATEFUL))
  else
    # Just a local file.
    local sudo=

    # We'll only populate partitions 1, 2, 3. Image B isn't required for this,
    # and the others are still theoretical.
    NUM_KERN_SECTORS=$(roundup $(numsectors $kernel_img))
    NUM_ROOTFS_SECTORS=$(roundup $(numsectors $rootfs_img))
    NUM_STATEFUL_SECTORS=$(roundup $(numsectors $stateful_img))
    NUM_RESERVED_SECTORS=0

    START_KERN_A=$start_useful
    START_STATEFUL=$(($START_KERN_A + $NUM_KERN_SECTORS))
    START_ROOTFS_A=$(($START_STATEFUL + $NUM_STATEFUL_SECTORS))
    START_KERN_B=""
    START_ROOTFS_B=""
    START_RESERVED=""

    # For minimal install, we're not worried about the secondary GPT header
    # being at the end of the device because we're almost always writing to a
    # file. If that's not true, the secondary will just be invalid.
    local start_gpt_footer=$(($START_ROOTFS_A + $NUM_ROOTFS_SECTORS))
    local end_useful=$start_gpt_footer

    local total_sectors=$(($start_gpt_footer + $num_footer_sectors))

    # Create the image file if it doesn't exist.
    if [ ! -e ${outdev} ]; then
      dd if=/dev/zero of=${outdev} bs=512 count=1 seek=$(($total_sectors - 1))
    fi
  fi

  echo "Creating partition tables..."

  # Zap any old partitions (otherwise gpt complains).
  $sudo dd if=/dev/zero of=${outdev} conv=notrunc bs=512 \
    count=$num_header_sectors
  $sudo dd if=/dev/zero of=${outdev} conv=notrunc bs=512 \
    seek=${start_gpt_footer} count=$num_footer_sectors
  
  # Create the new GPT partitions. The order determines the partition number.
  # Note that the partition label is in the GPT only. The filesystem label is
  # what's used to populate /dev/disk/by-label/, and this is not that.
  $sudo $GPT create ${outdev}
  
  $sudo $GPT add -b ${START_STATEFUL} -s ${NUM_STATEFUL_SECTORS} \
    -t ${STATEFUL_GUID} ${outdev}
  $sudo $GPT label -i 1 -l "STATE" ${outdev}
  
  $sudo $GPT add -b ${START_KERN_A} -s ${NUM_KERN_SECTORS} \
    -t ${KERN_GUID} ${outdev}
  $sudo $GPT label -i 2 -l "KERN-A" ${outdev}
  
  $sudo $GPT add -b ${START_ROOTFS_A} -s ${NUM_ROOTFS_SECTORS} \
    -t ${ROOTFS_GUID} ${outdev}
  $sudo $GPT label -i 3 -l "ROOT-A" ${outdev}
  
  # add the rest of the partitions for a full install
  if [ -n "$START_KERN_B" ]; then
    $sudo $GPT add -b ${START_KERN_B} -s ${NUM_KERN_SECTORS} \
      -t ${KERN_GUID} ${outdev}
    $sudo $GPT label -i 4 -l "KERN-B" ${outdev}
   
    $sudo $GPT add -b ${START_ROOTFS_B} -s ${NUM_ROOTFS_SECTORS} \
      -t ${ROOTFS_GUID} ${outdev}
    $sudo $GPT label -i 5 -l "ROOT-B" ${outdev}
  fi

  # Create the PMBR and instruct it to boot ROOT-A
  $sudo $GPT boot -i 3 -b ${pmbrcode} ${outdev}
  
  # Display what we've got
  $sudo $GPT -r show -l ${outdev}

  sync
}


# Helper function, please ignore and look below.
_partinfo() {
  local device=$1
  local partnum=$2
  local start size part x n
  sudo $GPT -r -S show $device \
    | grep 'GPT part -' \
    | while read start size part x x x n x; do \
        if [ "${part}" -eq "${partnum}" ]; then \
          echo $start $size; \
        fi; \
      done
  # The 'while' is a subshell, so there's no way to indicate success.
}

# Read GPT table to find information about a specific partition.
# Invoke as: subshell
# Args: DEVICE PARTNUM
# Returns: offset and size (in sectors) of partition PARTNUM
partinfo() {
  # get string
  local X=$(_partinfo $1 $2)
  # detect success or failure here
  [ -n "$X" ]
  echo $X
}

# Read GPT table to find the starting location of a specific partition.
# Invoke as: subshell
# Args: DEVICE PARTNUM
# Returns: offset (in sectors) of partition PARTNUM
partoffset() {
  # get string
  local X=$(_partinfo $1 $2)
  # detect success or failure here
  [ -n "$X" ]
  echo ${X% *}
}

# Read GPT table to find the size of a specific partition.
# Invoke as: subshell
# Args: DEVICE PARTNUM
# Returns: size (in sectors) of partition PARTNUM
partsize() {
  # get string
  local X=$(_partinfo $1 $2)
  # detect success or failure here
  [ -n "$X" ]
  echo ${X#* }
}

