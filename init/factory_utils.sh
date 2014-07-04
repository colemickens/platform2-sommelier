# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

is_factory_test_mode() {
  # The path to factory enabled tag. If this path exists in a debug build,
  # we assume factory test mode.
  local factory_tag_path="/mnt/stateful_partition/dev_image/factory/enabled"
  crossystem "debug_build?1" && [ -f "${factory_tag_path}" ]
}

is_factory_installer_mode() {
  [ -f /root/.factory_installer ]
}

is_factory_mode() {
  is_factory_test_mode || is_factory_installer_mode
}

inhibit_if_factory_mode() {
  if is_factory_mode && [ $disable_inhibit -eq 0 ]; then
    initctl stop --no-wait $1
  fi
}
