#!/bin/sh

# Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

XUSER="${1}"
VT="${2}"
XAUTH_FILE="${3}"

XCOOKIE=$(mcookie)
rm -f ${XAUTH_FILE} && xauth -q -f ${XAUTH_FILE} add :0 . ${XCOOKIE} &&
  chown ${XUSER}:${XUSER} ${XAUTH_FILE}

# Set permissions for logfile
mkdir -p /var/log/xorg && chown ${XUSER}:${XUSER} /var/log/xorg
ln -sf xorg/Xorg.0.log /var/log/Xorg.0.log

# Disables all the Ctrl-Alt-Fn shortcuts for switching between virtual terminals
# if we're in verified boot mode. Otherwise, disables only Fn (n>=3) keys.
MAXVT=0
if is_developer_end_user; then
  # developer end-user
  MAXVT=2
fi

# The X server sends SIGUSR1 to its parent once it's ready to accept
# connections -- but only if its own process has inherited SIG_IGN for the
# SIGUSR1 handler.  So we:
# 1. Start a shell through sudo that waits for SIGUSR1.
# 2. This shell starts another subshell which clears its own handler for (among
#    other things) SIGUSR1, then calls exec on the actual X binary.
# 3. When X is ready to accept connections, it sends SIGUSR1, which is caught
#    by the outer shell.  Note that both X and the parent shell are running
#    with the same UID through the sudo session -- this is important to allow
#    SIGUSR1 to propagate.
# 4. The script then terminates, leaving X running.

# The '-noreset' works around an apparent bug in the X server that
# makes us fail to boot sometimes; see http://crosbug.com/7578.
nice -n -20 sudo -u ${XUSER} sh -c "
  trap 'exit 0' USR1
  ( trap '' USR1 TTOU TTIN
    exec /usr/bin/X -nohwaccess -noreset -maxvt ${MAXVT} -nolisten tcp vt${VT} \
      -auth ${XAUTH_FILE} -logfile /var/log/xorg/Xorg.0.log 2> /dev/null ) &
  wait"
