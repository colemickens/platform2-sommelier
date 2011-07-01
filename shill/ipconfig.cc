// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/ipconfig.h"

#include <base/logging.h>
#include <base/stl_util-inl.h>
#include <chromeos/dbus/service_constants.h>

#include "shill/adaptor_interfaces.h"
#include "shill/error.h"

using std::string;

namespace shill {

IPConfig::IPConfig(const std::string &device_name) : device_name_(device_name) {
  // Address might be R/O or not, depending on the type of IPconfig, so
  // we'll leave this up to the subclasses.
  // Register(Const?)String(flimflam::kAddressProperty, &properties_.address);
  RegisterString(flimflam::kBroadcastProperty, &properties_.broadcast_address);
  RegisterString(flimflam::kDomainNameProperty, &properties_.domain_name);
  RegisterString(flimflam::kGatewayProperty, &properties_.gateway);
  RegisterConstString(flimflam::kMethodProperty, &properties_.method);
  RegisterInt32(flimflam::kMtuProperty, &properties_.mtu);
  RegisterStrings(flimflam::kNameServersProperty, &properties_.dns_servers);
  RegisterString(flimflam::kPeerAddressProperty, &properties_.peer_address);
  RegisterInt32(flimflam::kPrefixlenProperty, &properties_.subnet_cidr);
  // TODO(cmasone): Does anyone use this?
  // RegisterStrings(flimflam::kSearchDomainsProperty,
  //                 &properties_.domain_search);
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

bool IPConfig::SetInt32Property(const std::string& name,
                                int32 value,
                                Error *error) {
  VLOG(2) << "Setting " << name << " as an int32.";
  bool set = (ContainsKey(int32_properties_, name) &&
              int32_properties_[name]->Set(value));
  if (!set && error)
    error->Populate(Error::kInvalidArguments, name + " is not a R/W int32.");
  return set;
}

bool IPConfig::SetStringProperty(const string& name,
                                 const string& value,
                                 Error *error) {
  VLOG(2) << "Setting " << name << " as a string.";
  bool set = (ContainsKey(string_properties_, name) &&
              string_properties_[name]->Set(value));
  if (!set && error)
    error->Populate(Error::kInvalidArguments, name + " is not a R/W string.");
  return set;
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
