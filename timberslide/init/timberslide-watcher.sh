#!/bin/sh
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

LOG_PATH="$1"
RETRY=10
PID=

echo_time() {
  # The format is the same as timberslide.cc
  date +"[%m%d/%H%M%S.%N INFO] $*"
}

while [ "${RETRY}" -gt 0 ] && [ -z "${PID}" ]; do
  PID=$(initctl status timberslide LOG_PATH="${LOG_PATH}" | \
    awk 'NF>1{print $NF}')
  RETRY=$((RETRY-1))
  sleep 0.2
done

if [ -z "${PID}" ]; then
  echo_time "Could not find timberslide LOG_PATH=${LOG_PATH}, maybe stop?"
  exit 1
fi

# Check if the debugfs is deleted.
while [ -z $(lsof -p "${PID}" | grep "${LOG_PATH} (deleted)") ]; do
  # EC logs are fetched per 10 seconds, so check the status per 5 second should
  # be sufficient.
  sleep 5
done

echo_time "${LOG_PATH} is deleted, restarting timberslide"
initctl stop timberslide LOG_PATH="${LOG_PATH}"
# Ensure logs are flushed
sync
initctl start timberslide LOG_PATH="${LOG_PATH}"

# exit with 1 to let upstart restart timberslide-watcher service.
exit 1
