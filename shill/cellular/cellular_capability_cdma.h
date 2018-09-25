// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_CELLULAR_CELLULAR_CAPABILITY_CDMA_H_
#define SHILL_CELLULAR_CELLULAR_CAPABILITY_CDMA_H_

#include <memory>
#include <string>

#include <base/memory/weak_ptr.h>
#include <chromeos/dbus/service_constants.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST

#include "shill/cellular/cellular_capability.h"
#include "shill/cellular/cellular_capability_classic.h"
#include "shill/cellular/cellular_service.h"
#include "shill/cellular/modem_cdma_proxy_interface.h"

namespace shill {

class ModemInfo;

class CellularCapabilityCdma : public CellularCapabilityClassic {
 public:
  CellularCapabilityCdma(Cellular* cellular, ModemInfo* modem_info);
  ~CellularCapabilityCdma() override;

  // Inherited from CellularCapability.
  std::string GetTypeString() const override;
  void StartModem(Error* error,
                  const ResultCallback& callback) override;
  bool AreProxiesInitialized() const override;
  void Activate(const std::string& carrier,
                Error* error,
                const ResultCallback& callback) override;
  bool IsActivating() const override;
  bool IsRegistered() const override;
  void SetUnregistered(bool searching) override;
  void OnServiceCreated() override;
  std::string GetNetworkTechnologyString() const override;
  std::string GetRoamingStateString() const override;
  void GetSignalQuality() override;
  void SetupConnectProperties(KeyValueStore* properties) override;
  void DisconnectCleanup() override;

  // Inherited from CellularCapabilityClassic.
  void GetRegistrationState() override;
  void GetProperties(const ResultCallback& callback) override;

  virtual void GetMEID(const ResultCallback& callback);

  uint32_t activation_state() const { return activation_state_; }
  uint32_t registration_state_evdo() const { return registration_state_evdo_; }
  uint32_t registration_state_1x() const { return registration_state_1x_; }

 protected:
  // Inherited from CellularCapabilityClassic.
  void InitProxies() override;
  void ReleaseProxies() override;
  void UpdateStatus(const KeyValueStore& properties) override;

 private:
  friend class CellularCapabilityCdmaTest;
  friend class CellularTest;
  FRIEND_TEST(CellularCapabilityCdmaTest, Activate);
  FRIEND_TEST(CellularCapabilityCdmaTest, ActivateError);
  FRIEND_TEST(CellularCapabilityCdmaTest, GetActivationStateString);
  FRIEND_TEST(CellularCapabilityCdmaTest, GetActivationErrorString);
  FRIEND_TEST(CellularServiceTest, IsAutoConnectable);

  static const char kPhoneNumber[];

  void HandleNewActivationState(uint32_t error);

  static std::string GetActivationStateString(uint32_t state);
  static std::string GetActivationErrorString(uint32_t error);

  // Signal callbacks from the Modem.CDMA interface
  void OnActivationStateChangedSignal(
      uint32_t activation_state,
      uint32_t activation_error,
      const KeyValueStore& status_changes);
  void OnRegistrationStateChangedSignal(
      uint32_t state_1x, uint32_t state_evdo);
  void OnSignalQualitySignal(uint32_t strength);

  // Method reply callbacks from the Modem.CDMA interface
  void OnActivateReply(const ResultCallback& callback,
                       uint32_t status, const Error& error);

  void OnGetRegistrationStateReply(uint32_t state_1x, uint32_t state_evdo,
                                   const Error& error);
  void OnGetSignalQualityReply(uint32_t strength, const Error& error);

  std::unique_ptr<ModemCdmaProxyInterface> proxy_;

  // Helper method to extract the online portal information from properties.
  void UpdateOnlinePortal(const KeyValueStore& properties);
  void UpdateServiceOLP() override;

  bool activation_starting_;
  ResultCallback pending_activation_callback_;
  std::string pending_activation_carrier_;
  uint32_t activation_state_;
  uint32_t registration_state_evdo_;
  uint32_t registration_state_1x_;
  std::string usage_url_;

  base::WeakPtrFactory<CellularCapabilityCdma> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(CellularCapabilityCdma);
};

}  // namespace shill

#endif  // SHILL_CELLULAR_CELLULAR_CAPABILITY_CDMA_H_
