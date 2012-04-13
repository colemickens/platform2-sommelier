// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/modem.h"

#include <base/bind.h>
#include <base/logging.h>

#include "shill/cellular.h"
#include "shill/manager.h"
#include "shill/proxy_factory.h"
#include "shill/rtnl_handler.h"
#include "shill/scope_logger.h"

using base::Bind;
using std::string;
using std::vector;

namespace shill {

// TODO(petkov): Consider generating these in mm/mm-modem.h.
const char Modem::kPropertyLinkName[] = "Device";
const char Modem::kPropertyIPMethod[] = "IpMethod";
const char Modem::kPropertyType[] = "Type";

Modem::Modem(const string &owner,
             const string &path,
             ControlInterface *control_interface,
             EventDispatcher *dispatcher,
             Metrics *metrics,
             Manager *manager,
             mobile_provider_db *provider_db)
    : owner_(owner),
      path_(path),
      control_interface_(control_interface),
      dispatcher_(dispatcher),
      metrics_(metrics),
      manager_(manager),
      provider_db_(provider_db),
      type_(Cellular::kTypeInvalid),
      pending_device_info_(false),
      rtnl_handler_(RTNLHandler::GetInstance()),
      proxy_factory_(ProxyFactory::GetInstance()) {
  LOG(INFO) << "Modem created: " << owner << " at " << path;
}

Modem::~Modem() {
  if (device_.get()) {
    manager_->device_info()->DeregisterDevice(device_);
  }
}

void Modem::Init() {
  dbus_properties_proxy_.reset(
      proxy_factory_->CreateDBusPropertiesProxy(this, path(), owner()));
}

void Modem::OnDeviceInfoAvailable(const string &link_name) {
  SLOG(Modem, 2) << __func__;
  if (pending_device_info_ && link_name_ == link_name) {
    // pending_device_info_ is only set if we've already been through
    // CreateDeviceFromModemProperties() and saved our initial
    // properties already
    pending_device_info_ = false;
    CreateDeviceFromModemProperties(initial_modem_properties_);
  }
}

Cellular *Modem::ConstructCellular(const string &link_name,
                                   const string &address,
                                   int interface_index) {
  LOG(INFO) << "Creating a cellular device on link " << link_name_
            << " interface index " << interface_index << ".";
  return new Cellular(control_interface_,
                      dispatcher_,
                      metrics_,
                      manager_,
                      link_name,
                      address,
                      interface_index,
                      type_,
                      owner_,
                      path_,
                      provider_db_);
}

void Modem::CreateDeviceFromModemProperties(
    const DBusPropertiesMap &modem_properties) {
  SLOG(Modem, 2) << __func__;

  if (device_.get()) {
    return;
  }
  if (!GetLinkName(modem_properties, &link_name_)) {
    LOG(ERROR) << "Unable to create cellular device without a link name.";
    return;
  }
  if (manager_->device_info()->IsDeviceBlackListed(link_name_)) {
    LOG(INFO) << "Do not create cellular device for blacklisted interface "
              << link_name_;
    return;
  }
  // TODO(petkov): Get the interface index from DeviceInfo, similar to the MAC
  // address below.
  int interface_index =
      rtnl_handler_->GetInterfaceIndex(link_name_);
  if (interface_index < 0) {
    LOG(ERROR) << "Unable to create cellular device -- no interface index.";
    return;
  }

  ByteString address_bytes;
  if (!manager_->device_info()->GetMACAddress(interface_index,
                                              &address_bytes)) {
    // Save our properties, wait for OnDeviceInfoAvailable to be called.
    LOG(WARNING) << "No hardware address, device creation pending device info.";
    initial_modem_properties_ = modem_properties;
    pending_device_info_ = true;
    return;
  }

  string address = address_bytes.HexEncode();
  device_ = ConstructCellular(link_name_, address, interface_index);

  // Give the device a chance to extract any capability-specific properties.
  device_->OnDBusPropertiesChanged(GetModemInterface(), modem_properties,
                                    vector<string>());

  manager_->device_info()->RegisterDevice(device_);
}

void Modem::OnDBusPropertiesChanged(
    const string &interface,
    const DBusPropertiesMap &changed_properties,
    const vector<string> &invalidated_properties) {
  if (device_.get()) {
    device_->OnDBusPropertiesChanged(interface,
                                      changed_properties,
                                      invalidated_properties);
  }
}

void Modem::OnModemManagerPropertiesChanged(
    const string &interface,
    const DBusPropertiesMap &properties) {
  vector<string> invalidated_properties;
  OnDBusPropertiesChanged(interface, properties, invalidated_properties);
}

}  // namespace shill
