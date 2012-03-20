// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/modem.h"

#include <mm/ModemManager-enums.h>
#include <mm/ModemManager-names.h>

#include "shill/cellular.h"

using std::string;

namespace shill {

Modem1::Modem1(const string &owner,
               const string &path,
               ControlInterface *control_interface,
               EventDispatcher *dispatcher,
               Metrics *metrics,
               Manager *manager,
               mobile_provider_db *provider_db)
    : Modem(owner, path, control_interface, dispatcher, metrics, manager,
            provider_db) {
}

Modem1::~Modem1() {}

Cellular::ModemState Modem1::ConvertMmToCellularModemState(uint32 input) const {
  switch (input) {
    case MM_MODEM_STATE_UNKNOWN: return Cellular::kModemStateUnknown;
    case MM_MODEM_STATE_INITIALIZING: return Cellular::kModemStateInitializing;
    case MM_MODEM_STATE_LOCKED: return Cellular::kModemStateLocked;
    case MM_MODEM_STATE_DISABLED: return Cellular::kModemStateDisabled;
    case MM_MODEM_STATE_DISABLING: return Cellular::kModemStateDisabling;
    case MM_MODEM_STATE_ENABLING: return Cellular::kModemStateEnabling;
    case MM_MODEM_STATE_ENABLED: return Cellular::kModemStateEnabled;
    case MM_MODEM_STATE_SEARCHING: return Cellular::kModemStateSearching;
    case MM_MODEM_STATE_REGISTERED: return Cellular::kModemStateRegistered;
    case MM_MODEM_STATE_DISCONNECTING:
      return Cellular::kModemStateDisconnecting;
    case MM_MODEM_STATE_CONNECTING: return Cellular::kModemStateConnecting;
    case MM_MODEM_STATE_CONNECTED: return Cellular::kModemStateConnected;
    default:
      DCHECK(false) << "Unknown cellular state: " << input;
      return Cellular::kModemStateUnknown;
  }
}

bool Modem1::GetLinkName(const DBusPropertiesMap & /* modem_props */,
                         string *name) const {
  // TODO(rochberg): use the device path to find the link name in
  // sysfs.  crosbug.com/28498
  *name = "usb0";
  return true;
}

void Modem1::CreateDeviceMM1(const DBusInterfaceToProperties &i_to_p) {
  DBusInterfaceToProperties::const_iterator modem_properties =
      i_to_p.find(MM_DBUS_INTERFACE_MODEM);
  if (modem_properties == i_to_p.end()) {
    LOG(ERROR) << "Cellular device with no modem properties";
    return;
  }
  set_type(Cellular::kTypeUniversal);

  // We cannot check the IP method to make sure it's not PPP. The IP
  // method will be checked later when the bearer object is fetched.
  CreateDeviceFromModemProperties(modem_properties->second);
}

}  // namespace shill
