# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

is_factory_mode() {
  # The path to factory enabled tag. If this path exists in a debug build,
  # we assume factory test mode.
  FACTORY_TAG_PATH="/mnt/stateful_partition/dev_image/factory/enabled"

  # Factory mode can be either:
  # 1. We are running factory tests if this is a debug build and factory
  #    enabled tag exists.
  # 2. We are running a factory installer.
  (crossystem "debug_build?1" && [ -f "$FACTORY_TAG_PATH" ]) ||
      [ -f /root/.factory_installer ]
}

inhibit_if_factory_mode() {
  if is_factory_mode && [ $disable_inhibit -eq 0 ]; then
    initctl stop --no-wait $1
  fi
}
