// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/ipconfig.h"

#include <chromeos/dbus/service_constants.h>

#include "shill/adaptor_interfaces.h"
#include "shill/control_interface.h"
#include "shill/error.h"
#include "shill/logging.h"
#include "shill/static_ip_parameters.h"
#include "shill/store_interface.h"

using base::Callback;
using std::string;

namespace shill {

// static
const char IPConfig::kStorageType[] = "Method";
// static
const char IPConfig::kType[] = "ip";
// static
uint IPConfig::global_serial_ = 0;

IPConfig::IPConfig(ControlInterface *control_interface,
                   const std::string &device_name)
    : device_name_(device_name),
      type_(kType),
      serial_(global_serial_++),
      adaptor_(control_interface->CreateIPConfigAdaptor(this)) {
  Init();
}

IPConfig::IPConfig(ControlInterface *control_interface,
                   const std::string &device_name,
                   const std::string &type)
    : device_name_(device_name),
      type_(type),
      serial_(global_serial_++),
      adaptor_(control_interface->CreateIPConfigAdaptor(this)) {
  Init();
}

void IPConfig::Init() {
  store_.RegisterConstString(flimflam::kAddressProperty,
                             &properties_.address);
  store_.RegisterConstString(flimflam::kBroadcastProperty,
                             &properties_.broadcast_address);
  store_.RegisterConstString(flimflam::kDomainNameProperty,
                             &properties_.domain_name);
  store_.RegisterConstString(flimflam::kGatewayProperty, &properties_.gateway);
  store_.RegisterConstString(flimflam::kMethodProperty, &properties_.method);
  store_.RegisterConstInt32(flimflam::kMtuProperty, &properties_.mtu);
  store_.RegisterConstStrings(flimflam::kNameServersProperty,
                              &properties_.dns_servers);
  store_.RegisterConstString(flimflam::kPeerAddressProperty,
                             &properties_.peer_address);
  store_.RegisterConstInt32(flimflam::kPrefixlenProperty,
                            &properties_.subnet_prefix);
  store_.RegisterConstStrings(shill::kSearchDomainsProperty,
                              &properties_.domain_search);
  SLOG(Inet, 2) << __func__ << " device: " << device_name();
}

IPConfig::~IPConfig() {
  SLOG(Inet, 2) << __func__ << " device: " << device_name();
}

string IPConfig::GetRpcIdentifier() {
  return adaptor_->GetRpcIdentifier();
}

string IPConfig::GetStorageIdentifier(const string &id_suffix) {
  string id = GetRpcIdentifier();
  ControlInterface::RpcIdToStorageId(&id);
  size_t needle = id.find('_');
  LOG_IF(ERROR, needle == string::npos) << "No _ in storage id?!?!";
  id.replace(id.begin() + needle + 1, id.end(), id_suffix);
  return id;
}

bool IPConfig::RequestIP() {
  return false;
}

bool IPConfig::RenewIP() {
  return false;
}

bool IPConfig::ReleaseIP() {
  return false;
}

void IPConfig::Refresh(Error */*error*/) {
  RenewIP();
}

void IPConfig::ApplyStaticIPParameters(
    StaticIPParameters *static_ip_parameters) {
  static_ip_parameters->ApplyTo(&properties_);
}

bool IPConfig::Load(StoreInterface *storage, const string &id_suffix) {
  const string id = GetStorageIdentifier(id_suffix);
  if (!storage->ContainsGroup(id)) {
    LOG(WARNING) << "IPConfig is not available in the persistent store: " << id;
    return false;
  }
  string local_type;
  storage->GetString(id, kStorageType, &local_type);
  return local_type == type();
}

bool IPConfig::Save(StoreInterface *storage, const string &id_suffix) {
  const string id = GetStorageIdentifier(id_suffix);
  storage->SetString(id, kStorageType, type());
  return true;
}

void IPConfig::UpdateProperties(const Properties &properties, bool success) {
  properties_ = properties;
  if (!update_callback_.is_null()) {
    update_callback_.Run(this, success);
  }
}

void IPConfig::RegisterUpdateCallback(
    const Callback<void(const IPConfigRefPtr&, bool)> &callback) {
  update_callback_ = callback;
}

}  // namespace shill
