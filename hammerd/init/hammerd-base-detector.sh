#!/bin/sh
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# The script provides utility for detecting whether a base is currently
# connected.

# base_connected will block for at most 10 * 0.01 = ~100ms.
base_connected() {
  local logger_name="$1"
  # If the cros_ec_buttons driver hasn't been loaded yet, retry.
  local event_path=""
  local retries=10
  while true; do
    : $((retries -= 1))
    event_path=$(grep cros_ec_buttons /sys/class/input/event*/device/name \
        | sed -nE 's|.*(/input/event[0-9]*)/.*|/dev\1|p' | head -n 1)
    if [ -n "${event_path}" ]; then
      break
    fi
    if [ "${retries}" -lt 1 ]; then
      logger -t "${logger_name}" \
          "Error: cros_ec_buttons driver could not be found."
      return 1
    fi
    sleep 0.01
  done

  # Return code is 0 on base connected, 10 on disconnected.
  evtest --query "${event_path}" EV_SW SW_TABLET_MODE
}
