#!/bin/sh

# Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# ${USER} defined in /etc/init.d/session_manager
# ${DATA_DIR} defined in /etc/init.d/session_manager
export LOGNAME=${USER}
export SHELL=/bin/bash
export HOME=${DATA_DIR}/user
export DISPLAY=:0.0
export PATH=/bin:/usr/bin:/usr/local/bin:/usr/bin/X11

XAUTH_FILE=${DATA_DIR}/.Xauthority
export XAUTHORITY=${XAUTH_FILE}

mkdir -p ${HOME}
/usr/bin/xauth -q -f ${XAUTH_FILE} add :0 . $1
exec /usr/bin/chromeos-chrome-login 2>> ${DATA_DIR}/login_mgr.log

