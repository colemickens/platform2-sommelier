// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_CELLULAR_SERVICE_
#define SHILL_CELLULAR_SERVICE_

#include <map>
#include <string>

#include <base/basictypes.h>

#include "shill/cellular.h"
#include "shill/refptr_types.h"
#include "shill/service.h"

namespace shill {

class ControlInterface;
class Error;
class EventDispatcher;
class Manager;

class CellularService : public Service {
 public:
  CellularService(ControlInterface *control_interface,
                  EventDispatcher *dispatcher,
                  Manager *manager,
                  const CellularRefPtr &device);
  virtual ~CellularService();

  // Inherited from Service.
  virtual void Connect(Error *error);
  virtual void Disconnect();
  virtual void ActivateCellularModem(const std::string &carrier, Error *error);

  // cellular_<MAC>_<Service_Operator_Name>
  std::string GetStorageIdentifier(const std::string &mac);

  const std::string &activation_state() const { return activation_state_; }
  void set_activation_state(const std::string &state) {
    activation_state_ = state;
  }

  uint8 strength() const { return strength_; }
  void set_strength(uint8 strength) { strength_ = strength; }

  const std::string &payment_url() const { return payment_url_; }
  void set_payment_url(const std::string &url) { payment_url_ = url; }

  const std::string &usage_url() const { return usage_url_; }
  void set_usage_url(const std::string &url) { usage_url_ = url; }

  const Cellular::Operator &serving_operator() const;
  void set_serving_operator(const Cellular::Operator &oper);

  const std::string &network_tech() const { return network_tech_; }
  void set_network_tech(const std::string &tech) { network_tech_ = tech; }

  const std::string &roaming_state() const { return roaming_state_; }
  void set_roaming_state(const std::string &state) { roaming_state_ = state; }

 protected:
  virtual std::string CalculateState() { return "idle"; }

 private:
  static const char kServiceType[];

  virtual std::string GetDeviceRpcId();

  // Properties
  std::string activation_state_;
  Cellular::Operator serving_operator_;
  std::string network_tech_;
  std::string roaming_state_;
  std::string payment_url_;
  uint8 strength_;
  std::string usage_url_;

  std::map<std::string, std::string> apn_info_;
  std::map<std::string, std::string> last_good_apn_info_;

  CellularRefPtr cellular_;
  const std::string type_;

  DISALLOW_COPY_AND_ASSIGN(CellularService);
};

}  // namespace shill

#endif  // SHILL_CELLULAR_SERVICE_
