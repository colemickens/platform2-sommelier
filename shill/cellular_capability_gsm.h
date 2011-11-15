// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_CELLULAR_CAPABILITY_GSM_
#define SHILL_CELLULAR_CAPABILITY_GSM_

#include <base/memory/scoped_ptr.h>
#include <base/task.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST

#include "shill/accessor_interface.h"
#include "shill/cellular_capability.h"
#include "shill/modem_gsm_card_proxy_interface.h"
#include "shill/modem_gsm_network_proxy_interface.h"

namespace shill {

class CellularCapabilityGSM : public CellularCapability,
                              public ModemGSMCardProxyDelegate,
                              public ModemGSMNetworkProxyDelegate {
 public:
  CellularCapabilityGSM(Cellular *cellular);

  // Inherited from CellularCapability.
  virtual void InitProxies();
  virtual void UpdateStatus(const DBusPropertiesMap &properties);
  virtual void SetupConnectProperties(DBusPropertiesMap *properties);
  virtual void GetSignalQuality();
  virtual void GetRegistrationState();
  virtual void GetProperties();
  virtual void Register();
  virtual void RegisterOnNetwork(const std::string &network_id, Error *error);
  virtual void RequirePIN(const std::string &pin, bool require, Error *error);
  virtual void EnterPIN(const std::string &pin, Error *error);
  virtual void UnblockPIN(const std::string &unblock_code,
                          const std::string &pin,
                          Error *error);
  virtual void ChangePIN(const std::string &old_pin,
                         const std::string &new_pin,
                         Error *error);
  virtual void Scan(Error *error);
  virtual std::string GetNetworkTechnologyString() const;
  virtual std::string GetRoamingStateString() const;
  virtual void OnModemManagerPropertiesChanged(
      const DBusPropertiesMap &properties);
  virtual void OnServiceCreated();

  // Obtains the IMEI, IMSI, SPN and MSISDN.
  virtual void GetIdentifiers();

  const std::string &spn() const { return spn_; }

 private:
  friend class CellularCapabilityGSMTest;
  FRIEND_TEST(CellularCapabilityGSMTest, ParseScanResult);
  FRIEND_TEST(CellularCapabilityGSMTest, ParseScanResultProviderLookup);
  FRIEND_TEST(CellularCapabilityGSMTest, RegisterOnNetwork);
  FRIEND_TEST(CellularCapabilityGSMTest, Scan);
  FRIEND_TEST(CellularCapabilityGSMTest, SetAccessTechnology);
  FRIEND_TEST(CellularCapabilityGSMTest, UpdateOperatorInfo);

  static const char kNetworkPropertyAccessTechnology[];
  static const char kNetworkPropertyID[];
  static const char kNetworkPropertyLongName[];
  static const char kNetworkPropertyShortName[];
  static const char kNetworkPropertyStatus[];
  static const char kPhoneNumber[];
  static const char kPropertyAccessTechnology[];

  void RegisterOnNetworkTask(const std::string &network_id);
  void RequirePINTask(const std::string &pin, bool require);
  void EnterPINTask(const std::string &pin);
  void UnblockPINTask(const std::string &unblock_code, const std::string &pin);
  void ChangePINTask(const std::string &old_pin, const std::string &new_pin);
  void ScanTask();

  void SetAccessTechnology(uint32 access_technology);

  // Updates the GSM operator name and country based on a newly obtained network
  // id.
  void UpdateOperatorInfo();

  // Updates the serving operator on the active service.
  void UpdateServingOperator();

  Stringmap ParseScanResult(
      const ModemGSMNetworkProxyInterface::ScanResult &result);

  // Signal callbacks inherited from ModemGSMNetworkProxyDelegate.
  virtual void OnGSMNetworkModeChanged(uint32 mode);
  virtual void OnGSMRegistrationInfoChanged(uint32 status,
                                            const std::string &operator_code,
                                            const std::string &operator_name);
  virtual void OnGSMSignalQualityChanged(uint32 quality);

  scoped_ptr<ModemGSMCardProxyInterface> card_proxy_;
  scoped_ptr<ModemGSMNetworkProxyInterface> network_proxy_;

  ScopedRunnableMethodFactory<CellularCapabilityGSM> task_factory_;

  uint32 registration_state_;
  uint32 access_technology_;
  std::string network_id_;
  std::string operator_name_;
  std::string operator_country_;
  std::string spn_;

  // Properties.
  std::string selected_network_;
  Stringmaps found_networks_;
  bool scanning_;
  uint16 scan_interval_;

  DISALLOW_COPY_AND_ASSIGN(CellularCapabilityGSM);
};

}  // namespace shill

#endif  // SHILL_CELLULAR_CAPABILITY_GSM_
