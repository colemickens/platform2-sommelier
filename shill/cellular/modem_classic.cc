// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/cellular/modem.h"

#include <mm/mm-modem.h>

#include "shill/cellular/cellular.h"

using std::string;
using std::vector;

namespace shill {

ModemClassic::ModemClassic(const string& service,
                           const string& path,
                           ModemInfo* modem_info,
                           ControlInterface* control_interface)
    : Modem(service, path, modem_info, control_interface) {}

ModemClassic::~ModemClassic() {}

bool ModemClassic::GetLinkName(const KeyValueStore& modem_properties,
                               string* name) const {
  if (!modem_properties.ContainsString(kPropertyLinkName)) {
    return false;
  }
  *name = modem_properties.GetString(kPropertyLinkName);
  return true;
}

void ModemClassic::CreateDeviceClassic(
    const KeyValueStore& modem_properties) {
  Init();
  uint32_t mm_type = kuint32max;
  if (modem_properties.ContainsUint(kPropertyType)) {
    mm_type = modem_properties.GetUint(kPropertyType);
  }
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
  if (!modem_properties.ContainsUint(kPropertyIPMethod) ||
      (ip_method = modem_properties.GetUint(kPropertyIPMethod)) !=
          MM_MODEM_IP_METHOD_DHCP) {
    LOG(ERROR) << "Unsupported IP method: " << ip_method;
    return;
  }

  InterfaceToProperties properties;
  properties[MM_MODEM_INTERFACE] = modem_properties;
  CreateDeviceFromModemProperties(properties);
}

string ModemClassic::GetModemInterface(void) const {
  return string(MM_MODEM_INTERFACE);
}

}  // namespace shill
