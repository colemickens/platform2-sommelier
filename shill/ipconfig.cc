// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/ipconfig.h"

#include <base/logging.h>
#include <chromeos/dbus/service_constants.h>

#include "shill/adaptor_interfaces.h"
#include "shill/control_interface.h"
#include "shill/error.h"

using std::string;

namespace shill {

IPConfig::IPConfig(ControlInterface *control_interface,
                   const std::string &device_name)
    : device_name_(device_name),
      adaptor_(control_interface->CreateIPConfigAdaptor(this)) {
  // Address might be R/O or not, depending on the type of IPconfig, so
  // we'll leave this up to the subclasses.
  // Register(Const?)String(flimflam::kAddressProperty, &properties_.address);
  store_.RegisterString(flimflam::kBroadcastProperty,
                        &properties_.broadcast_address);
  store_.RegisterString(flimflam::kDomainNameProperty,
                        &properties_.domain_name);
  store_.RegisterString(flimflam::kGatewayProperty, &properties_.gateway);
  store_.RegisterConstString(flimflam::kMethodProperty, &properties_.method);
  store_.RegisterInt32(flimflam::kMtuProperty, &properties_.mtu);
  store_.RegisterStrings(flimflam::kNameServersProperty,
                         &properties_.dns_servers);
  store_.RegisterString(flimflam::kPeerAddressProperty,
                        &properties_.peer_address);
  store_.RegisterInt32(flimflam::kPrefixlenProperty, &properties_.subnet_cidr);
  // TODO(cmasone): Does anyone use this?
  // store_.RegisterStrings(flimflam::kSearchDomainsProperty,
  //                        &properties_.domain_search);
  VLOG(2) << __func__ << " device: " << device_name;
}

IPConfig::~IPConfig() {
  VLOG(2) << __func__ << " device: " << device_name();
}

string IPConfig::GetRpcIdentifier() {
  return adaptor_->GetRpcIdentifier();
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

void IPConfig::UpdateProperties(const Properties &properties, bool success) {
  properties_ = properties;
  if (update_callback_.get()) {
    update_callback_->Run(this, success);
  }
}

void IPConfig::RegisterUpdateCallback(
    Callback2<const IPConfigRefPtr&, bool>::Type *callback) {
  update_callback_.reset(callback);
}

}  // namespace shill
