// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_CELLULAR_CAPABILITY_GSM_
#define SHILL_CELLULAR_CAPABILITY_GSM_

#include <deque>
#include <string>
#include <vector>

#include <base/memory/scoped_ptr.h>
#include <base/memory/weak_ptr.h>
#include <chromeos/dbus/service_constants.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST

#include "shill/accessor_interface.h"
#include "shill/cellular.h"
#include "shill/cellular_capability.h"
#include "shill/cellular_capability_classic.h"
#include "shill/modem_gsm_card_proxy_interface.h"
#include "shill/modem_gsm_network_proxy_interface.h"

struct mobile_provider;

namespace shill {

class CellularCapabilityGSM : public CellularCapabilityClassic {
 public:
  CellularCapabilityGSM(Cellular *cellular, ProxyFactory *proxy_factory);

  // Inherited from CellularCapability.
  virtual void StartModem(Error *error, const ResultCallback &callback);
  virtual void OnServiceCreated();
  virtual void SetupConnectProperties(DBusPropertiesMap *properties);
  virtual void Connect(const DBusPropertiesMap &properties, Error *error,
                       const ResultCallback &callback);
  // The following two methods never need to report results
  // via callbacks, which is why they don't take callbacks
  // as arguments.
  virtual void GetSignalQuality();
  virtual void GetRegistrationState();
  // The following six methods are only ever called as
  // callbacks (from the main loop), which is why they
  // don't take an Error * argument.
  virtual void GetProperties(const ResultCallback &callback);
  virtual void GetIMEI(const ResultCallback &callback);
  virtual void GetIMSI(const ResultCallback &callback);
  virtual void GetSPN(const ResultCallback &callback);
  virtual void GetMSISDN(const ResultCallback &callback);
  virtual void Register(const ResultCallback &callback);

  virtual void RegisterOnNetwork(const std::string &network_id,
                                 Error *error,
                                 const ResultCallback &callback);
  virtual bool IsRegistered();
  virtual void SetUnregistered(bool searching);
  virtual std::string CreateFriendlyServiceName();
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
  virtual std::string GetNetworkTechnologyString() const;
  virtual std::string GetRoamingStateString() const;
  virtual std::string GetTypeString() const {
    return flimflam::kTechnologyFamilyGsm;
  }
  virtual void OnDBusPropertiesChanged(
      const std::string &interface,
      const DBusPropertiesMap &properties,
      const std::vector<std::string> &invalidated_properties);
  virtual bool AllowRoaming();

 protected:
  virtual void InitProxies();
  virtual void ReleaseProxies();
  virtual void UpdateStatus(const DBusPropertiesMap &properties);

 private:
  friend class CellularTest;
  friend class CellularCapabilityGSMTest;
  friend class CellularCapabilityTest;
  FRIEND_TEST(CellularCapabilityGSMTest, CreateDeviceFromProperties);
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
  FRIEND_TEST(CellularCapabilityGSMTest, UpdateStatus);
  FRIEND_TEST(CellularCapabilityGSMTest, SetHomeProvider);
  FRIEND_TEST(CellularCapabilityGSMTest, UpdateOperatorInfo);
  FRIEND_TEST(CellularCapabilityGSMTest, GetRegistrationState);
  FRIEND_TEST(CellularCapabilityGSMTest, OnDBusPropertiesChanged);
  FRIEND_TEST(CellularCapabilityGSMTest, SetupApnTryList);
  FRIEND_TEST(CellularCapabilityTest, AllowRoaming);
  FRIEND_TEST(CellularCapabilityTest, TryApns);
  FRIEND_TEST(CellularTest, StartGSMRegister);
  FRIEND_TEST(ModemTest, CreateDeviceFromProperties);

  struct SimLockStatus {
   public:
    SimLockStatus() : enabled(false), retries_left(0) {}

    bool enabled;
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
  static const char kPropertyEnabledFacilityLocks[];
  static const char kPropertyUnlockRequired[];
  static const char kPropertyUnlockRetries[];


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

  KeyValueStore SimLockStatusToProperty(Error *error);

  void SetupApnTryList();
  void FillConnectPropertyMap(DBusPropertiesMap *properties);

  void HelpRegisterDerivedKeyValueStore(
      const std::string &name,
      KeyValueStore(CellularCapabilityGSM::*get)(Error *error),
      void(CellularCapabilityGSM::*set)(
          const KeyValueStore &value, Error *error));

  bool IsUnderlyingDeviceRegistered() const;

  // Signal callbacks
  void OnNetworkModeSignal(uint32 mode);
  void OnRegistrationInfoSignal(uint32 status,
                                const std::string &operator_code,
                                const std::string &operator_name);
  void OnSignalQualitySignal(uint32 quality);

  // Method callbacks
  void OnGetRegistrationInfoReply(uint32 status,
                                  const std::string &operator_code,
                                  const std::string &operator_name,
                                  const Error &error);
  void OnGetSignalQualityReply(uint32 quality, const Error &error);
  void OnRegisterReply(const ResultCallback &callback,
                       const Error &error);
  void OnGetIMEIReply(const ResultCallback &callback,
                      const std::string &imei,
                      const Error &error);
  void OnGetIMSIReply(const ResultCallback &callback,
                      const std::string &imsi,
                      const Error &error);
  void OnGetSPNReply(const ResultCallback &callback,
                     const std::string &spn,
                     const Error &error);
  void OnGetMSISDNReply(const ResultCallback &callback,
                        const std::string &msisdn,
                        const Error &error);
  void OnScanReply(const ResultCallback &callback,
                   const GSMScanResults &results,
                   const Error &error);
  void OnConnectReply(const ResultCallback &callback, const Error &error);

  scoped_ptr<ModemGSMCardProxyInterface> card_proxy_;
  scoped_ptr<ModemGSMNetworkProxyInterface> network_proxy_;
  base::WeakPtrFactory<CellularCapabilityGSM> weak_ptr_factory_;

  uint32 registration_state_;
  uint32 access_technology_;
  Cellular::Operator serving_operator_;
  std::string spn_;
  mobile_provider *home_provider_;
  std::string desired_network_;

  // Properties.
  std::string selected_network_;
  Stringmaps found_networks_;
  std::deque<Stringmap> apn_try_list_;
  bool scanning_;
  uint16 scan_interval_;
  SimLockStatus sim_lock_status_;
  Stringmaps apn_list_;

  static unsigned int friendly_service_name_id_;

  DISALLOW_COPY_AND_ASSIGN(CellularCapabilityGSM);
};

}  // namespace shill

#endif  // SHILL_CELLULAR_CAPABILITY_GSM_
