#!/bin/sh

# Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script can be called during startup to trash the stateful partition
# and possibly reset the other root filesystem

. /usr/share/misc/chromeos-common.sh
. /usr/sbin/write_gpt.sh
load_base_vars

SCRIPT="$0"

# stateful partition isn't around for logging, so dump to the screen:
set -x

: [ -x /sbin/frecon ] && ${TTY=/run/frecon/vt0} || ${TTY=/dev/tty1}

PRESERVED_TAR="/tmp/preserve.tar"
LABMACHINE=".labmachine"
STATE_PATH="/mnt/stateful_partition"
POWERWASH_COUNT="${STATE_PATH}/unencrypted/preserve/powerwash_count"

# List of files to preserve relative to /mnt/stateful_partition/
PRESERVED_FILES=""
PRESERVED_LIST=""

preserve_files() {
  # Preserve these files in safe mode. (Please request a privacy review before
  # adding files.)
  #
  # - unencrypted/preserve/update_engine/prefs/rollback-happened: Contains a
  #   boolean value indicating whether a rollback has happened since the last
  #   update check where device policy was available. Needed to avoid forced
  #   updates after rollbacks (device policy is not yet loaded at this time).
  if [ "$SAFE_WIPE" = "safe" ]; then
    PRESERVED_FILES="
      unencrypted/preserve/powerwash_count
      unencrypted/preserve/tpm_firmware_update_request
      unencrypted/preserve/update_engine/prefs/rollback-happened
      unencrypted/preserve/update_engine/prefs/rollback-version
    "

    # Preserve pre-installed demo mode resources for offline Demo Mode.
    PRESERVED_FILES="
      ${PRESERVED_FILES}
      unencrypted/cros-components/demo_mode_resources/image.squash
      unencrypted/cros-components/demo_mode_resources/imageloader.json
      unencrypted/cros-components/demo_mode_resources/imageloader.sig.2
      unencrypted/cros-components/demo_mode_resources/table
    "

    # For rollback wipes, we also preserve rollback data. This is an encrypted
    # proto which contains install attributes, device policy and owner.key (used
    # to keep the enrollment), also other device-level configurations e.g. shill
    # configuration to restore network connection after rollback.
    # We also preserve the attestation DB (needed because we don't do TPM clear
    # in this case).
    if [ "${ROLLBACK_WIPE}" = "rollback" ]; then
      ROLLBACK_FILES="
        unencrypted/preserve/attestation.epb
        unencrypted/preserve/rollback_data
      "
      PRESERVED_FILES="${PRESERVED_FILES} ${ROLLBACK_FILES}"
    fi

    # Powerwash count is only preserved for "safe" powerwashes.
    COUNT=1
    if [ -f $POWERWASH_COUNT ]; then
      COUNT_UNSANITIZED=$(head -1 $POWERWASH_COUNT | cut -c1-4)
      if [ $(expr "$COUNT_UNSANITIZED" : "^[0-9][0-9]*$") -ne 0 ]; then
        COUNT=$(( COUNT_UNSANITIZED + 1 ))
      fi
    fi
    echo $COUNT > $POWERWASH_COUNT
  fi

  # For a factory wipe (and only factory) preserve seed for CRX cache.
  if [ "$FACTORY_WIPE" = "factory" ]; then
    IMPORT_FILES="$(cd ${STATE_PATH};
                    echo unencrypted/import_extensions/extensions/*.crx)"
    if [ "$IMPORT_FILES" != \
         "unencrypted/import_extensions/extensions/*.crx" ]; then
      PRESERVED_FILES="${PRESERVED_FILES} ${IMPORT_FILES}"
    fi
  fi

  # Test images in the lab enable certain extra behaviors if the
  # .labmachine flag file is present.  Those behaviors include some
  # important recovery behaviors (cf. the recover_duts upstart job).
  # We need those behaviors to survive across power wash, otherwise,
  # the current boot could wind up as a black hole.
  if [ -f "${STATE_PATH}/${LABMACHINE}" ] &&
      crossystem 'debug_build?1'; then
    PRESERVED_FILES="${PRESERVED_FILES} ${LABMACHINE}"
  fi

  if [ -n "$PRESERVED_FILES" ]; then
    # We want to preserve permissions and recreate the directory structure
    # for all of the files in the PRESERVED_FILES variable. In order to do
    # so we run tar --no-recursion and specify the names of each of the
    # parent directories. For example for home/.shadow/install_attributes.pb
    # we pass to tar home home/.shadow home/.shadow/install_attributes.pb
    for file in $PRESERVED_FILES; do
      if [ ! -e "$STATE_PATH/$file" ]; then
        continue
      fi
      path=$file
      while [ "$path" != '.' ]; do
        PRESERVED_LIST="$path $PRESERVED_LIST"
        path=$(dirname $path)
      done
    done
    tar cf $PRESERVED_TAR -C $STATE_PATH --no-recursion -- $PRESERVED_LIST
  fi
}

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

detect_system_config() {
  # Root devs are /dev/sda3, /dev/ubiblock5_0.
  # Kernel devs to go along with these are /dev/sda2, /dev/mtd4 respectively.

  # As we move factory wiping from release image to factory test image,
  # clobber-state will be invoked directly under a tmpfs. 'rootdev' cannot
  # report correct output under such a situation. Therefore, the output of
  # 'rootdev' is preserved then assigned to environment variables
  # ${ROOT_DEV}/${ROOT_DISK} for clobber-state. For other cases, the
  # environment variables will be empty and it fallbacks to use 'rootdev'.
  if [ -z "${ROOT_DEV}" ]; then
    ROOT_DEV=$(rootdev -s)
  fi
  if [ -z "${ROOT_DISK}" ]; then
    ROOT_DISK=$(rootdev -d -s)
  fi
  IS_MTD=0
  case "${ROOT_DISK}" in
    "/dev/ubi"*)
      # Special casing for NAND devices.
      IS_MTD=1
      ROOT_DISK="/dev/mtd0"
      STATE_DEV="/dev/ubi${PARTITION_NUM_STATE}_0"
      # On NAND, kernel is stored on /dev/mtdX.
      KERNEL_DEV=$(echo "${ROOT_DEV}" | tr \
          "${PARTITION_NUM_ROOT_A}${PARTITION_NUM_ROOT_B}" \
          "${PARTITION_NUM_KERN_A}${PARTITION_NUM_KERN_B}" | \
          sed -e 's/ubiblock\([0-9]*\)_0/mtd\1/')
      ;;
    *)
      STATE_DEV=${ROOT_DEV%[0-9]*}${PARTITION_NUM_STATE}
      KERNEL_DEV=$(echo "${ROOT_DEV}" | tr \
          "${PARTITION_NUM_ROOT_A}${PARTITION_NUM_ROOT_B}" \
          "${PARTITION_NUM_KERN_A}${PARTITION_NUM_KERN_B}")
      ;;
  esac
  OTHER_ROOT_DEV=$(echo "${ROOT_DEV}" | tr \
      "${PARTITION_NUM_ROOT_A}${PARTITION_NUM_ROOT_B}" \
      "${PARTITION_NUM_ROOT_B}${PARTITION_NUM_ROOT_A}")
  OTHER_KERNEL_DEV=$(echo "${KERNEL_DEV}" | tr \
      "${PARTITION_NUM_KERN_A}${PARTITION_NUM_KERN_B}" \
      "${PARTITION_NUM_KERN_B}${PARTITION_NUM_KERN_A}")
  KERNEL_PART_NUM=${KERNEL_DEV##[/a-z]*[/a-z]}
  WIPE_PART_NUM=${OTHER_ROOT_DEV##[/a-z]*[/a-z]}
  WIPE_PART_NUM=${WIPE_PART_NUM%_0}
  # Save the option before wipe-out
  WIPE_OPTION_FILE="${STATE_PATH}/factory_wipe_option"
  if [ -O ${WIPE_OPTION_FILE} ]; then
    WIPE_OPTION=$(cat ${WIPE_OPTION_FILE})
  else
    WIPE_OPTION=
  fi

  # Discover type of device holding the stateful partition; assume SSD.
  # Since there doesn't seem to be a good way to get from a partition name
  # to the base device name beyond simple heuristics, just find the device
  # with the same major number but with minor 0.
  ROTATIONAL=0
  MAJOR=$(stat -c %t ${STATE_DEV})
  for i in $(find /dev -type b); do
    if [ "$(stat -c %t:%T $i)" = "${MAJOR}:0" ]; then
      ROTATIONAL_PATH="/sys/block/$(basename ${i})/queue/rotational"
      if [ -r "${ROTATIONAL_PATH}" ]; then
        ROTATIONAL=$(cat "${ROTATIONAL_PATH}")
        break
      fi
    fi
  done

  # Sanity check root device partition number.
  if [ "$WIPE_PART_NUM" != "${PARTITION_NUM_ROOT_A}" ] && \
     [ "$WIPE_PART_NUM" != "${PARTITION_NUM_ROOT_B}" ]
  then
    echo "Invalid partition to wipe, $WIPE_PART_NUM (${OTHER_ROOT_DEV})"
    exit 1
  fi
}

# Forces a 5 minute delay, writing progress to the specified TTY.
# This is used to prevent developer mode transitions from happening too
# quickly.
force_delay() {
  local delay=300
  ( stty -F "${TTY}" raw -echo -cread
    while [ "${delay}" -ge 0 ]; do
      printf "%2d:%02d\r" $(( delay / 60 )) $(( delay % 60 ))
      sleep 1
      : $(( delay -= 1 ))
    done
    echo
  ) >"${TTY}"
  wait
}

wipe_rotational_drive() {
  # If the stateful filesystem is available, do some best-effort content
  # shredding. Since the filesystem is not mounted with "data=journal", the
  # writes really are overwriting the block contents (unlike on an SSD).
  if grep -q " ${STATE_PATH} " /proc/mounts ; then
    (
      # Directly remove things that are already encrypted (which are also the
      # large things), or are static from images.
      rm -rf "${STATE_PATH}/encrypted.block" \
             "${STATE_PATH}/var_overlay" \
             "${STATE_PATH}/dev_image"
      find "${STATE_PATH}/home/.shadow" -maxdepth 2 -type d \
                                        -name vault -print0 |
        xargs -r0 rm -rf
      # Shred everything else. We care about contents not filenames, so do not
      # use "-u" since metadata updates via fdatasync dominate the shred time.
      # Note that if the count-down is interrupted, the reset file continues
      # to exist, which correctly continues to indicate a needed wipe.
      find "${STATE_PATH}"/. -type f -print0 | xargs -r0 shred -fz
      sync
    ) &
  fi
}

# Wipe encryption key information from the stateful partition for supported
# devices.
# Sets KEYSET_WIPE_FAILED on failure.
wipe_keysets() {
  local file
  unset KEYSET_WIPE_FAILED

  for file in \
      "${STATE_PATH}/encrypted.key" \
      "${STATE_PATH}/encrypted.needs-finalization" \
      "${STATE_PATH}/home/.shadow/cryptohome.key" \
      "${STATE_PATH}/home/.shadow/salt" \
      "${STATE_PATH}/home/.shadow/salt.sum" \
      "${STATE_PATH}"/home/.shadow/*/master.*; do
    if [ -f "${file}" ]; then
      if ! secure_erase_file "${file}"; then
        KEYSET_WIPE_FAILED="1"
        break
      fi
    fi
  done
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
  if [ -n "${PRESERVED_LIST}" ]; then
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
  preserve_files
  detect_system_config

  # Preserve the log file
  clobber-log --preserve "${SCRIPT}" "$@"

  # On a non-fast wipe, rotational drives take too long. Override to run them
  # through "fast" mode, with a forced delay. Sensitive contents should already
  # be encrypted.
  if [ "${FAST_WIPE}" != "fast" ] && [ "${ROTATIONAL}" = "1" ]; then
    wipe_rotational_drive
    force_delay
    FAST_WIPE="fast"
  fi

  # For drives that support secure erasure, wipe the keysets,
  # and then run the drives through "fast" mode, with a forced delay.
  #
  # Note: currently only eMMC-based SSDs are supported.
  if [ "${FAST_WIPE}" != "fast" ]; then
    wipe_keysets
    if [ -z "${KEYSET_WIPE_FAILED}" ]; then
      force_delay
      FAST_WIPE="fast"
    fi
  fi

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
