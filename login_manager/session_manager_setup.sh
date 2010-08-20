#!/bin/sh

# Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Set up to start the X server ASAP, then let the startup run in the
# background while we set up other stuff.
XAUTH_FILE="/var/run/chromelogin.auth"
MCOOKIE=$(head -c 8 /dev/urandom | openssl md5)  # speed this up?
xauth -q -f ${XAUTH_FILE} add :0 . ${MCOOKIE}

# The X server sends SIGUSR1 to its parent once it's ready to accept
# connections.  The subshell here starts X, waits for the signal, then
# terminates once X is ready.
( trap 'exit 0' USR1 ; xstart.sh ${XAUTH_FILE} & wait ) &


export USER=chronos
export DATA_DIR=/home/${USER}
export LOGIN_PROFILE_DIR=${DATA_DIR}/Default
export LOGNAME=${USER}
export SHELL=/bin/sh
export HOME=${DATA_DIR}/user
export DISPLAY=:0.0
export GTK_IM_MODULE=ibus
# By default, libdbus treats all warnings as fatal errors. That's too strict.
export DBUS_FATAL_WARNINGS=0

# Forces Chrome to put its log into the home directory.
# Release versions do this already, but not Debug ones.
# Note that $CHROME_LOG and $CHROME_OLD_LOGS are defined in ui.conf
export CHROME_LOG_FILE=${CHROME_LOG}

# Clean up old chrome logs.
mkdir -p $CHROME_OLD_LOGS
chown $USER $CHROME_OLD_LOGS
mv ${CHROME_LOG}_* $CHROME_OLD_LOGS

# Forces Chrome mini dumps that are sent to the crash server to also be written
# locally.  Chrome by default will create these mini dump files in
# ~/.config/google-chrome/Crash Reports/
if [ -f /mnt/stateful_partition/etc/enable_chromium_minidumps ] ; then
  export CHROME_HEADLESS=1
  # If possible we would like to have the crash reports located somewhere else
  if [ ! -f ~/.config/google-chrome/Crash\ Reports ] ; then
    mkdir -p /mnt/stateful_partition/var/minidumps/
    chown chronos /mnt/stateful_partition/var/minidumps/
    ln -s /mnt/stateful_partition/var/minidumps/ \
      ~/.config/google-chrome/Crash\ Reports
  fi
fi

export XAUTHORITY=${DATA_DIR}/.Xauthority

mkdir -p ${DATA_DIR} && chown ${USER}:${USER} ${DATA_DIR}
mkdir -p ${HOME} && chown ${USER}:${USER} ${HOME}
xauth -q -f ${XAUTHORITY} add :0 . ${MCOOKIE} &&
  chown ${USER}:${USER} ${XAUTHORITY}

# Old builds will have a ${LOGIN_PROFILE_DIR} that's owned by root; newer ones
# won't have this directory at all.
mkdir -p ${LOGIN_PROFILE_DIR}
chown ${USER}:${USER} ${LOGIN_PROFILE_DIR}

# temporary hack to tell cryptohome that we're doing chrome-login
touch /tmp/doing-chrome-login

# TODO: Remove auto_proxy environment variable when proxy settings
# are exposed in the UI.
if [ -f /home/chronos/var/default_proxy ] ; then
  export auto_proxy=$(cat /home/chronos/var/default_proxy)
else
  if [ -f /etc/default_proxy ] ; then
    export auto_proxy=$(cat /etc/default_proxy)
  fi
fi

CHROME="/opt/google/chrome/chrome"
WM_SCRIPT="/sbin/window-manager-session.sh"
SEND_METRICS="/etc/send_metrics"
CONSENT_FILE="$DATA_DIR/Consent To Send Stats"

# xdg-open is used to open downloaded files.
# It runs sensible-browser, which uses $BROWSER.
export BROWSER=${CHROME}

USER_ID=$(id -u ${USER})

SKIP_OOBE=
# For test automation.  If file exists, do not remember last username and skip
# out-of-box-experience windows except the login window
if [ -f /root/.forget_usernames ] ; then
  rm -f "${DATA_DIR}/Local State"
  SKIP_OOBE="--login-screen=login"
fi

# For recovery image, do NOT display OOBE or login window
if [ -f /mnt/stateful_partition/.recovery ]; then
  # the magic string "test:nowindow" comes from
  # src/chrome/browser/chromeos/login/wizard_controller.cc
  SKIP_OOBE="--login-screen=test:nowindow"
fi

# Enables gathering of chrome dumps.  In stateful partition so testers
# can enable getting core dumps after build time.
if [ -f /mnt/stateful_partition/etc/enable_chromium_coredumps ] ; then
  mkdir -p /mnt/stateful_partition/var/coredumps/
  # Chrome runs and chronos so we need to change the permissions of this folder
  # so it can write there when it crashes
  chown chronos /mnt/stateful_partition/var/coredumps/
  ulimit -c unlimited
  echo "/mnt/stateful_partition/var/coredumps/core.%e.%p" > \
    /proc/sys/kernel/core_pattern
fi

if [ -f "$SEND_METRICS" ]; then
  if [ ! -f "$CONSENT_FILE" ]; then
    # Automatically opt-in to Chrome OS stats collecting.  This does
    # not have to be a cryptographically random string, but we do need
    # a 32 byte, printable string.
    head -c 8 /dev/random | openssl md5 > "$CONSENT_FILE"
  fi
fi

# We need to delete these files as Chrome may have left them around from
# its prior run (if it crashed).
rm -f ${DATA_DIR}/SingletonLock
rm -f ${DATA_DIR}/SingletonSocket

# The subshell that started the X server will terminate once X is
# ready.  Wait here for that event before continuing.
wait

# TODO: consider moving this when we start X in a different way.
initctl emit x-started
bootstat x-started

#
# Reset PATH to exclude directories unneeded by session_manager.
# Save that until here, because many of the commands above depend
# on the default PATH handed to us by init.
#
export PATH=/bin:/usr/bin:/usr/bin/X11
exec /sbin/session_manager --uid=${USER_ID} -- \
    $CHROME --login-manager \
            --enable-gview \
            --log-level=0 \
            --enable-logging \
            --no-first-run \
            --user-data-dir="$DATA_DIR" \
            --login-profile=user \
            --in-chrome-auth \
            --apps-gallery-title="Web Store" \
            --apps-gallery-url="https://chrome.google.com/extensions/" \
            --enable-login-images \
            --enable-nacl \
            --enable-tabbed-options \
            --scroll-pixels=4 \
            "${SKIP_OOBE}" \
-- "$WM_SCRIPT"
