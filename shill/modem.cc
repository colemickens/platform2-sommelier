// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/modem.h"

#include <base/logging.h>
#include <mm/mm-modem.h>

#include "shill/cellular.h"
#include "shill/manager.h"
#include "shill/proxy_factory.h"
#include "shill/rtnl_handler.h"

using std::string;

namespace shill {

// TODO(petkov): Consider generating these in mm/mm-modem.h.
const char Modem::kPropertyLinkName[] = "Device";
const char Modem::kPropertyIPMethod[] = "IpMethod";
const char Modem::kPropertyType[] = "Type";
const char Modem::kPropertyUnlockRequired[] = "UnlockRequired";
const char Modem::kPropertyUnlockRetries[] = "UnlockRetries";

Modem::Modem(const std::string &owner,
             const std::string &path,
             ControlInterface *control_interface,
             EventDispatcher *dispatcher,
             Manager *manager)
    : owner_(owner),
      path_(path),
      control_interface_(control_interface),
      dispatcher_(dispatcher),
      manager_(manager) {
  LOG(INFO) << "Modem created: " << owner << " at " << path;
}

Modem::~Modem() {}

void Modem::Init() {
  CHECK(!device_.get());
  dbus_properties_proxy_.reset(
      ProxyFactory::factory()->CreateDBusPropertiesProxy(this, path_, owner_));
  // TODO(petkov): Switch to asynchronous calls (crosbug.com/17583).
  DBusPropertiesMap properties =
      dbus_properties_proxy_->GetAll(MM_MODEM_INTERFACE);
  CreateCellularDevice(properties);
}

void Modem::CreateCellularDevice(const DBusPropertiesMap &properties) {
  uint32 ip_method = kuint32max;
  if (!DBusProperties::GetUint32(properties, kPropertyIPMethod, &ip_method) ||
      ip_method != MM_MODEM_IP_METHOD_DHCP) {
    LOG(ERROR) << "Unsupported IP method: " << ip_method;
    return;
  }

  string link_name;
  if (!DBusProperties::GetString(properties, kPropertyLinkName, &link_name)) {
    LOG(ERROR) << "Unable to create cellular device without a link name.";
    return;
  }
  int interface_index =
      RTNLHandler::GetInstance()->GetInterfaceIndex(link_name);
  if (interface_index < 0) {
    LOG(ERROR) << "Unable to create cellular device -- no interface index.";
    return;
  }

  uint32 type = 0;
  DBusProperties::GetUint32(properties, kPropertyType, &type);
  // TODO(petkov): Use type.

  LOG(INFO) << "Creating a cellular device on link " << link_name
            << " interface index " << interface_index << ".";
  device_ = new Cellular(control_interface_,
                         dispatcher_,
                         manager_,
                         link_name,
                         interface_index);
  manager_->RegisterDevice(device_);

  string unlock_required;
  if (DBusProperties::GetString(
          properties, kPropertyUnlockRequired, &unlock_required)) {
    uint32 unlock_retries = 0;
    DBusProperties::GetUint32(properties,
                              kPropertyUnlockRetries,
                              &unlock_retries);
    // TODO(petkov): Set these properties on the device instance.
  }
}

}  // namespace shill
