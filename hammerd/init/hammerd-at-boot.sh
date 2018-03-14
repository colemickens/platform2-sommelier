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

# base_connected will block for at most 10 * 0.01 = ~100ms.
base_connected() {
  # If the cros_ec_buttons driver hasn't been loaded yet, retry.
  local event_path=""
  local retries=10
  while true; do
    retries=$((retries - 1))
    event_path=`grep cros_ec_buttons /sys/class/input/event*/device/name \
        | sed -n 's|.*\(/input/event[0-9]*\)/.*|/dev\1|p' | head -n 1`
    if [ "${event_path}" != "" ]; then
      break
    fi
    if [ "${retries}" -lt 1 ]; then
      logger -t hammer-at-boot \
          "Error: cros_ec_buttons driver could not be found."
      return 1
    fi
    sleep 0.01
  done

  # Return code is 0 on base connected, 10 on disconnected.
  evtest --query "${event_path}" EV_SW SW_TABLET_MODE
}

main() {
  if ! base_connected; then
    logger -t hammer-at-boot "Base not connected, skipping hammerd at boot."
    metrics_client -e Platform.DetachableBase.AttachedOnBoot 0 2
    return
  fi

  logger -t hammerd-at-boot "Force trigger hammerd at boot."
  metrics_client -e Platform.DetachableBase.AttachedOnBoot 1 2

  # Background process that catches the DBus signal.
  monitor_dbus_signal &
  local bg_pid=$!

  # Trigger hammerd job, which blocks until the job completes.
  initctl start hammerd AT_BOOT="true" UPDATE_IF="mismatch"

  # Kill the dbus-monitor in the background process.
  pkill -9 -P "${bg_pid}" -f dbus-monitor
}

main "$@"
