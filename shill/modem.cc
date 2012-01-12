// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
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
using std::vector;

namespace shill {

// TODO(petkov): Consider generating these in mm/mm-modem.h.
const char Modem::kPropertyLinkName[] = "Device";
const char Modem::kPropertyIPMethod[] = "IpMethod";
const char Modem::kPropertyState[] = "State";
const char Modem::kPropertyType[] = "Type";

Modem::Modem(const std::string &owner,
             const std::string &path,
             ControlInterface *control_interface,
             EventDispatcher *dispatcher,
             Metrics *metrics,
             Manager *manager,
             mobile_provider_db *provider_db)
    : proxy_factory_(ProxyFactory::GetInstance()),
      owner_(owner),
      path_(path),
      task_factory_(this),
      control_interface_(control_interface),
      dispatcher_(dispatcher),
      metrics_(metrics),
      manager_(manager),
      provider_db_(provider_db),
      pending_device_info_(false) {
  LOG(INFO) << "Modem created: " << owner << " at " << path;
}

Modem::~Modem() {}

void Modem::Init() {
  VLOG(2) << __func__;
  CHECK(!device_.get());

  // Defer device creation because dbus-c++ doesn't allow registration of new
  // D-Bus objects in the context of a D-Bus signal handler.
  dispatcher_->PostTask(task_factory_.NewRunnableMethod(&Modem::InitTask));
}

void Modem::InitTask() {
  VLOG(2) << __func__;
  CHECK(!device_.get());

  dbus_properties_proxy_.reset(
      proxy_factory_->CreateDBusPropertiesProxy(this, path_, owner_));
  CreateDevice();
}

void Modem::OnDeviceInfoAvailable(const string &link_name) {
  VLOG(2) << __func__;
  if (pending_device_info_ && link_name_ == link_name) {
    pending_device_info_ = false;
    CreateDevice();
  }
}

void Modem::CreateDevice() {
  VLOG(2) << __func__;
  // TODO(petkov): Switch to asynchronous calls (crosbug.com/17583).
  DBusPropertiesMap properties =
      dbus_properties_proxy_->GetAll(MM_MODEM_INTERFACE);
  CreateDeviceFromProperties(properties);
}

void Modem::CreateDeviceFromProperties(const DBusPropertiesMap &properties) {
  VLOG(2) << __func__;
  if (device_.get()) {
    return;
  }

  uint32 ip_method = kuint32max;
  if (!DBusProperties::GetUint32(properties, kPropertyIPMethod, &ip_method) ||
      ip_method != MM_MODEM_IP_METHOD_DHCP) {
    LOG(ERROR) << "Unsupported IP method: " << ip_method;
    return;
  }

  if (!DBusProperties::GetString(properties, kPropertyLinkName, &link_name_)) {
    LOG(ERROR) << "Unable to create cellular device without a link name.";
    return;
  }
  // TODO(petkov): Get the interface index from DeviceInfo, similar to the MAC
  // address below.
  int interface_index =
      RTNLHandler::GetInstance()->GetInterfaceIndex(link_name_);
  if (interface_index < 0) {
    LOG(ERROR) << "Unable to create cellular device -- no interface index.";
    return;
  }

  ByteString address_bytes;
  if (!manager_->device_info()->GetMACAddress(interface_index,
                                              &address_bytes)) {
    LOG(WARNING) << "No hardware address, device creation pending device info.";
    pending_device_info_ = true;
    return;
  }

  uint32 mm_type = kuint32max;
  Cellular::Type type;
  DBusProperties::GetUint32(properties, kPropertyType, &mm_type);
  switch (mm_type) {
    case MM_MODEM_TYPE_CDMA:
      type = Cellular::kTypeCDMA;
      break;
    case MM_MODEM_TYPE_GSM:
      type = Cellular::kTypeGSM;
      break;
    default:
      LOG(ERROR) << "Unsupported cellular modem type: " << mm_type;
      return;
  }

  LOG(INFO) << "Creating a cellular device on link " << link_name_
            << " interface index " << interface_index << ".";
  device_ = new Cellular(control_interface_,
                         dispatcher_,
                         metrics_,
                         manager_,
                         link_name_,
                         address_bytes.HexEncode(),
                         interface_index,
                         type,
                         owner_,
                         path_,
                         provider_db_);

  uint32 modem_state = Cellular::kModemStateUnknown;
  DBusProperties::GetUint32(properties, kPropertyState, &modem_state);
  device_->set_modem_state(static_cast<Cellular::ModemState>(modem_state));

  // Give the device a chance to extract any capability-specific properties.
  device_->OnModemManagerPropertiesChanged(properties);

  manager_->device_info()->RegisterDevice(device_);
}

void Modem::OnDBusPropertiesChanged(
    const string &/*interface*/,
    const DBusPropertiesMap &/*changed_properties*/,
    const vector<string> &/*invalidated_properties*/) {
  // Ignored.
}

void Modem::OnModemManagerPropertiesChanged(
    const string &/*interface*/,
    const DBusPropertiesMap &properties) {
  if (device_.get()) {
    device_->OnModemManagerPropertiesChanged(properties);
  }
}

}  // namespace shill
