#!/bin/sh

# Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

trap '' USR1 TTOU TTIN

# Disables all the Ctrl-Alt-Fn shortcuts for switching between virtual terminals
# if we're in verified boot mode. Otherwise, disables only Fn (n>=3) keys.
MAXVT=0
# If you are editing this if-condition, please also update
# platform/init/hotkey-access.conf.
if crossystem "cros_debug?1" ||
    grep -q '^chronos:[^*]' /etc/shadow; then
  # developer end-user
  MAXVT=2
fi

# The '-noreset' works around an apparent bug in the X server that
# makes us fail to boot sometimes; see http://crosbug.com/7578.
exec nice -n -20 /usr/bin/X \
    -noreset -maxvt $MAXVT -nolisten tcp vt01 -auth $1 2> /dev/null
