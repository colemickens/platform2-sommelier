# Copyright (c) 2009-2010 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Chrosh commands that are only loaded when we're booting from removable
# media.

do_install() {
  if [ "$1" ]; then
    if [ $(expr "$1" : '^/dev/[[:alnum:]]\+$') == 0 ]; then
      help "invalid device name: $1"
      return 1
    fi

    local dst="$1"
    shift

    $CHROMEOS_INSTALL --dst="$dst" "$@"
  else
    $CHROMEOS_INSTALL "$@"
  fi
}

USAGE_update_firmware='[--factory]'
HELP_update_firmware='
  Update system firmware, if applicable. This command will update only the RW
  region by default. To rewrite RO and everything, add option --factory.
'
cmd_update_firmware() (
  local CHROMEOS_FIRMWARE_UPDATE="/usr/sbin/chromeos-firmwareupdate"
  local UPDATE_OPTION="--mode=recovery"

  case "$1" in
    --factory* )
      UPDATE_OPTION="--factory"
      ;;
    "" )
      # default option.
      ;;
    * )
      help "unsupported option: $1"
      return 1
      ;;
  esac

  # The firmware updater needs same permission as chromeos-install (the command
  # invoked by do_install). chromeos-install is now running as normal user and
  # then 'sudo' internally to invoke as root, so we also use sudo here.
  # TODO(hungte) if some day we have better security jail / sandbox, change the
  # sudo to follow new magic for granting root permission.

  if [ -s $CHROMEOS_FIRMWARE_UPDATE ]; then
    sudo $CHROMEOS_FIRMWARE_UPDATE --force $UPDATE_OPTION
  else
    echo "There is no firmware update in current system."
  fi
)

USAGE_install='[<dev>]'
HELP_install='
  Install Chrome OS to the target device, clearing out all existing data.
'
cmd_install() (
  do_install "$1"
)

USAGE_upgrade='[<dev>]'
HELP_upgrade='
  Upgrade an existing Chrome OS installation, saving existing user data.
'
cmd_upgrade() (
  do_install "$1" --preserve_stateful
)
