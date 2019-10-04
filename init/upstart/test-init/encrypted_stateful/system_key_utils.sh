# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script by default early generates a system key in test images, before the
# normal mount-encrypted run in startup_utils.sh. This allows us to soft-clear
# the TPM in integration tests w/o accidentally wiping encstateful after a
# reboot.

readonly STATEFUL_DIR="/mnt/stateful_partition"

# Flag file indicating that a system key should NOT be early generated, in which
# case mount-encrypted will generate a system key as in the production workflow.
# This flag file enables us to integration-test the production workflow of
# mount-encrypted.
readonly NO_EARLY_SYS_KEY_FILE="${STATEFUL_DIR}/.no_early_system_key"

# Backup file that stores the key material of current system key. With the
# backup, we can restore the system key in NVRAM after TPM is soft-cleared.
readonly SYS_KEY_BACKUP_FILE="${STATEFUL_DIR}/unencrypted/preserve/system.key"

readonly SYS_KEY_LOG_FILE="/run/create_system_key.log"

log_sys_key_msg() {
  echo "$1" >> "${SYS_KEY_LOG_FILE}"
}

create_system_key() {
  truncate -s 0 "${SYS_KEY_LOG_FILE}"

  if [ -f "${NO_EARLY_SYS_KEY_FILE}" ]; then
    log_sys_key_msg "Opt not to create a system key in advance."
    return
  fi

  log_sys_key_msg "Checking if a system key already exists in NVRAM..."
  if mount-encrypted info | grep "^NVRAM: available."; then
    log_sys_key_msg "There is already a system key in NVRAM."
    return
  fi

  log_sys_key_msg "No system key found in NVRAM. Start creating one."

  # Generates 32-byte random key material and backs it up.
  if ! dd if=/dev/urandom of="${SYS_KEY_BACKUP_FILE}" bs=1 count=32; then
    log_sys_key_msg "Failed to generate or back up system key material."
    return
  fi

  # Persists system key.
  if mount-encrypted set "${SYS_KEY_BACKUP_FILE}" \
      >> "${SYS_KEY_LOG_FILE}" 2>&1; then
    log_sys_key_msg "Successfully created a system key."
  fi
}
