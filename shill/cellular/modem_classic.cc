// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/cellular/modem.h"

#include <mm/mm-modem.h>

#include "shill/cellular/cellular.h"

using std::string;
using std::vector;

namespace shill {

ModemClassic::ModemClassic(const string &owner,
                           const string &service,
                           const string &path,
                           ModemInfo *modem_info)
    : Modem(owner, service, path, modem_info) {}

ModemClassic::~ModemClassic() {}

bool ModemClassic::GetLinkName(const DBusPropertiesMap& modem_properties,
                               string *name) const {
  return DBusProperties::GetString(modem_properties, kPropertyLinkName, name);
}

void ModemClassic::CreateDeviceClassic(
    const DBusPropertiesMap &modem_properties) {
  Init();
  uint32_t mm_type = kuint32max;
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
  uint32_t ip_method = kuint32max;
  if (!DBusProperties::GetUint32(modem_properties,
                                 kPropertyIPMethod,
                                 &ip_method) ||
      ip_method != MM_MODEM_IP_METHOD_DHCP) {
    LOG(ERROR) << "Unsupported IP method: " << ip_method;
    return;
  }

  DBusInterfaceToProperties properties;
  properties[MM_MODEM_INTERFACE] = modem_properties;
  CreateDeviceFromModemProperties(properties);
}

string ModemClassic::GetModemInterface(void) const {
  return string(MM_MODEM_INTERFACE);
}

}  // namespace shill
