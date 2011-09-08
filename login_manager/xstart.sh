#!/bin/sh

# Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

trap '' USR1 TTOU TTIN
# The following code checks for a marker file, and if present, identifies
# the syntp_drv serial device and disables it for testing.  The code is
# normally not run, since the file is not there, and so it does not
# proceed to the second (expensive) half of the test.
SERIAL_MARKER=/mnt/stateful_partition/var/lib/Synaptics/run/synset.serio
if [ -f /home/chronos/.tp_disable -a \
     -f $(SERIAL_MARKER) ] ; then
  # If the syntp driver is running (synset.serio exists), but should be
  # disabled (.tp_disable exists), reconfigure the i8042 driver to disable
  # the serio_raw device currently being used by syntp (/dev/serio_rawN).
  SERIAL_DEVICE=`cat $(SERIAL_MARKER)`
  SERIAL_NAME=`basename ${SERIAL_DEVICE}`
  echo -n "rescan" > /sys/devices/platform/i8042/${SERIAL_NAME}/drvctl
fi
# The '-noreset' works around an apparent bug in the X server that
# makes us fail to boot sometimes; see http://crosbug.com/7578.
exec /usr/bin/X -noreset -nolisten tcp vt01 -auth $1 2> /dev/null
