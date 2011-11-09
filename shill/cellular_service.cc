// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/cellular_service.h"

#include <string>

#include <base/logging.h>
#include <base/stringprintf.h>
#include <chromeos/dbus/service_constants.h>

#include "shill/cellular.h"

using std::string;

namespace shill {

// static
const char CellularService::kServiceType[] = "cellular";

CellularService::CellularService(ControlInterface *control_interface,
                                 EventDispatcher *dispatcher,
                                 Manager *manager,
                                 const CellularRefPtr &device)
    : Service(control_interface, dispatcher, manager, flimflam::kTypeCellular),
      strength_(0),
      cellular_(device) {
  PropertyStore *store = this->mutable_store();
  store->RegisterConstString(flimflam::kActivationStateProperty,
                             &activation_state_);
  store->RegisterStringmap(flimflam::kCellularApnProperty, &apn_info_);
  store->RegisterConstStringmap(flimflam::kCellularLastGoodApnProperty,
                                &last_good_apn_info_);
  store->RegisterConstString(flimflam::kNetworkTechnologyProperty,
                             &network_tech_);
  store->RegisterConstString(flimflam::kPaymentURLProperty, &payment_url_);
  store->RegisterConstString(flimflam::kRoamingStateProperty, &roaming_state_);
  store->RegisterConstStringmap(flimflam::kServingOperatorProperty,
                                &serving_operator_.ToDict());
  store->RegisterConstUint8(flimflam::kSignalStrengthProperty, &strength_);
  store->RegisterConstString(flimflam::kUsageURLProperty, &usage_url_);
}

CellularService::~CellularService() { }

void CellularService::Connect(Error *error) {
  cellular_->Connect(error);
}

void CellularService::Disconnect() { }

void CellularService::ActivateCellularModem(const string &carrier,
                                            Error *error) {
  cellular_->Activate(carrier, error);
}

bool CellularService::TechnologyIs(const Technology::Identifier type) const {
  return cellular_->TechnologyIs(type);
}

string CellularService::GetStorageIdentifier() const {
  string id = base::StringPrintf("%s_%s_%s",
                                 kServiceType,
                                 cellular_->address().c_str(),
                                 serving_operator_.GetName().c_str());
  std::replace_if(id.begin(), id.end(), &Service::LegalChar, '_');
  return id;
}

string CellularService::GetDeviceRpcId(Error */*error*/) {
  return cellular_->GetRpcIdentifier();
}

const Cellular::Operator &CellularService::serving_operator() const {
  return serving_operator_;
}

void CellularService::set_serving_operator(const Cellular::Operator &oper) {
  serving_operator_.CopyFrom(oper);
}

}  // namespace shill
