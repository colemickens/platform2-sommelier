// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_CELLULAR_CAPABILITY_CDMA_
#define SHILL_CELLULAR_CAPABILITY_CDMA_

#include <base/memory/scoped_ptr.h>
#include <base/task.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST

#include "shill/cellular_capability.h"
#include "shill/modem_cdma_proxy_interface.h"

namespace shill {

class CellularCapabilityCDMA : public CellularCapability,
                               public ModemCDMAProxyDelegate {

 public:
  CellularCapabilityCDMA(Cellular *cellular);

  // Inherited from CellularCapability.
  virtual void InitProxies();
  virtual void UpdateStatus(const DBusPropertiesMap &properties);
  virtual void SetupConnectProperties(DBusPropertiesMap *properties);
  virtual void Activate(const std::string &carrier, Error *error);
  virtual std::string GetNetworkTechnologyString() const;
  virtual std::string GetRoamingStateString() const;
  virtual void GetSignalQuality();
  virtual void GetRegistrationState();
  virtual void GetProperties();
  virtual void OnModemManagerPropertiesChanged(
      const DBusPropertiesMap &properties);
  virtual void OnServiceCreated();

  // Obtains the MEID.
  virtual void GetIdentifiers();

  uint32 activation_state() const { return activation_state_; }
  uint32 registration_state_evdo() const { return registration_state_evdo_; }
  uint32 registration_state_1x() const { return registration_state_1x_; }

 private:
  friend class CellularCapabilityCDMATest;
  FRIEND_TEST(CellularCapabilityCDMATest, GetActivationStateString);
  FRIEND_TEST(CellularCapabilityCDMATest, GetActivationErrorString);
  FRIEND_TEST(CellularTest, CreateService);

  static const char kPhoneNumber[];

  void ActivateTask(const std::string &carrier);

  void HandleNewActivationState(uint32 error);

  // Updates the serving operator on the active service.
  void UpdateServingOperator();

  static std::string GetActivationStateString(uint32 state);
  static std::string GetActivationErrorString(uint32 error);

  // Signal callbacks inherited from ModemCDMAProxyDelegate.
  virtual void OnCDMAActivationStateChanged(
      uint32 activation_state,
      uint32 activation_error,
      const DBusPropertiesMap &status_changes);
  virtual void OnCDMARegistrationStateChanged(uint32 state_1x,
                                              uint32 state_evdo);
  virtual void OnCDMASignalQualityChanged(uint32 strength);

  scoped_ptr<ModemCDMAProxyInterface> proxy_;

  ScopedRunnableMethodFactory<CellularCapabilityCDMA> task_factory_;

  uint32 activation_state_;
  uint32 registration_state_evdo_;
  uint32 registration_state_1x_;
  uint16 prl_version_;
  std::string payment_url_;
  std::string usage_url_;

  DISALLOW_COPY_AND_ASSIGN(CellularCapabilityCDMA);
};

}  // namespace shill

#endif  // SHILL_CELLULAR_CAPABILITY_CDMA_
