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
using std::vector;

namespace shill {

// TODO(petkov): Consider generating these in mm/mm-modem.h.
const char Modem::kPropertyAccessTechnology[] = "AccessTechnology";
const char Modem::kPropertyLinkName[] = "Device";
const char Modem::kPropertyIPMethod[] = "IpMethod";
const char Modem::kPropertyState[] = "State";
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
      task_factory_(this),
      control_interface_(control_interface),
      dispatcher_(dispatcher),
      manager_(manager) {
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
      ProxyFactory::factory()->CreateDBusPropertiesProxy(this, path_, owner_));

  // TODO(petkov): Switch to asynchronous calls (crosbug.com/17583).
  DBusPropertiesMap properties =
      dbus_properties_proxy_->GetAll(MM_MODEM_INTERFACE);
  CreateCellularDevice(properties);
}

void Modem::CreateCellularDevice(const DBusPropertiesMap &properties) {
  VLOG(2) << __func__;
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

  ByteString address_bytes;
  if (!manager_->device_info()->GetMACAddress(interface_index,
                                              &address_bytes)) {
    // TODO(petkov): ensure that DeviceInfo has heard about this device before
    //               we go ahead and try to add it.
    LOG(ERROR) << "Unable to create cellular device without a hardware addr.";
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

  LOG(INFO) << "Creating a cellular device on link " << link_name
            << " interface index " << interface_index << ".";
  device_ = new Cellular(control_interface_,
                         dispatcher_,
                         manager_,
                         link_name,
                         address_bytes.HexEncode(),
                         interface_index,
                         type,
                         owner_,
                         path_);

  uint32 modem_state = Cellular::kModemStateUnknown;
  DBusProperties::GetUint32(properties, kPropertyState, &modem_state);
  device_->set_modem_state(static_cast<Cellular::ModemState>(modem_state));

  string unlock_required;
  if (DBusProperties::GetString(
          properties, kPropertyUnlockRequired, &unlock_required)) {
    uint32 unlock_retries = 0;
    DBusProperties::GetUint32(properties,
                              kPropertyUnlockRetries,
                              &unlock_retries);
    device_->set_sim_lock_status(
        Cellular::SimLockStatus(unlock_required, unlock_retries));
  }

  manager_->device_info()->RegisterDevice(device_);
}

void Modem::OnDBusPropertiesChanged(
    const string &interface,
    const DBusPropertiesMap &changed_properties,
    const vector<string> &invalidated_properties) {
  // Ignored.
}

void Modem::OnModemManagerPropertiesChanged(
    const string &interface,
    const DBusPropertiesMap &properties) {
  if (!device_.get()) {
    return;
  }
  Cellular::SimLockStatus lock_status = device_->sim_lock_status();
  if (DBusProperties::GetString(
          properties, kPropertyUnlockRequired, &lock_status.lock_type) ||
      DBusProperties::GetUint32(properties,
                                kPropertyUnlockRetries,
                                &lock_status.retries_left)) {
    device_->set_sim_lock_status(lock_status);
  }
  uint32 access_technology = MM_MODEM_GSM_ACCESS_TECH_UNKNOWN;
  if (DBusProperties::GetUint32(properties,
                                kPropertyAccessTechnology,
                                &access_technology)) {
    device_->SetGSMAccessTechnology(access_technology);
  }
}

}  // namespace shill
