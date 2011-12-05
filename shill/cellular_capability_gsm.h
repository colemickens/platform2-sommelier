// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_CELLULAR_CAPABILITY_GSM_
#define SHILL_CELLULAR_CAPABILITY_GSM_

#include <base/memory/scoped_ptr.h>
#include <base/task.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST

#include "shill/accessor_interface.h"
#include "shill/cellular.h"
#include "shill/cellular_capability.h"
#include "shill/modem_gsm_card_proxy_interface.h"
#include "shill/modem_gsm_network_proxy_interface.h"

struct mobile_provider;

namespace shill {

class CellularCapabilityGSM : public CellularCapability,
                              public ModemGSMCardProxyDelegate,
                              public ModemGSMNetworkProxyDelegate {
 public:
  static const char kPropertyUnlockRequired[];
  static const char kPropertyUnlockRetries[];

  CellularCapabilityGSM(Cellular *cellular, ProxyFactory *proxy_factory);

  // Inherited from CellularCapability.
  virtual void StartModem();
  virtual void StopModem();
  virtual void OnServiceCreated();
  virtual void UpdateStatus(const DBusPropertiesMap &properties);
  virtual void SetupConnectProperties(DBusPropertiesMap *properties);
  virtual void GetSignalQuality();
  virtual void GetRegistrationState(AsyncCallHandler *call_handler);
  virtual void GetProperties(AsyncCallHandler *call_handler);
  virtual void GetIMEI(AsyncCallHandler *call_handler);
  virtual void GetIMSI(AsyncCallHandler *call_handler);
  virtual void GetSPN(AsyncCallHandler *call_handler);
  virtual void GetMSISDN(AsyncCallHandler *call_handler);
  virtual void Register(AsyncCallHandler *call_handler);
  virtual void RegisterOnNetwork(const std::string &network_id,
                                 AsyncCallHandler *call_handler);
  virtual bool IsRegistered();
  virtual std::string CreateFriendlyServiceName();
  virtual void RequirePIN(
      const std::string &pin, bool require, AsyncCallHandler *call_handler);
  virtual void EnterPIN(const std::string &pin, AsyncCallHandler *call_handler);
  virtual void UnblockPIN(const std::string &unblock_code,
                          const std::string &pin,
                          AsyncCallHandler *call_handler);
  virtual void ChangePIN(const std::string &old_pin,
                         const std::string &new_pin,
                         AsyncCallHandler *call_handler);
  virtual void Scan(AsyncCallHandler *call_handler);
  virtual std::string GetNetworkTechnologyString() const;
  virtual std::string GetRoamingStateString() const;
  virtual void OnModemManagerPropertiesChanged(
      const DBusPropertiesMap &properties);

  const std::string &spn() const { return spn_; }
  const std::string &sim_lock_type() const {
    return sim_lock_status_.lock_type;
  }
  uint32 sim_lock_retries_left() const { return sim_lock_status_.retries_left; }

 private:
  friend class CellularTest;
  friend class CellularCapabilityGSMTest;
  FRIEND_TEST(CellularCapabilityGSMTest, CreateFriendlyServiceName);
  FRIEND_TEST(CellularCapabilityGSMTest, GetIMEI);
  FRIEND_TEST(CellularCapabilityGSMTest, GetIMSI);
  FRIEND_TEST(CellularCapabilityGSMTest, GetMSISDN);
  FRIEND_TEST(CellularCapabilityGSMTest, GetSPN);
  FRIEND_TEST(CellularCapabilityGSMTest, RequirePIN);
  FRIEND_TEST(CellularCapabilityGSMTest, EnterPIN);
  FRIEND_TEST(CellularCapabilityGSMTest, UnblockPIN);
  FRIEND_TEST(CellularCapabilityGSMTest, ChangePIN);
  FRIEND_TEST(CellularCapabilityGSMTest, InitAPNList);
  FRIEND_TEST(CellularCapabilityGSMTest, ParseScanResult);
  FRIEND_TEST(CellularCapabilityGSMTest, ParseScanResultProviderLookup);
  FRIEND_TEST(CellularCapabilityGSMTest, RegisterOnNetwork);
  FRIEND_TEST(CellularCapabilityGSMTest, Scan);
  FRIEND_TEST(CellularCapabilityGSMTest, SetAccessTechnology);
  FRIEND_TEST(CellularCapabilityGSMTest, SetHomeProvider);
  FRIEND_TEST(CellularCapabilityGSMTest, UpdateOperatorInfo);
  FRIEND_TEST(CellularCapabilityGSMTest, GetRegistrationState);

  struct SimLockStatus {
   public:
    SimLockStatus() : retries_left(0) {}
    SimLockStatus(const std::string &in_lock_type, uint32 in_retries_left)
        : lock_type(in_lock_type),
          retries_left(in_retries_left) {}

    std::string lock_type;
    uint32 retries_left;
  };

  static const char kNetworkPropertyAccessTechnology[];
  static const char kNetworkPropertyID[];
  static const char kNetworkPropertyLongName[];
  static const char kNetworkPropertyShortName[];
  static const char kNetworkPropertyStatus[];
  static const char kPhoneNumber[];
  static const char kPropertyAccessTechnology[];

  void SetAccessTechnology(uint32 access_technology);

  // Sets the upper level information about the home cellular provider from the
  // modem's IMSI and SPN.
  void SetHomeProvider();

  // Updates the GSM operator name and country based on a newly obtained network
  // id.
  void UpdateOperatorInfo();

  // Updates the serving operator on the active service.
  void UpdateServingOperator();

  // Initializes the |apn_list_| property based on the current |home_provider_|.
  void InitAPNList();

  Stringmap ParseScanResult(const GSMScanResult &result);

  StrIntPair SimLockStatusToProperty(Error *error);

  void HelpRegisterDerivedStrIntPair(
      const std::string &name,
      StrIntPair(CellularCapabilityGSM::*get)(Error *),
      void(CellularCapabilityGSM::*set)(const StrIntPair&, Error *));

  // Signal callbacks inherited from ModemGSMNetworkProxyDelegate.
  virtual void OnGSMNetworkModeChanged(uint32 mode);
  virtual void OnGSMRegistrationInfoChanged(uint32 status,
                                            const std::string &operator_code,
                                            const std::string &operator_name,
                                            const Error &error,
                                            AsyncCallHandler *call_handler);
  virtual void OnGSMSignalQualityChanged(uint32 quality);
  //
  // Method callbacks inherited from ModemGSMNetworkProxyDelegate.
  virtual void OnRegisterCallback(const Error &error,
                                  AsyncCallHandler *call_handler);
  virtual void OnGetIMEICallback(const std::string &imei,
                                 const Error &error,
                                 AsyncCallHandler *call_handler);
  virtual void OnGetIMSICallback(const std::string &imsi,
                                 const Error &error,
                                 AsyncCallHandler *call_handler);
  virtual void OnGetSPNCallback(const std::string &spn,
                                const Error &error,
                                AsyncCallHandler *call_handler);
  virtual void OnGetMSISDNCallback(const std::string &msisdn,
                                   const Error &error,
                                   AsyncCallHandler *call_handler);
  virtual void OnPINOperationCallback(const Error &error,
                                      AsyncCallHandler *call_handler);
  virtual void OnScanCallback(const GSMScanResults &results,
                              const Error &error,
                              AsyncCallHandler *call_handler);

  scoped_ptr<ModemGSMCardProxyInterface> card_proxy_;
  scoped_ptr<ModemGSMNetworkProxyInterface> network_proxy_;

  ScopedRunnableMethodFactory<CellularCapabilityGSM> task_factory_;

  uint32 registration_state_;
  uint32 access_technology_;
  Cellular::Operator serving_operator_;
  std::string spn_;
  mobile_provider *home_provider_;
  std::string desired_network_;

  // Properties.
  std::string selected_network_;
  Stringmaps found_networks_;
  bool scanning_;
  uint16 scan_interval_;
  SimLockStatus sim_lock_status_;
  Stringmaps apn_list_;

  static unsigned int friendly_service_name_id_;

  DISALLOW_COPY_AND_ASSIGN(CellularCapabilityGSM);
};

}  // namespace shill

#endif  // SHILL_CELLULAR_CAPABILITY_GSM_
