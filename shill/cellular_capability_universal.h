// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_CELLULAR_CAPABILITY_UNIVERSAL_H_
#define SHILL_CELLULAR_CAPABILITY_UNIVERSAL_H_

#include <deque>
#include <map>
#include <string>
#include <vector>

#include <base/memory/scoped_ptr.h>
#include <base/memory/weak_ptr.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST
#include <ModemManager/ModemManager.h>

#include "shill/accessor_interface.h"
#include "shill/cellular.h"
#include "shill/cellular_capability.h"
#include "shill/cellular_operator_info.h"
#include "shill/mm1_modem_modem3gpp_proxy_interface.h"
#include "shill/mm1_modem_proxy_interface.h"
#include "shill/mm1_modem_simple_proxy_interface.h"
#include "shill/mm1_sim_proxy_interface.h"
#include "shill/out_of_credits_detector.h"

struct mobile_provider;

namespace shill {

class CellularBearer;
class ModemInfo;

// CellularCapabilityUniversal handles modems using the
// org.chromium.ModemManager1 DBUS interface.  This class is used for
// all types of modems, i.e. CDMA, GSM, and LTE modems.
class CellularCapabilityUniversal : public CellularCapability {
 public:
  typedef std::vector<DBusPropertiesMap> ScanResults;
  typedef DBusPropertiesMap ScanResult;
  typedef std::map<uint32_t, uint32_t> LockRetryData;

  // Constants used in connect method call.  Make available to test matchers.
  // TODO(jglasgow): Generate from modem manager into
  // ModemManager-names.h.
  // See http://crbug.com/212909.
  static const char kConnectPin[];
  static const char kConnectOperatorId[];
  static const char kConnectApn[];
  static const char kConnectIPType[];
  static const char kConnectUser[];
  static const char kConnectPassword[];
  static const char kConnectNumber[];
  static const char kConnectAllowRoaming[];
  static const char kConnectRMProtocol[];

  CellularCapabilityUniversal(Cellular *cellular,
                              ProxyFactory *proxy_factory,
                              ModemInfo *modem_info);

  // Inherited from CellularCapability.
  // Checks the modem state.  If the state is kModemStateDisabled, then the
  // modem is enabled.  Otherwise, the enable command is buffered until the
  // modem becomes disabled.  ModemManager rejects the enable command if the
  // modem is not disabled, for example, if it is initializing instead.
  virtual void StartModem(Error *error, const ResultCallback &callback);
  virtual void StopModem(Error *error, const ResultCallback &callback);
  virtual void Connect(const DBusPropertiesMap &properties, Error *error,
                       const ResultCallback &callback);
  virtual void Disconnect(Error *error, const ResultCallback &callback);
  virtual void DisconnectCleanup();
  virtual void Activate(const std::string &carrier,
                        Error *error, const ResultCallback &callback);
  virtual void CompleteActivation(Error *error);

  virtual void OnServiceCreated();
  virtual void SetupConnectProperties(DBusPropertiesMap *properties);
  virtual void GetProperties();
  virtual bool IsRegistered() const;
  virtual bool IsServiceActivationRequired() const;
  virtual void Register(const ResultCallback &callback);

  virtual void RegisterOnNetwork(const std::string &network_id,
                                 Error *error,
                                 const ResultCallback &callback);
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
  virtual void Reset(Error *error, const ResultCallback &callback);

  virtual void Scan(Error *error, const ResultStringmapsCallback &callback);
  virtual CellularBearer *GetActiveBearer() const;
  virtual std::string GetNetworkTechnologyString() const;
  virtual std::string GetRoamingStateString() const;
  virtual void GetSignalQuality();
  virtual std::string GetTypeString() const;
  virtual void OnDBusPropertiesChanged(
      const std::string &interface,
      const DBusPropertiesMap &changed_properties,
      const std::vector<std::string> &invalidated_properties);
  virtual bool AllowRoaming();

 protected:
  virtual void InitProxies();
  virtual void ReleaseProxies();

  // Updates the |sim_path_| variable and creates a new proxy to the
  // DBUS ModemManager1.Sim interface.
  // TODO(armansito): Put this method in a 3GPP-only subclass.
  virtual void OnSimPathChanged(const std::string &sim_path);

  // Updates the online payment portal information, if any, for the cellular
  // provider.
  virtual void UpdateOLP();

  // Post-payment activation handlers.
  virtual void UpdatePendingActivationState();

  // Updates the storage identifier used for the current cellular service.
  virtual void UpdateStorageIdentifier();

  // Returns the operator-specific form of |mdn|, which is passed to the online
  // payment portal of a cellular operator.
  std::string GetMdnForOLP(
      const CellularOperatorInfo::CellularOperator &cellular_operator) const;

 private:
  struct ModemModes {
    ModemModes()
        : allowed_modes(MM_MODEM_MODE_NONE),
          preferred_mode(MM_MODEM_MODE_NONE) {}

    ModemModes(uint32 allowed, MMModemMode preferred)
        : allowed_modes(allowed),
          preferred_mode(preferred) {}

    uint32 allowed_modes;        // Bits based on MMModemMode.
    MMModemMode preferred_mode;  // A single MMModemMode bit.
  };

  // Constants used in scan results.  Make available to unit tests.
  // TODO(jglasgow): Generate from modem manager into ModemManager-names.h.
  // See http://crbug.com/212909.
  static const char kStatusProperty[];
  static const char kOperatorLongProperty[];
  static const char kOperatorShortProperty[];
  static const char kOperatorCodeProperty[];
  static const char kOperatorAccessTechnologyProperty[];

  // Modem Model ID strings.  From modem firmware via modemmanager.
  static const char kALT3100ModelId[];
  static const char kE362ModelId[];

  // Generic service name prefix, shown when the correct carrier name is
  // unknown.
  static const char kGenericServiceNamePrefix[];

  static const int64 kActivationRegistrationTimeoutMilliseconds;
  static const int64 kEnterPinTimeoutMilliseconds;
  static const int64 kRegistrationDroppedUpdateTimeoutMilliseconds;
  static const int kSetPowerStateTimeoutMilliseconds;


  // Root path. The SIM path is reported by ModemManager to be the root path
  // when no SIM is present.
  static const char kRootPath[];

  friend class CellularTest;
  friend class CellularCapabilityTest;
  friend class CellularCapabilityUniversalTest;
  friend class CellularCapabilityUniversalCDMATest;
  FRIEND_TEST(CellularCapabilityUniversalCDMAMainTest, PropertiesChanged);
  FRIEND_TEST(CellularCapabilityUniversalCDMAMainTest, UpdateOLP);
  FRIEND_TEST(CellularCapabilityUniversalCDMAMainTest,
              UpdateStorageIdentifier);
  FRIEND_TEST(CellularCapabilityUniversalMainTest, AllowRoaming);
  FRIEND_TEST(CellularCapabilityUniversalMainTest,
              ActivationWaitForRegisterTimeout);
  FRIEND_TEST(CellularCapabilityUniversalMainTest, Connect);
  FRIEND_TEST(CellularCapabilityUniversalMainTest, ConnectApns);
  FRIEND_TEST(CellularCapabilityUniversalMainTest, CreateFriendlyServiceName);
  FRIEND_TEST(CellularCapabilityUniversalMainTest, DisconnectNoProxy);
  FRIEND_TEST(CellularCapabilityUniversalMainTest,
              DisconnectWithDeferredCallback);
  FRIEND_TEST(CellularCapabilityUniversalMainTest, ExtractPcoValue);
  FRIEND_TEST(CellularCapabilityUniversalMainTest, GetMdnForOLP);
  FRIEND_TEST(CellularCapabilityUniversalMainTest,
              GetNetworkTechnologyStringOnE362);
  FRIEND_TEST(CellularCapabilityUniversalMainTest,
              GetOutOfCreditsDetectionType);
  FRIEND_TEST(CellularCapabilityUniversalMainTest, GetTypeString);
  FRIEND_TEST(CellularCapabilityUniversalMainTest, IsMdnValid);
  FRIEND_TEST(CellularCapabilityUniversalMainTest, IsRegistered);
  FRIEND_TEST(CellularCapabilityUniversalMainTest, IsServiceActivationRequired);
  FRIEND_TEST(CellularCapabilityUniversalMainTest, IsValidSimPath);
  FRIEND_TEST(CellularCapabilityUniversalMainTest, NormalizeMdn);
  FRIEND_TEST(CellularCapabilityUniversalMainTest, OnLockRetriesChanged);
  FRIEND_TEST(CellularCapabilityUniversalMainTest, OnLockTypeChanged);
  FRIEND_TEST(CellularCapabilityUniversalMainTest,
              OnModemCurrentCapabilitiesChanged);
  FRIEND_TEST(CellularCapabilityUniversalMainTest, OnSimLockPropertiesChanged);
  FRIEND_TEST(CellularCapabilityUniversalMainTest, PropertiesChanged);
  FRIEND_TEST(CellularCapabilityUniversalMainTest, Reset);
  FRIEND_TEST(CellularCapabilityUniversalMainTest, Scan);
  FRIEND_TEST(CellularCapabilityUniversalMainTest, ScanFailure);
  FRIEND_TEST(CellularCapabilityUniversalMainTest, SetHomeProvider);
  FRIEND_TEST(CellularCapabilityUniversalMainTest, SimLockStatusChanged);
  FRIEND_TEST(CellularCapabilityUniversalMainTest, SimLockStatusToProperty);
  FRIEND_TEST(CellularCapabilityUniversalMainTest, SimPathChanged);
  FRIEND_TEST(CellularCapabilityUniversalMainTest, SimPropertiesChanged);
  FRIEND_TEST(CellularCapabilityUniversalMainTest, StartModem);
  FRIEND_TEST(CellularCapabilityUniversalMainTest, StartModemFailure);
  FRIEND_TEST(CellularCapabilityUniversalMainTest, StartModemInWrongState);
  FRIEND_TEST(CellularCapabilityUniversalMainTest,
              StartModemWithDeferredEnableFailure);
  FRIEND_TEST(CellularCapabilityUniversalMainTest, StopModem);
  FRIEND_TEST(CellularCapabilityUniversalMainTest, StopModemConnected);
  FRIEND_TEST(CellularCapabilityUniversalMainTest, TerminationAction);
  FRIEND_TEST(CellularCapabilityUniversalMainTest,
              TerminationActionRemovedByStopModem);
  FRIEND_TEST(CellularCapabilityUniversalMainTest, UpdateActiveBearer);
  FRIEND_TEST(CellularCapabilityUniversalMainTest,
              UpdatePendingActivationState);
  FRIEND_TEST(CellularCapabilityUniversalMainTest,
              UpdateRegistrationState);
  FRIEND_TEST(CellularCapabilityUniversalMainTest,
              UpdateRegistrationStateModemNotConnected);
  FRIEND_TEST(CellularCapabilityUniversalMainTest,
              UpdateServiceActivationState);
  FRIEND_TEST(CellularCapabilityUniversalMainTest, UpdateServiceName);
  FRIEND_TEST(CellularCapabilityUniversalMainTest, UpdateStorageIdentifier);
  FRIEND_TEST(CellularCapabilityUniversalMainTest, UpdateOLP);
  FRIEND_TEST(CellularCapabilityUniversalMainTest, UpdateOperatorInfo);
  FRIEND_TEST(CellularCapabilityUniversalMainTest,
              UpdateOperatorInfoViaOperatorId);
  FRIEND_TEST(CellularCapabilityUniversalTimerTest, CompleteActivation);
  FRIEND_TEST(CellularTest, EnableTrafficMonitor);
  FRIEND_TEST(CellularTest,
              HandleNewRegistrationStateForServiceRequiringActivation);
  FRIEND_TEST(CellularTest, ModemStateChangeLostRegistration);
  FRIEND_TEST(CellularTest, OnPPPDied);

  // SimLockStatus represents the fields in the Cellular.SIMLockStatus
  // DBUS property of the shill device.
  struct SimLockStatus {
   public:
    SimLockStatus() : enabled(false),
                      lock_type(MM_MODEM_LOCK_UNKNOWN),
                      retries_left(0) {}

    bool enabled;
    MMModemLock lock_type;
    uint32 retries_left;
  };

  // SubscriptionState represents the provisioned state of SIM. It is used
  // currently by activation logic for LTE to determine if activation process is
  // complete.
  enum SubscriptionState {
    kSubscriptionStateUnknown = 0,
    kSubscriptionStateUnprovisioned = 1,
    kSubscriptionStateProvisioned = 2,
    kSubscriptionStateOutOfData = 3
  };

  // Methods used in starting a modem
  void EnableModem(bool deferralbe,
                   Error *error,
                   const ResultCallback& callback);
  void EnableModemCompleted(bool deferrable,
                            const ResultCallback &callback,
                            const Error &error);

  // Methods used in stopping a modem
  void Stop_Disable(const ResultCallback &callback);
  void Stop_DisableCompleted(const ResultCallback &callback,
                             const Error &error);
  void Stop_PowerDown(const ResultCallback &callback);
  void Stop_PowerDownCompleted(const ResultCallback &callback,
                               const Error &error);

  // Updates the name property that is exposed by the service to Chrome.
  void UpdateServiceName();

  // Methods used in acquiring information related to the carrier;

  // Updates the Universal operator name and country based on a newly
  // obtained network id.
  void UpdateOperatorInfo();

  // Sets the upper level information about the home cellular provider from the
  // modem's IMSI and SPN.
  void SetHomeProvider();

  // Updates the serving operator on the active service.
  void UpdateServingOperator();

  // Initializes the |apn_list| property based on the current
  // |home_provider_info_|.
  void InitAPNList();

  // Updates |active_bearer_| to match the currently active bearer.
  void UpdateActiveBearer();

  Stringmap ParseScanResult(const ScanResult &result);

  KeyValueStore SimLockStatusToProperty(Error *error);

  void SetupApnTryList();
  void FillConnectPropertyMap(DBusPropertiesMap *properties);

  void HelpRegisterConstDerivedKeyValueStore(
      const std::string &name,
      KeyValueStore(CellularCapabilityUniversal::*get)(Error *error));

  // Returns true if a connect error should be retried.  This function
  // abstracts modem specific behavior for modems which do a lousy job
  // of returning specific errors on connect failures.
  bool RetriableConnectError(const Error &error) const;

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

  void OnSupportedCapabilitesChanged(
      const std::vector<uint32> &supported_capabilities);
  void OnModemCurrentCapabilitiesChanged(uint32 current_capabilities);
  void OnMdnChanged(const std::string &mdn);
  void OnModemRevisionChanged(const std::string &revision);
  void OnModemStateChanged(Cellular::ModemState state);
  void OnAccessTechnologiesChanged(uint32 access_technologies);
  void OnSupportedModesChanged(const std::vector<ModemModes> &supported_modes);
  void OnCurrentModesChanged(const ModemModes &current_modes);
  void OnBearersChanged(const RpcIdentifiers &bearers);
  void OnLockRetriesChanged(const LockRetryData &lock_retries);
  void OnLockTypeChanged(MMModemLock unlock_required);
  void OnSimLockStatusChanged();

  // Returns false if the MDN is empty or if the MDN consists of all 0s.
  bool IsMdnValid() const;

  // 3GPP property change handlers
  virtual void OnModem3GPPPropertiesChanged(
      const DBusPropertiesMap &properties,
      const std::vector<std::string> &invalidated_properties);
  void On3GPPRegistrationChanged(MMModem3gppRegistrationState state,
                                 const std::string &operator_code,
                                 const std::string &operator_name);
  void Handle3GPPRegistrationChange(
      MMModem3gppRegistrationState updated_state,
      std::string updated_operator_code,
      std::string updated_operator_name);
  void On3GPPSubscriptionStateChanged(MMModem3gppSubscriptionState state);
  void OnFacilityLocksChanged(uint32 locks);

  // SIM property change handlers
  // TODO(armansito): Put these methods in a 3GPP-only subclass.
  void OnSimPropertiesChanged(
      const DBusPropertiesMap &props,
      const std::vector<std::string> &invalidated_properties);
  void OnSpnChanged(const std::string &spn);
  void OnSimIdentifierChanged(const std::string &id);
  void OnOperatorIdChanged(const std::string &operator_id);
  void OnOperatorNameChanged(const std::string &operator_name);

  // Method callbacks
  void OnRegisterReply(const ResultCallback &callback,
                       const Error &error);
  void OnResetReply(const ResultCallback &callback, const Error &error);
  void OnScanReply(const ResultStringmapsCallback &callback,
                   const ScanResults &results,
                   const Error &error);
  void OnConnectReply(const ResultCallback &callback,
                      const DBus::Path &bearer,
                      const Error &error);

  // Timeout callback that is called if the modem takes too long to register to
  // a network after online payment is complete. Resets the modem.
  void OnActivationWaitForRegisterTimeout();

  // Returns true, if |sim_path| constitutes a valid SIM path. Currently, a
  // path is accepted to be valid, as long as it is not equal to one of ""
  // and "/".
  bool IsValidSimPath(const std::string &sim_path) const;

  // Returns the normalized version of |mdn| by keeping only digits in |mdn|
  // and removing other non-digit characters.
  std::string NormalizeMdn(const std::string &mdn) const;

  static std::string GenerateNewGenericServiceName();

  // Post-payment activation handlers.
  void ResetAfterActivation();
  void UpdateServiceActivationState();
  void OnResetAfterActivationReply(const Error &error);

  static bool IsRegisteredState(MMModem3gppRegistrationState state);

  // Returns the out-of-credits detection algorithm to be used on this modem.
  OutOfCreditsDetector::OOCType GetOutOfCreditsDetectionType() const;

  scoped_ptr<mm1::ModemModem3gppProxyInterface> modem_3gpp_proxy_;
  scoped_ptr<mm1::ModemProxyInterface> modem_proxy_;
  scoped_ptr<mm1::ModemSimpleProxyInterface> modem_simple_proxy_;
  scoped_ptr<mm1::SimProxyInterface> sim_proxy_;

  base::WeakPtrFactory<CellularCapabilityUniversal> weak_ptr_factory_;

  MMModem3gppRegistrationState registration_state_;

  // Bits based on MMModemCapabilities
  std::vector<uint32> supported_capabilities_;  // Technologies supported
  uint32 current_capabilities_;  // Technologies supported without a reload
  uint32 access_technologies_;   // Bits based on MMModemAccessTechnology
  std::vector<ModemModes> supported_modes_;
  ModemModes current_modes_;

  Cellular::Operator serving_operator_;
  std::string spn_;
  std::string operator_id_;
  mobile_provider *home_provider_info_;
  std::string desired_network_;

  // Properties.
  std::deque<Stringmap> apn_try_list_;
  bool resetting_;
  SimLockStatus sim_lock_status_;
  SubscriptionState subscription_state_;
  std::string sim_path_;
  scoped_ptr<CellularBearer> active_bearer_;
  RpcIdentifiers bearer_paths_;
  bool reset_done_;

  // If the modem is not in a state to be enabled when StartModem is called,
  // enabling is deferred using this callback.
  base::Closure deferred_enable_modem_callback_;

  base::CancelableClosure activation_wait_for_registration_callback_;
  int64 activation_registration_timeout_milliseconds_;

  // Sometimes flaky cellular network causes the 3GPP registration state to
  // rapidly change from registered --> searching and back. Delay such updates
  // a little to smooth over temporary registration loss.
  base::CancelableClosure registration_dropped_update_callback_;
  int64 registration_dropped_update_timeout_milliseconds_;

  static unsigned int friendly_service_name_id_;

  DISALLOW_COPY_AND_ASSIGN(CellularCapabilityUniversal);
};

}  // namespace shill

#endif  // SHILL_CELLULAR_CAPABILITY_UNIVERSAL_H_
