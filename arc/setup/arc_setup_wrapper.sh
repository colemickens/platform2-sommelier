#!/bin/bash
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

export ENVFILE=/etc/init/arc-setup-env
export LOGFILE=/var/log/arc-setup.log

exec > $LOGFILE 2>&1
bootstat mini-android-start
echo "$(date --rfc-3339=ns): arc-setup --setup"
set -e -x
. $ENVFILE
/usr/sbin/arc-setup --setup
bootstat arc-setup-for-mini-android-end
exit 0
