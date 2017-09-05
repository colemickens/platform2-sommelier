#!/bin/sh
# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# If the hammer is attached during boot and needs update, we block the UI at
# splash screen until the hammer is updated. This script is called by `pre-ui`
# upstart job.

monitor_dbus_signal() {
  # Monitor the hammerd updating DBus signal.
  # If we catch the "update start" signals, then show the boot message, and
  # restore the frecon after the update ends.
  local interface='org.chromium.hammerd'
  local started_signal='BaseFirmwareUpdateStarted'

  local line
  dbus-monitor --system --profile "interface='${interface}'" | \
  while read -r line; do
    if [ -z "${line##*${started_signal}*}" ]; then
      logger -t hammerd-at-boot "Hammerd starts updating, display message."
      chromeos-boot-alert update_detachable_base_firmware
    fi
  done
}

main() {
  logger -t hammerd-at-boot "Force trigger hammerd at boot."
  # Background process that catches the DBus signal.
  monitor_dbus_signal &
  local bg_pid=$!

  # Trigger hammerd job, which blocks until the job completes.
  initctl start hammerd AT_BOOT="true"

  # Kill the dbus-monitor in the background process.
  pkill -9 -P "${bg_pid}" -f dbus-monitor
}

main "$@"
