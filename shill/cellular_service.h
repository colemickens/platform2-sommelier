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
  // Online payment portal.
  class OLP {
   public:
    OLP();
    ~OLP();

    void CopyFrom(const OLP &olp);
    bool Equals(const OLP &olp) const;

    const std::string &GetURL() const;
    void SetURL(const std::string &url);

    const std::string &GetMethod() const;
    void SetMethod(const std::string &method);

    const std::string &GetPostData() const;
    void SetPostData(const std::string &post_data);

    const Stringmap &ToDict() const;

   private:
    Stringmap dict_;

    DISALLOW_COPY_AND_ASSIGN(OLP);
  };

  CellularService(ControlInterface *control_interface,
                  EventDispatcher *dispatcher,
                  Metrics *metrics,
                  Manager *manager,
                  const CellularRefPtr &device);
  virtual ~CellularService();

  // Inherited from Service.
  virtual void Connect(Error *error);
  virtual void Disconnect(Error *error);
  virtual void ActivateCellularModem(const std::string &carrier,
                                     ReturnerInterface *returner);
  virtual bool TechnologyIs(const Technology::Identifier type) const;

  virtual std::string GetStorageIdentifier() const;
  void SetStorageIdentifier(const std::string &identifier);

  void SetActivationState(const std::string &state);
  const std::string &activation_state() const { return activation_state_; }

  void SetOLP(const OLP &olp);
  const OLP &olp() const { return olp_; }

  void SetUsageURL(const std::string &url);
  const std::string &usage_url() const { return usage_url_; }

  void SetServingOperator(const Cellular::Operator &oper);
  const Cellular::Operator &serving_operator() const;

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
  OLP olp_;
  std::string usage_url_;

  std::map<std::string, std::string> apn_info_;
  std::map<std::string, std::string> last_good_apn_info_;

  std::string storage_identifier_;

  CellularRefPtr cellular_;

  DISALLOW_COPY_AND_ASSIGN(CellularService);
};

}  // namespace shill

#endif  // SHILL_CELLULAR_SERVICE_
