// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/cellular_service.h"

#include <string>

#include <base/logging.h>
#include <base/stringprintf.h>
#include <chromeos/dbus/service_constants.h>

#include "shill/adaptor_interfaces.h"
#include "shill/cellular.h"

using std::string;

namespace shill {

// static
const char CellularService::kServiceType[] = "cellular";

// TODO(petkov): Add these to system_api/dbus/service_constants.h
namespace {
const char kKeyOLPURL[] = "url";
const char kKeyOLPMethod[] = "method";
const char kKeyOLPPostData[] = "postdata";
}  // namespace {}

CellularService::OLP::OLP() {
  SetURL("");
  SetMethod("");
  SetPostData("");
}

CellularService::OLP::~OLP() {}

void CellularService::OLP::CopyFrom(const OLP &olp) {
  dict_ = olp.dict_;
}

bool CellularService::OLP::Equals(const OLP &olp) const {
  return dict_ == olp.dict_;
}

const string &CellularService::OLP::GetURL() const {
  return dict_.find(kKeyOLPURL)->second;
}

void CellularService::OLP::SetURL(const string &url) {
  dict_[kKeyOLPURL] = url;
}

const string &CellularService::OLP::GetMethod() const {
  return dict_.find(kKeyOLPMethod)->second;
}

void CellularService::OLP::SetMethod(const string &method) {
  dict_[kKeyOLPMethod] = method;
}

const string &CellularService::OLP::GetPostData() const {
  return dict_.find(kKeyOLPPostData)->second;
}

void CellularService::OLP::SetPostData(const string &post_data) {
  dict_[kKeyOLPPostData] = post_data;
}

const Stringmap &CellularService::OLP::ToDict() const {
  return dict_;
}

CellularService::CellularService(ControlInterface *control_interface,
                                 EventDispatcher *dispatcher,
                                 Metrics *metrics,
                                 Manager *manager,
                                 const CellularRefPtr &device)
    : Service(control_interface, dispatcher, metrics, manager,
              Technology::kCellular),
      cellular_(device) {
  PropertyStore *store = this->mutable_store();
  store->RegisterConstString(flimflam::kActivationStateProperty,
                             &activation_state_);
  store->RegisterStringmap(flimflam::kCellularApnProperty, &apn_info_);
  store->RegisterConstStringmap(flimflam::kCellularLastGoodApnProperty,
                                &last_good_apn_info_);
  store->RegisterConstString(flimflam::kNetworkTechnologyProperty,
                             &network_technology_);
  store->RegisterConstStringmap(flimflam::kPaymentPortalProperty,
                                &olp_.ToDict());
  store->RegisterConstString(flimflam::kRoamingStateProperty, &roaming_state_);
  store->RegisterConstStringmap(flimflam::kServingOperatorProperty,
                                &serving_operator_.ToDict());
  store->RegisterConstString(flimflam::kUsageURLProperty, &usage_url_);

  set_friendly_name(device->CreateFriendlyServiceName());
  SetStorageIdentifier("cellular_" + device->address() + "_" + friendly_name());
}

CellularService::~CellularService() { }

void CellularService::Connect(Error *error) {
  Service::Connect(error);
  cellular_->Connect(error);
}

void CellularService::Disconnect(Error *error) {
  Service::Disconnect(error);
  cellular_->Disconnect(error);
}

void CellularService::ActivateCellularModem(const string &carrier,
                                            ReturnerInterface *returner) {
  cellular_->Activate(carrier, returner);
}

bool CellularService::TechnologyIs(const Technology::Identifier type) const {
  return cellular_->TechnologyIs(type);
}

void CellularService::SetStorageIdentifier(const string &identifier) {
  storage_identifier_ = identifier;
  std::replace_if(storage_identifier_.begin(),
                  storage_identifier_.end(),
                  &Service::IllegalChar, '_');
}

string CellularService::GetStorageIdentifier() const {
  return storage_identifier_;
}

string CellularService::GetDeviceRpcId(Error */*error*/) {
  return cellular_->GetRpcIdentifier();
}

void CellularService::SetActivationState(const string &state) {
  if (state == activation_state_) {
    return;
  }
  activation_state_ = state;
  adaptor()->EmitStringChanged(flimflam::kActivationStateProperty, state);
}

void CellularService::SetOLP(const OLP &olp) {
  if (olp_.Equals(olp)) {
    return;
  }
  olp_.CopyFrom(olp);
  adaptor()->EmitStringmapChanged(flimflam::kPaymentPortalProperty,
                                  olp.ToDict());
}

void CellularService::SetUsageURL(const std::string &url) {
  if (url == usage_url_) {
    return;
  }
  usage_url_ = url;
  adaptor()->EmitStringChanged(flimflam::kUsageURLProperty, url);
}

void CellularService::SetNetworkTechnology(const string &technology) {
  if (technology == network_technology_) {
    return;
  }
  network_technology_ = technology;
  adaptor()->EmitStringChanged(flimflam::kNetworkTechnologyProperty,
                               technology);
}

void CellularService::SetRoamingState(const string &state) {
  if (state == roaming_state_) {
    return;
  }
  roaming_state_ = state;
  adaptor()->EmitStringChanged(flimflam::kRoamingStateProperty, state);
}

const Cellular::Operator &CellularService::serving_operator() const {
  return serving_operator_;
}

void CellularService::SetServingOperator(const Cellular::Operator &oper) {
  if (serving_operator_.Equals(oper)) {
    return;
  }
  serving_operator_.CopyFrom(oper);
  adaptor()->EmitStringmapChanged(flimflam::kServingOperatorProperty,
                                  oper.ToDict());
}

}  // namespace shill
