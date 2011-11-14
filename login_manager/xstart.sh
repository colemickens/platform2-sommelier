#!/bin/sh

# Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

trap '' USR1 TTOU TTIN
# The following code checks for a marker file, and if missing,
# identifies the syntp_drv serial device and disables it for
# testing. Note that we only consider the file missing if the parent
# directory exists, but the marker doesn't.
if [ -d /home/chronos -a ! -f /home/chronos/.syntp_enable ] ; then
  # serio_rawN devices come from the udev rules for the Synaptics binary
  # driver throwing the PS/2 interface into raw mode.  If we find any
  # that are TP devices, put them back into normal mode to use the default
  # driver instead.
  for P in /sys/class/misc/serio_raw*
  do
    # Note: It's OK if globbing fails, since the next line will drop us out
    udevadm info -q env -p $P | grep -q ID_INPUT_TOUCHPAD=1 || continue
    # Rescan everything; things that don't have another driver will just
    # restart in serio_rawN mode again, since they won't be looking for
    # the disable in their udev rules.
    SERIAL_DEVICE=/sys$(dirname $(dirname $(udevadm info -q path -p $P)))
    echo -n "rescan" > ${SERIAL_DEVICE}/drvctl
  done
fi
# The '-noreset' works around an apparent bug in the X server that
# makes us fail to boot sometimes; see http://crosbug.com/7578.
exec /usr/bin/X -noreset -nolisten tcp vt01 -auth $1 2> /dev/null
