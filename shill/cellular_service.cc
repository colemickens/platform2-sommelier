// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/cellular_service.h"

#include <string>

#include <base/logging.h>
#include <chromeos/dbus/service_constants.h>

#include "shill/cellular.h"
#include "shill/control_interface.h"
#include "shill/device.h"
#include "shill/device_info.h"
#include "shill/manager.h"
#include "shill/shill_event.h"

using std::string;

namespace shill {
CellularService::CellularService(ControlInterface *control_interface,
                                 EventDispatcher *dispatcher,
                                 const CellularRefPtr &device,
                                 const string &name)
    : Service(control_interface, dispatcher, name),
      cellular_(device),
      strength_(0),
      type_(flimflam::kTypeCellular) {

  RegisterConstString(flimflam::kActivationStateProperty, &activation_state_);
  RegisterConstString(flimflam::kOperatorNameProperty, &operator_name_);
  RegisterConstString(flimflam::kOperatorCodeProperty, &operator_code_);
  RegisterConstString(flimflam::kNetworkTechnologyProperty, &network_tech_);
  RegisterConstString(flimflam::kRoamingStateProperty, &roaming_state_);
  RegisterConstString(flimflam::kPaymentURLProperty, &payment_url_);

  RegisterStringmap(flimflam::kCellularApnProperty, &apn_info_);
  RegisterConstStringmap(flimflam::kCellularLastGoodApnProperty,
                         &last_good_apn_info_);

  RegisterConstUint8(flimflam::kSignalStrengthProperty, &strength_);
  //  RegisterDerivedString(flimflam::kStateProperty,
  //                    &Service::CalculateState,
  //                    NULL);
  RegisterConstString(flimflam::kTypeProperty, &type_);
}

CellularService::~CellularService() { }

void CellularService::Connect() { }

void CellularService::Disconnect() { }

}  // namespace shill
