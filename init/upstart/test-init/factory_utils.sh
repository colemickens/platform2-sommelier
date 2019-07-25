# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

FACTORY_DIR="/mnt/stateful_partition/dev_image/factory"

is_factory_test_mode() {
  # The path to factory enabled tag. If this path exists in a debug build,
  # we assume factory test mode.
  local factory_tag_path="${FACTORY_DIR}/enabled"
  crossystem "debug_build?1" && [ -f "${factory_tag_path}" ]
}

is_factory_installer_mode() {
  grep -wq 'cros_factory_install' /proc/cmdline || \
    [ -f /root/.factory_installer ]
}

is_factory_mode() {
  is_factory_test_mode || is_factory_installer_mode
}

inhibit_if_factory_mode() {
  if is_factory_mode && [ "${disable_inhibit}" -eq 0 ]; then
    initctl stop --no-wait "$1"
  fi
}

# Overrides do_mount_var_and_home_chronos in chromeos_startup.
do_mount_var_and_home_chronos() {
  local option_file="${FACTORY_DIR}/init/encstateful_mount_option"
  local option=""

  if is_factory_mode; then
    if [ -f "${option_file}" ]; then
      option="$(cat "${option_file}")"
    else
      option="unencrypted"
    fi
  fi

  case "${option}" in
    unencrypted)
      # This should be same as platform2/init/unencrypted/startup_utils.sh
      mkdir -p /mnt/stateful_partition/var || return 1
      mount -n --bind /mnt/stateful_partition/var /var || return 1
      mount -n --bind /mnt/stateful_partition/home/chronos /home/chronos || \
        return 1
      ;;
    tmpfs)
      # Mount tmpfs to /var/.  When booting from USB disk, writing to /var/
      # slows down system performance dramatically.  Since there is no need to
      # really write to stateful partition, using option 'tmpfs' will mount
      # tmpfs on /var to improve performance.  (especially when running tests
      # like touchpad, touchscreen).
      mount -n -t tmpfs tmpfs_var /var || return 1
      mount -n --bind /mnt/stateful_partition/home/chronos /home/chronos || \
        return 1
      ;;
    *)
      # Return immediately on failure.
      mount_var_and_home_chronos || return
      ;;
  esac
}
