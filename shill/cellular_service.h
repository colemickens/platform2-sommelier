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
  static const char kStorageAPN[];
  static const char kStorageLastGoodAPN[];

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
                                     Error *error,
                                     const ResultCallback &callback);
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

  // Overrride Load and Save from parent Service class.  We will call
  // the parent method.
  virtual bool Load(StoreInterface *storage);
  virtual bool Save(StoreInterface *storage);

  Stringmap *GetUserSpecifiedApn();
  Stringmap *GetLastGoodApn();
  void SetLastGoodApn(const Stringmap &apn_info);
  void ClearLastGoodApn();

 private:
  friend class CellularServiceTest;
  FRIEND_TEST(CellularCapabilityGSMTest, SetupApnTryList);
  FRIEND_TEST(CellularCapabilityTest, TryApns);
  FRIEND_TEST(CellularTest, Connect);

  void HelpRegisterDerivedStringmap(
      const std::string &name,
      Stringmap(CellularService::*get)(Error *error),
      void(CellularService::*set)(const Stringmap &value, Error *error));

  virtual std::string GetDeviceRpcId(Error *error);

  Stringmap GetApn(Error *error);
  void SetApn(const Stringmap &value, Error *error);
  static void SaveApn(StoreInterface *storage,
                      const std::string &storage_group,
                      const Stringmap *apn_info,
                      const std::string &keytag);
  static void SaveApnField(StoreInterface *storage,
                           const std::string &storage_group,
                           const Stringmap *apn_info,
                           const std::string &keytag,
                           const std::string &apntag);
  static void LoadApn(StoreInterface *storage,
                      const std::string &storage_group,
                      const std::string &keytag,
                      Stringmap *apn_info);
  static bool LoadApnField(StoreInterface *storage,
                           const std::string &storage_group,
                           const std::string &keytag,
                           const std::string &apntag,
                           Stringmap *apn_info);

  // Properties
  std::string activation_state_;
  Cellular::Operator serving_operator_;
  std::string network_technology_;
  std::string roaming_state_;
  OLP olp_;
  std::string usage_url_;

  Stringmap apn_info_;
  Stringmap last_good_apn_info_;

  std::string storage_identifier_;

  CellularRefPtr cellular_;

  DISALLOW_COPY_AND_ASSIGN(CellularService);
};

}  // namespace shill

#endif  // SHILL_CELLULAR_SERVICE_
