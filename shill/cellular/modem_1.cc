// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/cellular/modem.h"

#include <ModemManager/ModemManager.h>

#include "shill/cellular/cellular.h"
#include "shill/device_info.h"

using std::string;
using std::vector;

namespace shill {

Modem1::Modem1(const string& owner,
               const string& service,
               const string& path,
               ModemInfo* modem_info)
    : Modem(owner, service, path, modem_info) {}

Modem1::~Modem1() {}

bool Modem1::GetLinkName(const DBusPropertiesMap& modem_props,
                         string* name) const {
  DBusPropertiesMap::const_iterator props_it;
  string net_port;

  props_it = modem_props.find(MM_MODEM_PROPERTY_PORTS);
  if (props_it == modem_props.end()) {
    LOG(ERROR) << "Device missing property: " << MM_MODEM_PROPERTY_PORTS;
    return false;
  }

  vector<DBus::Struct<string, uint32_t>> ports = props_it->second;
  for (const auto& port_pair : ports) {
    if (port_pair._2 == MM_MODEM_PORT_TYPE_NET) {
      net_port = port_pair._1;
      break;
    }
  }

  if (net_port.empty()) {
    LOG(ERROR) << "Could not find net port used by the device.";
    return false;
  }

  *name = net_port;
  return true;
}

void Modem1::CreateDeviceMM1(const DBusInterfaceToProperties& properties) {
  Init();
  uint32_t capabilities = kuint32max;
  DBusInterfaceToProperties::const_iterator it =
      properties.find(MM_DBUS_INTERFACE_MODEM);
  if (it == properties.end()) {
    LOG(ERROR) << "Cellular device with no modem properties";
    return;
  }
  const DBusPropertiesMap& modem_props = it->second;
  DBusProperties::GetUint32(modem_props,
                            MM_MODEM_PROPERTY_CURRENTCAPABILITIES,
                            &capabilities);

  if ((capabilities & MM_MODEM_CAPABILITY_LTE) ||
      (capabilities & MM_MODEM_CAPABILITY_GSM_UMTS)) {
    set_type(Cellular::kTypeUniversal);
  } else if (capabilities & MM_MODEM_CAPABILITY_CDMA_EVDO) {
    set_type(Cellular::kTypeUniversalCDMA);
  } else {
    LOG(ERROR) << "Unsupported capabilities: " << capabilities;
    return;
  }

  // We cannot check the IP method to make sure it's not PPP. The IP
  // method will be checked later when the bearer object is fetched.
  CreateDeviceFromModemProperties(properties);
}

string Modem1::GetModemInterface(void) const {
  return string(MM_DBUS_INTERFACE_MODEM);
}

}  // namespace shill
