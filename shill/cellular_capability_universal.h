// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_CELLULAR_CAPABILITY_UNIVERSAL_
#define SHILL_CELLULAR_CAPABILITY_UNIVERSAL_

#include <deque>
#include <map>
#include <string>
#include <vector>

#include <base/memory/scoped_ptr.h>
#include <base/memory/weak_ptr.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST
#include <mm/ModemManager-enums.h>

#include "shill/accessor_interface.h"
#include "shill/cellular.h"
#include "shill/cellular_capability.h"
#include "shill/mm1_modem_modem3gpp_proxy_interface.h"
#include "shill/mm1_modem_modemcdma_proxy_interface.h"
#include "shill/mm1_modem_proxy_interface.h"
#include "shill/mm1_modem_simple_proxy_interface.h"
#include "shill/mm1_sim_proxy_interface.h"


struct mobile_provider;

namespace shill {

// CellularCapabilityUniversal handles modems using the
// org.chromium.ModemManager1 DBUS interface.  This class is used for
// all types of modems, i.e. CDMA, GSM, and LTE modems.
class CellularCapabilityUniversal : public CellularCapability {
 public:
  typedef std::vector<DBusPropertiesMap> ScanResults;
  typedef DBusPropertiesMap ScanResult;
  typedef std::map< uint32_t, uint32_t > LockRetryData;

  CellularCapabilityUniversal(Cellular *cellular, ProxyFactory *proxy_factory);

  // Inherited from CellularCapability.
  virtual void StartModem(Error *error, const ResultCallback &callback);
  virtual void StopModem(Error *error, const ResultCallback &callback);
  virtual void Connect(const DBusPropertiesMap &properties, Error *error,
                       const ResultCallback &callback);
  virtual void Disconnect(Error *error, const ResultCallback &callback);
  virtual void Activate(const std::string &carrier,
                        Error *error, const ResultCallback &callback);

  virtual void OnServiceCreated();
  virtual void SetupConnectProperties(DBusPropertiesMap *properties);
  virtual void GetProperties();
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
  virtual void GetSignalQuality();
  virtual std::string GetTypeString() const { return "Universal"; }
  virtual void OnDBusPropertiesChanged(
      const std::string &interface,
      const DBusPropertiesMap &changed_properties,
      const std::vector<std::string> &invalidated_properties);
  virtual bool AllowRoaming();
 protected:
  virtual void InitProxies();
  virtual void ReleaseProxies();

 private:
  friend class CellularTest;
  friend class CellularCapabilityUniversalTest;
  friend class CellularCapabilityTest;
  FRIEND_TEST(CellularCapabilityUniversalTest, StartModem);
  FRIEND_TEST(CellularCapabilityUniversalTest, PropertiesChanged);
  FRIEND_TEST(CellularCapabilityUniversalTest, SimPropertiesChanged);

  // Methods used in starting a modem
  void Start_EnableModemCompleted(const ResultCallback &callback,
                                  const Error &error);

  // Methods used in stopping a modem
  void Stop_DisconnectCompleted(const ResultCallback &callback,
                               const Error &error);
  void Stop_Disable(const ResultCallback &callback);
  void Stop_DisableCompleted(const ResultCallback &callback,
                             const Error &error);

  // Sets the upper level information about the home cellular provider from the
  // modem's IMSI and SPN.
  void SetHomeProvider();

  // Updates the Universal operator name and country based on a newly
  // obtained network id.
  void UpdateOperatorInfo();

  // Updates the serving operator on the active service.
  void UpdateServingOperator();

  // Initializes the |apn_list_| property based on the current |home_provider_|.
  void InitAPNList();

  Stringmap ParseScanResult(const ScanResult &result);

  KeyValueStore SimLockStatusToProperty(Error *error);

  void SetupApnTryList();
  void FillConnectPropertyMap(DBusPropertiesMap *properties);

  void HelpRegisterDerivedKeyValueStore(
      const std::string &name,
      KeyValueStore(CellularCapabilityUniversal::*get)(Error *error),
      void(CellularCapabilityUniversal::*set)(
          const KeyValueStore &value, Error *error));

  // Signal callbacks
  void OnNetworkModeSignal(uint32 mode);
  void OnModemStateChangedSignal(int32 old_state,
                                 int32 new_state,
                                 uint32 reason);

  // Property Change notification handlers
  void OnModemPropertiesChanged(
      const DBusPropertiesMap &properties,
      const std::vector<std::string> &invalidated_properties);

  void OnSignalQualityChanged(uint32 quality);

  // Updates the sim_path_ variable and creates a new proxy to the
  // DBUS ModemManager1.Sim interface
  void OnSimPathChanged(const std::string &sim_path);
  void OnModemCapabilitesChanged(uint32 capabilities);
  void OnModemCurrentCapabilitiesChanged(uint32 current_capabilities);
  void OnMdnChanged(const std::string &mdn);
  void OnModemManufacturerChanged(const std::string &manufacturer);
  void OnModemModelChanged(const std::string &model);
  void OnModemRevisionChanged(const std::string &revision);
  void OnModemStateChanged(Cellular::ModemState state);
  void OnAccessTechnologiesChanged(uint32 access_technologies);
  void OnSupportedModesChanged(uint32 supported_modes);
  void OnAllowedModesChanged(uint32 allowed_modes);
  void OnPreferredModeChanged(MMModemMode preferred_mode);
  void OnLockRetriesChanged(MMModemLock unlock_required,
                            const LockRetryData &lock_retries);
  void OnSimLockStatusChanged();

  // 3GPP property change handlers
  virtual void OnModem3GPPPropertiesChanged(
      const DBusPropertiesMap &properties,
      const std::vector<std::string> &invalidated_properties);
  void OnImeiChanged(const std::string &imei);
  void On3GPPRegistrationChanged(MMModem3gppRegistrationState state,
                                 const std::string &operator_code,
                                 const std::string &operator_name);
  void OnFacilityLocksChanged(uint32 locks);

  // SIM property change handlers
  void OnSimPropertiesChanged(
      const DBusPropertiesMap &props,
      const std::vector<std::string> &invalidated_properties);
  void OnImsiChanged(const std::string &imsi);
  void OnSimIdentifierChanged(const std::string &id);
  void OnOperatorIdChanged(const std::string &operator_id);
  void OnOperatorNameChanged(const std::string &operator_name);

  // Method callbacks
  void OnRegisterReply(const ResultCallback &callback,
                       const Error &error);
  void OnScanReply(const ResultCallback &callback,
                   const ScanResults &results,
                   const Error &error);
  void OnConnectReply(const ResultCallback &callback,
                      const DBus::Path &bearer,
                      const Error &error);

  scoped_ptr<mm1::ModemModem3gppProxyInterface> modem_3gpp_proxy_;
  scoped_ptr<mm1::ModemModemCdmaProxyInterface> modem_cdma_proxy_;
  scoped_ptr<mm1::ModemProxyInterface> modem_proxy_;
  scoped_ptr<mm1::ModemSimpleProxyInterface> modem_simple_proxy_;
  scoped_ptr<mm1::SimProxyInterface> sim_proxy_;

  base::WeakPtrFactory<CellularCapabilityUniversal> weak_ptr_factory_;

  MMModem3gppRegistrationState registration_state_;
  MMModemCdmaRegistrationState cdma_registration_state_;

  // Bits based on MMModemCapabilities
  uint32 capabilities_;         // technologies supported, may require reload
  uint32 current_capabilities_; // technologies supportsed without a reload
  uint32 access_technologies_;  // Bits based on MMModemAccessTechnology
  uint32 supported_modes_;      // Bits based on MMModemMode
  uint32 allowed_modes_;        // Bits based on MMModemMode
  MMModemMode preferred_mode_;       // A single MMModemMode bit

  Cellular::Operator serving_operator_;
  std::string spn_;
  std::string sim_identifier_;
  std::string operator_id_;
  mobile_provider *home_provider_;
  std::string desired_network_;

  // Properties.
  std::string carrier_;
  std::string esn_;
  std::string firmware_revision_;
  std::string hardware_revision_;
  std::string imei_;
  std::string imsi_;
  std::string manufacturer_;
  std::string mdn_;
  std::string meid_;
  std::string min_;
  std::string model_id_;
  std::string selected_network_;
  Stringmaps found_networks_;
  std::deque<Stringmap> apn_try_list_;
  bool scanning_supported_;
  bool scanning_;
  uint16 scan_interval_;
  SimLockStatus sim_lock_status_;
  Stringmaps apn_list_;
  std::string sim_path_;
  DBus::Path bearer_path_;

  static unsigned int friendly_service_name_id_;

  DISALLOW_COPY_AND_ASSIGN(CellularCapabilityUniversal);
};

}  // namespace shill

#endif  // SHILL_CELLULAR_CAPABILITY_UNIVERSAL_
