// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_CELLULAR_SERVICE_
#define SHILL_CELLULAR_SERVICE_

#include <map>
#include <string>

#include <base/basictypes.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST

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
  virtual void Disconnect(Error *error);
  virtual void ActivateCellularModem(const std::string &carrier, Error *error);
  virtual bool TechnologyIs(const Technology::Identifier type) const;

  // cellular_<MAC>_<Service_Operator_Name>
  std::string GetStorageIdentifier() const;

  void SetActivationState(const std::string &state);
  const std::string &activation_state() const { return activation_state_; }

  const std::string &payment_url() const { return payment_url_; }
  void set_payment_url(const std::string &url) { payment_url_ = url; }

  const std::string &usage_url() const { return usage_url_; }
  void set_usage_url(const std::string &url) { usage_url_ = url; }

  const Cellular::Operator &serving_operator() const;
  void set_serving_operator(const Cellular::Operator &oper);

  // Sets network technology to |technology| and broadcasts the property change.
  void SetNetworkTechnology(const std::string &technology);
  const std::string &network_technology() const { return network_technology_; }

  // Sets roaming state to |state| and broadcasts the property change.
  void SetRoamingState(const std::string &state);
  const std::string &roaming_state() const { return roaming_state_; }

 private:
  friend class CellularServiceTest;
  FRIEND_TEST(CellularTest, Connect);

  static const char kServiceType[];

  virtual std::string GetDeviceRpcId(Error *error);

  // Properties
  std::string activation_state_;
  Cellular::Operator serving_operator_;
  std::string network_technology_;
  std::string roaming_state_;
  std::string payment_url_;
  std::string usage_url_;

  std::map<std::string, std::string> apn_info_;
  std::map<std::string, std::string> last_good_apn_info_;

  CellularRefPtr cellular_;

  DISALLOW_COPY_AND_ASSIGN(CellularService);
};

}  // namespace shill

#endif  // SHILL_CELLULAR_SERVICE_
