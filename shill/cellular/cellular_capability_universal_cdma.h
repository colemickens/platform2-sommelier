// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_CELLULAR_CELLULAR_CAPABILITY_UNIVERSAL_CDMA_H_
#define SHILL_CELLULAR_CELLULAR_CAPABILITY_UNIVERSAL_CDMA_H_

#include <memory>
#include <string>
#include <vector>

#include <base/memory/weak_ptr.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST

#include "shill/cellular/cellular.h"
#include "shill/cellular/cellular_capability_universal.h"
#include "shill/cellular/mm1_modem_modemcdma_proxy_interface.h"

namespace shill {

class CellularCapabilityUniversalCdma : public CellularCapabilityUniversal {
 public:
  CellularCapabilityUniversalCdma(Cellular* cellular, ModemInfo* modem_info);
  ~CellularCapabilityUniversalCdma() override;

  // Returns true if the service is activated.
  bool IsActivated() const;

  // Inherited from CellularCapability.
  void OnPropertiesChanged(
      const std::string& interface,
      const KeyValueStore& changed_properties,
      const std::vector<std::string>& invalidated_properties) override;
  bool AreProxiesInitialized() const override;
  bool IsServiceActivationRequired() const override;
  bool IsActivating() const override;
  void CompleteActivation(Error* error) override;
  bool IsRegistered() const override;
  void SetUnregistered(bool searching) override;
  void OnServiceCreated() override;
  std::string GetRoamingStateString() const override;
  void SetupConnectProperties(KeyValueStore* properties) override;

  void RegisterOnNetwork(const std::string& network_id, Error* error,
                         const ResultCallback& callback) override;
  void RequirePIN(const std::string& pin, bool require, Error* error,
                  const ResultCallback& callback) override;
  void EnterPIN(const std::string& pin, Error* error,
                const ResultCallback& callback) override;
  void UnblockPIN(const std::string& unblock_code,
                  const std::string& pin, Error* error,
                  const ResultCallback& callback) override;
  void ChangePIN(const std::string& old_pin, const std::string& new_pin,
                 Error* error, const ResultCallback& callback) override;
  void Reset(Error* error, const ResultCallback& callback) override;
  void Scan(Error* error,
            const ResultStringmapsCallback& callback) override;
  // TODO(armansito): Remove once 3GPP is implemented in its own class
  void OnSimPathChanged(const RpcIdentifier& sim_path) override;

  void GetProperties() override;

 protected:
  // Inherited from CellularCapabilityUniversal.
  void InitProxies() override;
  void ReleaseProxies() override;
  void UpdateServiceOLP() override;

  // Post-payment activation handlers.
  void UpdatePendingActivationState() override;

 private:
  friend class CellularCapabilityUniversalCdmaTest;
  FRIEND_TEST(CellularCapabilityUniversalCdmaDispatcherTest,
              UpdatePendingActivationState);
  FRIEND_TEST(CellularCapabilityUniversalCdmaMainTest, ActivateAutomatic);
  FRIEND_TEST(CellularCapabilityUniversalCdmaMainTest, IsActivating);
  FRIEND_TEST(CellularCapabilityUniversalCdmaMainTest, IsRegistered);
  FRIEND_TEST(CellularCapabilityUniversalCdmaMainTest,
              IsServiceActivationRequired);
  FRIEND_TEST(CellularCapabilityUniversalCdmaMainTest,
              OnCdmaRegistrationChanged);
  FRIEND_TEST(CellularCapabilityUniversalCdmaMainTest, PropertiesChanged);
  FRIEND_TEST(CellularCapabilityUniversalCdmaMainTest, UpdateServiceOLP);
  FRIEND_TEST(CellularCapabilityUniversalCdmaMainTest,
              UpdateServiceActivationStateProperty);

  // CDMA property change handlers
  virtual void OnModemCdmaPropertiesChanged(
      const KeyValueStore& properties,
      const std::vector<std::string>& invalidated_properties);
  void OnCdmaRegistrationChanged(MMModemCdmaRegistrationState state_1x,
                                 MMModemCdmaRegistrationState state_evdo,
                                 uint32_t sid, uint32_t nid);

  // CDMA activation handlers
  void ActivateAutomatic();
  void OnActivationStateChangedSignal(uint32_t activation_state,
                                      uint32_t activation_error,
                                      const KeyValueStore& status_changes);
  void OnActivateReply(const ResultCallback& callback,
                       const Error& error);
  void HandleNewActivationStatus(uint32_t error);

  void UpdateServiceActivationStateProperty();

  static std::string GetActivationStateString(uint32_t state);
  static std::string GetActivationErrorString(uint32_t error);

  std::unique_ptr<mm1::ModemModemCdmaProxyInterface> modem_cdma_proxy_;

  // CDMA ActivationState property.
  MMModemCdmaActivationState activation_state_;

  MMModemCdmaRegistrationState cdma_1x_registration_state_;
  MMModemCdmaRegistrationState cdma_evdo_registration_state_;

  uint32_t nid_;
  uint32_t sid_;

  // TODO(armansito): Should probably call this |weak_ptr_factory_| after
  // 3gpp refactor
  base::WeakPtrFactory<CellularCapabilityUniversalCdma> weak_cdma_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(CellularCapabilityUniversalCdma);
};

}  // namespace shill

#endif  // SHILL_CELLULAR_CELLULAR_CAPABILITY_UNIVERSAL_CDMA_H_
