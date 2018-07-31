#!/bin/bash
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

export ENVFILE=/etc/init/arc-setup-env

# The script should always end with exec to not confuse run_oci's process
# tracking.
set -e -x
. $ENVFILE
exec /usr/sbin/arc-setup --log_to_stderr --log_tag=arc-setup-precreate \
  --mode=setup
