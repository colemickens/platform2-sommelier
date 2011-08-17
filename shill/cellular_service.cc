// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/cellular_service.h"

#include <string>

#include <base/logging.h>
#include <chromeos/dbus/service_constants.h>

#include "shill/cellular.h"

using std::string;

namespace shill {

CellularService::CellularService(ControlInterface *control_interface,
                                 EventDispatcher *dispatcher,
                                 Manager *manager,
                                 const CellularRefPtr &device)
    : Service(control_interface, dispatcher, manager),
      strength_(0),
      cellular_(device),
      type_(flimflam::kTypeCellular) {

  store_.RegisterConstString(flimflam::kActivationStateProperty,
                             &activation_state_);

  store_.RegisterStringmap(flimflam::kCellularApnProperty, &apn_info_);
  store_.RegisterConstStringmap(flimflam::kCellularLastGoodApnProperty,
                                &last_good_apn_info_);

  store_.RegisterConstString(flimflam::kNetworkTechnologyProperty,
                             &network_tech_);
  store_.RegisterConstString(flimflam::kOperatorNameProperty, &operator_name_);
  store_.RegisterConstString(flimflam::kOperatorCodeProperty, &operator_code_);
  store_.RegisterConstString(flimflam::kPaymentURLProperty, &payment_url_);
  store_.RegisterConstString(flimflam::kRoamingStateProperty, &roaming_state_);

  store_.RegisterConstUint8(flimflam::kSignalStrengthProperty, &strength_);
  store_.RegisterConstString(flimflam::kTypeProperty, &type_);
  store_.RegisterConstString(flimflam::kUsageURLProperty, &usage_url_);
}

CellularService::~CellularService() { }

void CellularService::Connect() {
  cellular_->Connect();
}

void CellularService::Disconnect() { }

void CellularService::ActivateCellularModem(const string &carrier) {
  cellular_->Activate(carrier);
}

string CellularService::GetDeviceRpcId() {
  return cellular_->GetRpcIdentifier();
}

}  // namespace shill
