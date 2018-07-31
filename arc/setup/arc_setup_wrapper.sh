#!/bin/bash
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

export ENVFILE=/etc/init/arc-setup-env

bootstat mini-android-start
set -e -x
. $ENVFILE
/usr/sbin/arc-setup --log_to_stderr --log_tag=arc-setup-precreate --mode=setup
bootstat arc-setup-for-mini-android-end
exit 0
