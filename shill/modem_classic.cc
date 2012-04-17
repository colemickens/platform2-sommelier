// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/modem.h"

#include <mm/mm-modem.h>

#include "shill/cellular.h"

using std::string;
using std::vector;

namespace shill {

ModemClassic::ModemClassic(const std::string &owner,
                           const std::string &path,
                           ControlInterface *control_interface,
                           EventDispatcher *dispatcher,
                           Metrics *metrics,
                           Manager *manager,
                           mobile_provider_db *provider_db)
    : Modem(owner, path, control_interface, dispatcher, metrics, manager,
            provider_db) {
}

ModemClassic::~ModemClassic() {}

Cellular::ModemState ModemClassic::ConvertMmToCellularModemState(
    uint32 input) const {
  switch (input) {
    // These are defined as enums in mm-classic's private header file
    // mm-modem.h
    case 0: return Cellular::kModemStateUnknown;
    case 10: return Cellular::kModemStateDisabled;
    case 20: return Cellular::kModemStateDisabling;
    case 30: return Cellular::kModemStateEnabling;
    case 40: return Cellular::kModemStateEnabled;
    case 50: return Cellular::kModemStateSearching;
    case 60: return Cellular::kModemStateRegistered;
    case 70: return Cellular::kModemStateDisconnecting;
    case 80: return Cellular::kModemStateConnecting;
    case 90: return Cellular::kModemStateConnected;
    default:
      DCHECK(false) << "Unknown cellular state: " << input;
      return Cellular::kModemStateUnknown;
  }
}

bool ModemClassic::GetLinkName(const DBusPropertiesMap& modem_properties,
                               string *name) const {
  return DBusProperties::GetString(modem_properties, kPropertyLinkName, name);
}

void ModemClassic::CreateDeviceClassic(
    const DBusPropertiesMap &modem_properties) {
  Init();
  uint32 mm_type = kuint32max;
  DBusProperties::GetUint32(modem_properties, kPropertyType, &mm_type);
  switch (mm_type) {
    case MM_MODEM_TYPE_CDMA:
      set_type(Cellular::kTypeCDMA);
      break;
    case MM_MODEM_TYPE_GSM:
      set_type(Cellular::kTypeGSM);
      break;
    default:
      LOG(ERROR) << "Unsupported cellular modem type: " << mm_type;
      return;
  }
  uint32 ip_method = kuint32max;
  if (!DBusProperties::GetUint32(modem_properties,
                                 kPropertyIPMethod,
                                 &ip_method) ||
      ip_method != MM_MODEM_IP_METHOD_DHCP) {
    LOG(ERROR) << "Unsupported IP method: " << ip_method;
    return;
  }
  CreateDeviceFromModemProperties(modem_properties);
}

string ModemClassic::GetModemInterface(void) const {
  return string(MM_MODEM_INTERFACE);
}

}  // namespace shill
