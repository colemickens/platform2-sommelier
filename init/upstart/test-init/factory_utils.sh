# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

is_factory_test_mode() {
  # The path to factory enabled tag. If this path exists in a debug build,
  # we assume factory test mode.
  local factory_tag_path="/mnt/stateful_partition/dev_image/factory/enabled"
  crossystem "debug_build?1" && [ -f "${factory_tag_path}" ]
}

is_factory_installer_mode() {
  grep -wq 'cros_factory_install' /proc/cmdline || \
    [ -f /root/.factory_installer ]
}

is_factory_mode() {
  is_factory_test_mode || is_factory_installer_mode
}

factory_install_cros_payloads() {
  local script
  local scripts_root="/mnt/stateful_partition/cros_payloads/install"
  local log_file="${scripts_root}/log"
  for script in "${scripts_root}"/*.sh; do
    if [ ! -f "${script}" ]; then
      continue
    fi
    echo "$(date): Installing [${script}]..." >>"${log_file}"
    # /mnt/stateful_partition is mounted as 'noexec' so we have to invoke the
    # installer with shell explicitly.
    (cd "${scripts_root}"; sh "${script}" >>"${log_file}" 2>&1) && \
      rm -f "${script}"
  done
}

inhibit_if_factory_mode() {
  if is_factory_mode && [ "${disable_inhibit}" -eq 0 ]; then
    initctl stop --no-wait "$1"
  fi
}

# Overrides do_mount_var_and_home_chronos in chromeos_startup.
do_mount_var_and_home_chronos() {
  if is_factory_mode; then
    mount_var_and_home_chronos "factory"
  else
    mount_var_and_home_chronos
  fi
}
