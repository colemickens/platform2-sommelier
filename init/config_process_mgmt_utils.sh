#!/bin/sh
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Functions for configuring process management policies in the chromiumos LSM.

# Path to the securityfs file for configuring process management security
# policies in the chromiumos LSM (used for kernel version <= 4.4).
# TODO(mortonm): Remove this and the corresponding line in
# add_process_mgmt_policy when all devices have been updated/backported to get
# the SafeSetID LSM functionality.
CHROMIUMOS_PROCESS_MGMT_POLICIES="/sys/kernel/security/chromiumos\
/process_management_policies/add_whitelist_policy"

# Path to the securityfs file for configuring process management security
# policies in the SafeSetID LSM (used for kernel version >= 4.14).
SAFESETID_PROCESS_MGMT_POLICIES="/sys/kernel/security/safesetid\
/add_whitelist_policy"

# Add process management policy.
add_process_mgmt_policy() {
  if [ -e "${CHROMIUMOS_PROCESS_MGMT_POLICIES}" ]; then
    printf "$1" > "${CHROMIUMOS_PROCESS_MGMT_POLICIES}"
  else
    printf "$1" > "${SAFESETID_PROCESS_MGMT_POLICIES}"
  fi
}

# Project-specific process management policies. Projects may add policies by
# adding a file under /usr/share/cros/startup/process_management_policies/
# whose contents are one or more lines specifying a parent UID and a child UID
# that the parent can use for the purposes of process management. There should
# be one line for every UID-to-UID mapping that is to be put in the whitelist.
# Lines in the file should use the following format:
# <UID>:<UID>
#
# For example, if the 'shill' user needs to use 'dhcp', 'openvpn' and 'ipsec'
# and 'syslog' for process management, the file would look like:
# 20104:224
# 20104:217
# 20104:212
# 20104:202
configure_process_mgmt_security() {
  local file policy
  for file in /usr/share/cros/startup/process_management_policies/*; do
    if [ -f "${file}" ]; then
      while read -r policy; do
        case "${policy}" in
        # Ignore blank lines.
        "") ;;
        # Ignore comments.
        "#"*) ;;
        *)
          add_process_mgmt_policy "${policy}"
          ;;
        esac
      done < "${file}"
    fi
  done
}
