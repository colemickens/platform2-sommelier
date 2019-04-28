# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Common utilities for gathering information about modems.

#
# For modems managed by org.chromium.ModemManager
#
MM=org.chromium.ModemManager
MM_OBJECT=/org/chromium/ModemManager
MM_IMANAGER=org.freedesktop.ModemManager
MM_IMODEM=${MM_IMANAGER}.Modem
MM_IMODEM_SIMPLE=${MM_IMODEM}.Simple
MM_IMODEM_GSM=${MM_IMODEM}.Gsm
MM_IMODEM_GSM_CARD=${MM_IMODEM_GSM}.Card
MM_IMODEM_GSM_NETWORK=${MM_IMODEM_GSM}.Network
MM_IMODEM_CDMA=${MM_IMODEM}.Cdma

is_mm_modem() {
  local modem="$1"
  test $(expr match "${modem}" "${MM_OBJECT}/") -gt 0
}

#
# For modems managed by org.freedesktop.ModemManager1
#
MM1=org.freedesktop.ModemManager1
MM1_OBJECT=/org/freedesktop/ModemManager1
MM1_IMANAGER=org.freedesktop.ModemManager1
MM1_IMODEM=${MM1_IMANAGER}.Modem
MM1_IMODEM_SIMPLE=${MM1_IMODEM}.Simple
MM1_IMODEM_3GPP=${MM1_IMODEM}.Modem3gpp
MM1_IMODEM_CDMA=${MM1_IMODEM}.ModemCdma
MM1_ISIM=${MM1_IMANAGER}.Sim

mm1_modem_sim() {
  dbus_property "${MM1}" "${modem}" "${MM1_IMODEM}" Sim
}

mm1_modem_properties() {
  local modem="$1"
  local sim=$(mm1_modem_sim "${modem}")

  echo
  echo "Modem ${modem}:"
  echo "  GetStatus:"
  dbus_call "${MM1}" "${modem}" "${MM1_IMODEM_SIMPLE}.GetStatus" \
    | format_dbus_dict | indent 2
  echo "  Properties:"
  dbus_properties "${MM1}" "${modem}" "${MM1_IMODEM}" \
    | format_dbus_dict | indent 2
  echo "  3GPP:"
  dbus_properties "${MM1}" "${modem}" "${MM1_IMODEM_3GPP}" \
    | format_dbus_dict | indent 2
  echo "  CDMA:"
  dbus_properties "${MM1}" "${modem}" "${MM1_IMODEM_CDMA}" \
    | format_dbus_dict | indent 2

  if [ "${#sim}" -gt 1 ]; then
    echo "  SIM ${sim}:"
    dbus_properties "${MM1}" "${sim}" "${MM1_ISIM}" \
      | format_dbus_dict | indent 2
  fi
}

mm1_modems() {
  mmcli -L 2>/dev/null \
    | awk '/\/org\/freedesktop\/ModemManager1\/Modem\// { print $1 }'
}

is_mm1_modem() {
  local modem="$1"
  test $(expr match "${modem}" "${MM1_OBJECT}/") -gt 0
}

#
# Common stuff
#
MASKED_PROPERTIES="DeviceIdentifier|EquipmentIdentifier|OwnNumbers|ESN|MEID|IMEI|IMSI|SimIdentifier|MDN|MIN|payment_url_postdata"

mask_modem_properties() {
  sed -E "s/\<(${MASKED_PROPERTIES}): (.+)/\1: *** MASKED ***/i"
}

all_modem_status() {
  for modem in $(mm1_modems); do
    mm1_modem_properties "${modem}"
  done
}

default_modem() {
  mm1_modems | head -1
}

# Returns all modem managers that are currently running.
modem_managers() {
  dbus_call org.freedesktop.DBus /org/freedesktop/DBus \
      org.freedesktop.DBus.ListNames | awk '/ModemManager/ { print $2 }'
}

# Sets the log level of the specified modem manager.
set_modem_manager_logging() {
  local manager="$1"
  local level="$2"

  case "$manager" in
    org.chromium.ModemManager)
      dbus_call "${manager}" "${MM_OBJECT}" "${MM_IMANAGER}.SetLogging" \
        "string:${level}"
      ;;
    org.freedesktop.ModemManager1)
      if [ "${level}" = "error" ]; then
        level=err
      fi
      dbus_call "${manager}" "${MM1_OBJECT}" "${MM1_IMANAGER}.SetLogging" \
        "string:${level}"
      ;;
    *)
      return 1
      ;;
  esac
}
