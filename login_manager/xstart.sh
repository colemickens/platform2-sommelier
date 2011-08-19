#!/bin/sh

# Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

trap '' USR1 TTOU TTIN
# The '-noreset' works around an apparent bug in the X server that
# makes us fail to boot sometimes; see http://crosbug.com/7578.
if [ -f /home/chronos/.tp_disable -a -f /tmp/synset.serio ] ; then
  SERIAL_DEVICE=`cat /tmp/synset.serio`
  SERIAL_NAME=`basename ${SERIAL_DEVICE}`
  echo -n "rescan" > /sys/devices/platform/i8042/${SERIAL_NAME}/drvctl
fi
exec /usr/bin/X -noreset -nolisten tcp vt01 -auth $1 2> /dev/null
cat /tmp/synset.serio
