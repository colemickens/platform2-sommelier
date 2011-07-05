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

# File to store firmware information derived from crossystem.
BIOS_INFO_FILE="/var/log/bios_info.txt"

export USER=chronos
export DATA_DIR=/home/${USER}
export LOGIN_PROFILE_DIR=${DATA_DIR}/Default
export LOGNAME=${USER}
export SHELL=/bin/sh
export HOME=${DATA_DIR}/user
export DISPLAY=:0.0
export GTK_IM_MODULE=ibus
# Change the directory for ibus-daemon socket file from ~/.config/ibus/bus/ to
# /tmp/.ibus-socket-<unique random string>/ to fix crosbug.com/16501 and 17270.
export IBUS_ADDRESS_FILE=\
"$(sudo -u ${USER} /bin/mktemp -d /tmp/.ibus-socket-XXXXXXXXXX)\
/ibus-socket-file"
# By default, libdbus treats all warnings as fatal errors. That's too strict.
export DBUS_FATAL_WARNINGS=0

# Tell Chrome where to write logging messages.
# $CHROME_LOG_DIR and $CHROME_LOG_PREFIX are defined in ui.conf,
# and the directory is created there as well.
export CHROME_LOG_FILE="${CHROME_LOG_DIR}/${CHROME_LOG_PREFIX}"

# Log directory for this session.  Note that ${HOME} might not be
# mounted until later (when the cryptohome is mounted), so we don't
# mkdir CHROMEOS_SESSION_LOG_DIR immediately.
export CHROMEOS_SESSION_LOG_DIR="${HOME}/log"

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

CHROME="/opt/google/chrome/chrome"
# Note: If this script is renamed, ChildJob::kWindowManagerSuffix needs to be
# updated to contain the new name.  See http://crosbug.com/7901 for more info.
WM_SCRIPT="/sbin/window-manager-session.sh"
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

WEBUI_LOGIN=
# If file exists, use webui login. We also are currently forcing the intial
# screen to be the login screen. This is due to WebUI OOBE not being
# completed. Once the WebUI OOBE is working we can remove this.
if [ -f /root/.use_webui_login ] ; then
  SKIP_OOBE="--login-screen=login"
  WEBUI_LOGIN="--webui-login"
fi

# For recovery image, do NOT display OOBE or login window
if [ -f /mnt/stateful_partition/.recovery ]; then
  # Verify recovery UI HTML file exists
  if [ -f /usr/share/misc/recovery_ui.html ]; then
    SKIP_OOBE="--login-screen=html file:///usr/share/misc/recovery_ui.html"
    WEBUI_LOGIN=
  else
    # Fall back to displaying a blank screen
    # the magic string "test:nowindow" comes from
    # src/chrome/browser/chromeos/login/wizard_controller.cc
    SKIP_OOBE="--login-screen=test:nowindow"
    WEBUI_LOGIN=
  fi
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

# Remove consent file if it had at one point been created by this script.
if [ -f "$CONSENT_FILE" ]; then
  CONSENT_USER_GROUP=$(stat -c %U:%G "$CONSENT_FILE")
  # normally, the consent file would be owned by "chronos:chronos".
  if [ "$CONSENT_USER_GROUP" = "root:root" ]; then
    rm -f "$CONSENT_FILE"
  fi
fi

# We need to delete these files as Chrome may have left them around from
# its prior run (if it crashed).
rm -f ${DATA_DIR}/SingletonLock
rm -f ${DATA_DIR}/SingletonSocket

# Set up bios information for chrome://system and userfeedback
# Need to do this before user can request in chrome
# moved here to keep out of critical boot path

# Function for showing switch info as reported by crossystem
#
# $1 - crossystem parameter
# $2 - string to return if the value is 0
# $3 - string to return if the value is 1
#
# if $(crossystem $1) reports something else - return 'failure'
swstate () {
  case "$(crossystem $1)" in
    (0) echo $2;;
    (1) echo $3;;
    (*) echo 'failure'
  esac
}

# Function for showing boot reason as reported by crossystem.
bootreason () {
  local reason=$(crossystem 'recovery_reason')

  echo -n "($reason): "
  case "$reason" in
    (0)   echo "$(crossystem 'mainfw_type')";;
    (1)   echo 'Legacy firmware recovery request';;
    (2)   echo 'User pressed recovery button';;
    (3)   echo 'Both RW firmware sections invalid';;
    (4)   echo 'S3 resume failed';;
    (5)   echo 'RO firmware reported TPM error';;
    (6)   echo 'Verified boot shared data initialization error';;
    (7)   echo 'S3Resume() test error';;
    (8)   echo 'LoadFirmwareSetup() test error';;
    (8)   echo 'LoadFirmware() test error';;
    (63)  echo 'Unknown RO firmware error';;
    (65)  echo 'User requested recovery at dev warning screen';;
    (66)  echo 'No valid kernel detected';;
    (67)  echo 'Kernel failed signature check';;
    (68)  echo 'RW firmware reported TPM error';;
    (69)  echo 'Developer RW firmware with the developer switch off';;
    (70)  echo 'RW firmware shared data error';;
    (71)  echo 'LoadKernel() test error';;
    (127) echo 'Unknown RW firmware error';;
    (129) echo 'DM verity failure';;
    (191) echo 'Unknown kernel error';;
    (193) echo 'Recovery mode test from user-mode';;
    (255) echo 'Unknown user mode error';;
  esac
}

if [ -x /usr/sbin/mosys ]; then
  # If a sub-command is not available on a platform, mosys will fail with
  # a non-zero exit code (EXIT_FAILURE) and print the help menu. For example,
  # this will happen if a "mosys smbios" sub-command is run on ARM since ARM
  # platforms do not support SMBIOS. If mosys fails, delete the output file to
  # avoid placing non-relevent or confusing output in /var/log.

  mosys -l smbios info bios > ${BIOS_INFO_FILE}
  if [ $? -ne 0 ]; then
    rm -f ${BIOS_INFO_FILE}
  fi

  echo "ec info since last boot" > /var/log/ec_info.txt
  mosys -l ec info >> /var/log/ec_info.txt
  if [ $? -ne 0 ]; then
    rm -f /var/log/ec_info.txt
  fi
else
  echo "version              | $(crossystem ro_fwid)" > ${BIOS_INFO_FILE}
fi

while true; do  # Use while/break to have the entire block dumped into a file.
  echo "ro bios version      | $(crossystem ro_fwid)"
  echo "Boot switch status:"
  echo "  Recovery button: $(swstate 'recoverysw_boot' 'released' 'pressed')"
  echo "  Developer mode:  $(swstate 'devsw_boot' 'not enabled' 'selected')"
  echo "  RO firmware:     $(swstate 'wpsw_boot' 'writeable' 'protected')"
  echo "Boot reason $(bootreason)"
  echo "Boot firmware: $(crossystem 'mainfw_act')"
  echo "Active EC code: $(crossystem 'ecfw_act')"
  break
done >> ${BIOS_INFO_FILE}

# Set an environment variable to prevent Flash asserts from crashing the plugin
# process.
export DONT_CRASH_ON_ASSERT=1

# Look for pepper plugins and register them
PEPPER_PATH=/opt/google/chrome/pepper
REGISTER_PLUGINS=
COMMA=
FLASH_FLAGS=
for file in $(find $PEPPER_PATH -name '*.info'); do
  FILE_NAME=
  PLUGIN_NAME=
  DESCRIPTION=
  VERSION=
  MIME_TYPES=
  . $file
  [ -z "$FILE_NAME" ] && continue
  PLUGIN_STRING="${FILE_NAME}"
  if [ -n "$PLUGIN_NAME" ]; then
    PLUGIN_STRING="${PLUGIN_STRING}#${PLUGIN_NAME}"
    if [ -n "$DESCRIPTION" ]; then
      PLUGIN_STRING="${PLUGIN_STRING}#${DESCRIPTION}"
      [ -n "$VERSION" ] && PLUGIN_STRING="${PLUGIN_STRING}#${VERSION}"
    fi
  fi
  if [ "$PLUGIN_NAME" = "Shockwave Flash" ]; then
    # Flash is treated specially.
    FLASH_FLAGS="--ppapi-flash-path=${FILE_NAME}"
    FLASH_FLAGS="${FLASH_FLAGS} --ppapi-flash-version=${VERSION}"
  else
    PLUGIN_STRING="${PLUGIN_STRING};${MIME_TYPES}"
    REGISTER_PLUGINS="${REGISTER_PLUGINS}${COMMA}${PLUGIN_STRING}"
    COMMA=","
  fi
done
if [ -n "$REGISTER_PLUGINS" ]; then
  REGISTER_PLUGINS="--register-pepper-plugins=$REGISTER_PLUGINS"
fi

TOUCH_UI_FLAGS=
if [ -f /root/.use_touchui ] ; then
  # chrome://keyboard relies on experimental APIs.
  TOUCH_UI_FLAGS="--enable-experimental-extension-apis"
  # Look to see if there are touch devices.
  TOUCH_LIST_PATH=/etc/touch-devices
  if [ -s $TOUCH_LIST_PATH ] ; then
    DEVICE_LIST="$(cat $TOUCH_LIST_PATH)"
    if [ "${DEVICE_LIST%%[,0-9]*}" = "" ] ; then
      TOUCH_UI_FLAGS="$TOUCH_UI_FLAGS --touch-devices='$DEVICE_LIST'"
    fi
  fi
fi

PKCS11_FLAGS=--load-opencryptoki

# Use OpenGL acceleration flags except on ARM
if [ "$(uname -m)" != "armv7l" ] ; then
  ACCELERATED_FLAGS="--enable-accelerated-layers"
else
  ACCELERATED_FLAGS="--use-gl=egl"
fi

# If screensaver use isn't disabled, set screensaver.
SCREENSAVERS_PATH=/usr/share/chromeos-assets/screensavers
SCREENSAVER_FLAG=
if [ -d "${SCREENSAVERS_PATH}" ]; then
  SCREENSAVER_FLAG="--screen-saver-url=\
file://${SCREENSAVERS_PATH}/default/index.htm"
fi

# Device Manager Server used to fetch the enterprise policy, if applicable.
DMSERVER="https://m.google.com/devicemanagement/data/api"

# Set up cgroups for chrome. We create two task groups, one for at most one
# foreground renderer and one for all the background renderers and set the
# background group to the lowest possible priority.
mkdir -p /tmp/cgroup/cpu
mount -t cgroup cgroup /tmp/cgroup/cpu -o cpu
mkdir /tmp/cgroup/cpu/chrome_renderers
mkdir /tmp/cgroup/cpu/chrome_renderers/foreground
mkdir /tmp/cgroup/cpu/chrome_renderers/background
echo "2" > /tmp/cgroup/cpu/chrome_renderers/background/cpu.shares
chown -R chronos /tmp/cgroup/cpu/chrome_renderers

# The subshell that started the X server will terminate once X is
# ready.  Wait here for that event before continuing.
#
# RED ALERT!  The code from the 'wait' to the end of the script is
# part of the boot time critical path.  Every millisecond spent after
# the wait is a millisecond longer till the login screen.
#
# KEEP THIS CODE PATH CLEAN!  The code must be obviously fast by
# inspection; nothing should go after the wait that isn't required
# for correctness.

wait

initctl emit x-started
bootstat x-started

# When X starts, it copies the contents of the framebuffer to the root
# window.  We clear the framebuffer here to make sure that it doesn't flash
# back onscreen when X exits later.
ply-image --clear &

#
# Reset PATH to exclude directories unneeded by session_manager.
# Save that until here, because many of the commands above depend
# on the default PATH handed to us by init.
#
export PATH=/bin:/usr/bin:/usr/bin/X11

exec /sbin/session_manager --uid=${USER_ID} -- \
    $CHROME --apps-gallery-title="Web Store" \
            --apps-gallery-url="https://chrome.google.com/webstore/" \
            --compress-sys-feedback \
            --device-management-url="$DMSERVER" \
            --disable-domui-menu \
            --disable-seccomp-sandbox \
            --enable-accelerated-plugins \
            --enable-device-policy \
            --enable-gview \
            --enable-logging \
            --enable-login-images \
            --log-level=1 \
            --login-manager \
            --login-profile=user \
            --no-first-run \
            --parallel-auth \
            --scroll-pixels=3 \
            --reload-killed-tabs \
            --user-data-dir="$DATA_DIR" \
            "$REGISTER_PLUGINS" \
            "$TOUCH_UI_FLAGS" \
            ${ACCELERATED_FLAGS} \
            ${FLASH_FLAGS} \
            ${SCREENSAVER_FLAG} \
            ${SKIP_OOBE} \
            ${WEBUI_LOGIN} \
            ${PKCS11_FLAGS} \
-- "$WM_SCRIPT"
