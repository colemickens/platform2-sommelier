# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Utility functions for chromeos_startup to run for test images (loaded by
# dev_utils.sh).

# Load factory utilities.
. /usr/share/cros/factory_utils.sh

# Intended to be empty and may be overridden in system_key_utils.sh.
#
# This is to prevent do_mount_var_and_home_chronos() below from failing due to
# undefined create_system_key.
create_system_key() {
  :
}

# Loads encstateful system key utilities if the script exists. The script only
# exists on TPM 2.0 devices that encrypt stateful.
if [ -f /usr/share/cros/system_key_utils.sh ]; then
  . /usr/share/cros/system_key_utils.sh
fi

# Overrides do_mount_var_and_home_chronos in chromeos_startup.
do_mount_var_and_home_chronos() {
  if is_factory_mode; then
    factory_mount_var_and_home_chronos
  else
    # If this is a TPM 2.0 device that supports encrypted stateful, creates and
    # persists a system key into NVRAM and backs the key up if it doesn't exist.
    # If the call create_system_key is successful, mount_var_and_home_chronos
    # will skip the normal system key generation procedure; otherwise, it will
    # generate and persist a key via its normal workflow.
    #
    # Check system_key_utils.sh for details.
    #
    # For devices that have no TPM 2.0 chip or don't encrypted stateful, this
    # call is no-op.
    create_system_key

    mount_var_and_home_chronos
  fi
}
