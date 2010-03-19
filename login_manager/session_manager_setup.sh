#!/bin/sh

# Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

XAUTH=/usr/bin/xauth
XAUTH_FILE="/var/run/chromelogin.auth"
SERVER_READY=

user1_handler () {
  echo "received SIGUSR1!"
  SERVER_READY=y
}

trap user1_handler USR1
MCOOKIE=$(head -c 8 /dev/urandom | openssl md5)  # speed this up?
${XAUTH} -q -f ${XAUTH_FILE} add :0 . ${MCOOKIE}

/sbin/xstart.sh ${XAUTH_FILE} &

export USER=chronos
export DATA_DIR=/home/${USER}
export LOGIN_PROFILE_DIR=${DATA_DIR}/Default
export LOGNAME=${USER}
export SHELL=/bin/bash
export HOME=${DATA_DIR}/user
export DISPLAY=:0.0
export PATH=/bin:/usr/bin:/usr/local/bin:/usr/bin/X11
export GTK_IM_MODULE=ibus

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

XAUTH_FILE=${DATA_DIR}/.Xauthority
export XAUTHORITY=${XAUTH_FILE}

mkdir -p ${DATA_DIR} && chown ${USER}:${USER} ${DATA_DIR}
mkdir -p ${HOME} && chown ${USER}:${USER} ${HOME}
${XAUTH} -q -f ${XAUTH_FILE} add :0 . ${MCOOKIE} && \
  chown ${USER}:${USER} ${XAUTH_FILE}

# Disallow the login profile from having persistent data until
# http://code.google.com/p/chromium-os/issues/detail?id=1967 is resolved.
if mount | grep -q "${LOGIN_PROFILE_DIR} "; then
  umount -f ${LOGIN_PROFILE_DIR}
fi
rm -rf ${LOGIN_PROFILE_DIR}
mkdir -p ${LOGIN_PROFILE_DIR}
mount -n -t tmpfs -onodev,noexec,nosuid loginprofile ${LOGIN_PROFILE_DIR}
chown ${USER}:${USER} ${LOGIN_PROFILE_DIR}

# temporary hack to tell cryptohome that we're doing chrome-login
touch /tmp/doing-chrome-login

CHROME_DIR="/opt/google/chrome"
CHROME="$CHROME_DIR/chrome"
COOKIE_PIPE="/tmp/cookie_pipe"
SEND_METRICS="/etc/send_metrics"
CONSENT_FILE="$DATA_DIR/Consent To Send Stats"

# xdg-open is used to open downloaded files.
# It runs sensible-browser, which uses $BROWSER.
export BROWSER=${CHROME}

USER_ID=$(/usr/bin/id -u ${USER})

SKIP_OOBE=
# For test automation.  If file exists, do not remember last username and skip
# out-of-box-experience windows except the login window
if [ -f /root/.forget_usernames ] ; then
  rm -f "${DATA_DIR}/Local State"
  SKIP_OOBE="--login-screen=login"
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

while [ -z ${SERVER_READY} ]; do
  sleep .1
done

# TODO: consider moving this when we start X in a different way.
/sbin/initctl emit x-started

exec /sbin/session_manager --uid=${USER_ID} --login -- \
    $CHROME --enable-gview \
	    --enable-sync \
            --log-level=0 \
	    --main-menu-url="http://welcome-cros.appspot.com/menu" \
            --no-first-run \
            --user-data-dir=/home/$USER \
            --login-profile=user \
            "--cookie-pipe=$COOKIE_PIPE" \
            "${SKIP_OOBE}"
