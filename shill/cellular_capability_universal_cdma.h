// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_CELLULAR_CAPABILITY_UNIVERSAL_CDMA_H_
#define SHILL_CELLULAR_CAPABILITY_UNIVERSAL_CDMA_H_

#include "shill/cellular_capability_universal.h"
#include "shill/cellular.h"
#include "shill/mm1_modem_modemcdma_proxy_interface.h"

#include <base/memory/weak_ptr.h>

#include <string>
#include <vector>

#include <gtest/gtest_prod.h>  // for FRIEND_TEST

namespace shill {

class CellularCapabilityUniversalCDMA : public CellularCapabilityUniversal {
 public:
  CellularCapabilityUniversalCDMA(
      Cellular *cellular,
      ProxyFactory *proxy_factory,
      ModemInfo *modem_info);

  // Returns true if the service is activated.
  bool IsActivated() const;

  // Inherited from CellularCapability.
  virtual void Activate(const std::string &carrier,
                        Error *error, const ResultCallback &callback);
  virtual void CompleteActivation(Error *error);
  virtual std::string CreateFriendlyServiceName();
  virtual void GetProperties();
  virtual std::string GetRoamingStateString() const;
  virtual bool IsActivating() const;
  virtual bool IsRegistered();
  virtual bool IsServiceActivationRequired() const;
  virtual void OnDBusPropertiesChanged(
      const std::string &interface,
      const DBusPropertiesMap &changed_properties,
      const std::vector<std::string> &invalidated_properties);
  virtual void OnServiceCreated();
  virtual void SetUnregistered(bool searching);
  virtual void SetupConnectProperties(DBusPropertiesMap *properties);

  // TODO(armansito): Remove once 3GPP is implemented in its own class
  virtual void Register(const ResultCallback &callback);
  virtual void RegisterOnNetwork(const std::string &network_id,
                                 Error *error,
                                 const ResultCallback &callback);
  virtual void RequirePIN(const std::string &pin, bool require,
                          Error *error, const ResultCallback &callback);
  virtual void EnterPIN(const std::string &pin,
                        Error *error, const ResultCallback &callback);
  virtual void UnblockPIN(const std::string &unblock_code,
                          const std::string &pin,
                          Error *error, const ResultCallback &callback);
  virtual void ChangePIN(const std::string &old_pin,
                         const std::string &new_pin,
                         Error *error, const ResultCallback &callback);
  virtual void Scan(Error *error, const ResultCallback &callback);
  virtual void OnSimPathChanged(const std::string &sim_path);

 protected:
  virtual void InitProxies();
  virtual void ReleaseProxies();

  virtual void UpdateOLP();

  // Post-payment activation handlers.
  virtual void UpdatePendingActivationState();

  // Updates the storage identifier used for the current cellular service.
  virtual void UpdateStorageIdentifier();

 private:
  friend class CellularCapabilityUniversalCDMATest;
  FRIEND_TEST(CellularCapabilityUniversalCDMADispatcherTest,
              UpdatePendingActivationState);
  FRIEND_TEST(CellularCapabilityUniversalCDMAMainTest, ActivateAutomatic);
  FRIEND_TEST(CellularCapabilityUniversalCDMAMainTest,
              CreateFriendlyServiceName);
  FRIEND_TEST(CellularCapabilityUniversalCDMAMainTest, IsActivating);
  FRIEND_TEST(CellularCapabilityUniversalCDMAMainTest,
              IsServiceActivationRequired);
  FRIEND_TEST(CellularCapabilityUniversalCDMAMainTest,
              OnCDMARegistrationChanged);
  FRIEND_TEST(CellularCapabilityUniversalCDMAMainTest, PropertiesChanged);
  FRIEND_TEST(CellularCapabilityUniversalCDMAMainTest, UpdateOLP);
  FRIEND_TEST(CellularCapabilityUniversalCDMAMainTest, UpdateOperatorInfo);
  FRIEND_TEST(CellularCapabilityUniversalCDMAMainTest,
              UpdateServiceActivationStateProperty);
  FRIEND_TEST(CellularCapabilityUniversalCDMAMainTest,
              UpdateStorageIdentifier);

  // CDMA property change handlers
  virtual void OnModemCDMAPropertiesChanged(
      const DBusPropertiesMap &properties,
      const std::vector<std::string> &invalidated_properties);
  void OnCDMARegistrationChanged(MMModemCdmaRegistrationState state_1x,
                                 MMModemCdmaRegistrationState state_evdo,
                                 uint32_t sid, uint32_t nid);

  // CDMA activation handlers
  void ActivateAutomatic();
  void OnActivationStateChangedSignal(uint32 activation_state,
                                      uint32 activation_error,
                                      const DBusPropertiesMap &status_changes);
  void OnActivateReply(const ResultCallback &callback,
                       const Error &error);
  void HandleNewActivationStatus(uint32 error);

  void UpdateServiceActivationStateProperty();

  static std::string GetActivationStateString(uint32 state);
  static std::string GetActivationErrorString(uint32 error);

  void UpdateOperatorInfo();
  void UpdateServingOperator();

  scoped_ptr<mm1::ModemModemCdmaProxyInterface> modem_cdma_proxy_;
  // TODO(armansito): Should probably call this |weak_ptr_factory_| after
  // 3gpp refactor
  base::WeakPtrFactory<CellularCapabilityUniversalCDMA> weak_cdma_ptr_factory_;

  // CDMA ActivationState property.
  MMModemCdmaActivationState activation_state_;

  // The activation code needed for OTASP activation.
  std::string activation_code_;

  MMModemCdmaRegistrationState cdma_1x_registration_state_;
  MMModemCdmaRegistrationState cdma_evdo_registration_state_;

  // Current network operator. Since CDMA operator information is acquired
  // using the (SID, NID) pair, which itself is acquired OTA, |provider_|
  // can both be the home provider or the serving operator for roaming.
  Cellular::Operator provider_;
  uint32_t nid_;
  uint32_t sid_;

  // TODO(armansito): Use the common name |friendly_service_name_id_| once
  // CellularCapabilityUniversal gets broken up for 3GPP.
  static unsigned int friendly_service_name_id_cdma_;

  DISALLOW_COPY_AND_ASSIGN(CellularCapabilityUniversalCDMA);
};

}  // namespace shill

#endif  // SHILL_CELLULAR_CAPABILITY_UNIVERSAL_CDMA_H_
