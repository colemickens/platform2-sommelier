// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_CELLULAR_CELLULAR_CAPABILITY_UNIVERSAL_H_
#define SHILL_CELLULAR_CELLULAR_CAPABILITY_UNIVERSAL_H_

#include <deque>
#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include <base/memory/weak_ptr.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST
#include <ModemManager/ModemManager.h>

#include "shill/cellular/cellular.h"
#include "shill/cellular/cellular_bearer.h"
#include "shill/cellular/cellular_capability.h"
#include "shill/cellular/mm1_modem_location_proxy_interface.h"
#include "shill/cellular/mm1_modem_modem3gpp_proxy_interface.h"
#include "shill/cellular/mm1_modem_proxy_interface.h"
#include "shill/cellular/mm1_modem_simple_proxy_interface.h"
#include "shill/cellular/mm1_sim_proxy_interface.h"
#include "shill/cellular/subscription_state.h"
#include "shill/data_types.h"
#include "shill/key_value_store.h"

namespace shill {

class ModemInfo;

// CellularCapabilityUniversal handles modems using the
// org.freedesktop.ModemManager1 DBUS interface.  This class is used for
// all types of modems, i.e. CDMA, GSM, and LTE modems.
class CellularCapabilityUniversal : public CellularCapability {
 public:
  using ScanResults = std::vector<KeyValueStore>;
  using ScanResult = KeyValueStore;
  using LockRetryData = std::map<uint32_t, uint32_t>;
  using SignalQuality = std::tuple<uint32_t, bool>;
  using ModesData = std::tuple<uint32_t, uint32_t>;
  using SupportedModes = std::vector<ModesData>;
  using PcoList = std::vector<std::tuple<uint32_t, bool, std::vector<uint8_t>>>;

  // Constants used in connect method call.  Make available to test matchers.
  // TODO(jglasgow): Generate from modem manager into
  // ModemManager-names.h.
  // See http://crbug.com/212909.
  static const char kConnectApn[];
  static const char kConnectUser[];
  static const char kConnectPassword[];
  static const char kConnectAllowedAuth[];
  static const char kConnectAllowRoaming[];

  CellularCapabilityUniversal(Cellular* cellular, ModemInfo* modem_info);
  ~CellularCapabilityUniversal() override;

  // Inherited from CellularCapability.
  std::string GetTypeString() const override;
  void OnPropertiesChanged(
      const std::string& interface,
      const KeyValueStore& changed_properties,
      const std::vector<std::string>& invalidated_properties) override;
  // Checks the modem state.  If the state is kModemStateDisabled, then the
  // modem is enabled.  Otherwise, the enable command is buffered until the
  // modem becomes disabled.  ModemManager rejects the enable command if the
  // modem is not disabled, for example, if it is initializing instead.
  void StartModem(Error* error, const ResultCallback& callback) override;
  void StopModem(Error* error, const ResultCallback& callback) override;
  void Reset(Error* error, const ResultCallback& callback) override;
  bool AreProxiesInitialized() const override;
  bool IsServiceActivationRequired() const override;
  void CompleteActivation(Error* error) override;
  void Scan(Error* error, const ResultStringmapsCallback& callback) override;
  void RegisterOnNetwork(const std::string& network_id,
                         Error* error,
                         const ResultCallback& callback) override;
  bool IsRegistered() const override;
  void SetUnregistered(bool searching) override;
  void OnServiceCreated() override;
  std::string GetNetworkTechnologyString() const override;
  std::string GetRoamingStateString() const override;
  void SetupConnectProperties(KeyValueStore* properties) override;
  void Connect(const KeyValueStore& properties,
               Error* error,
               const ResultCallback& callback) override;
  void Disconnect(Error* error, const ResultCallback& callback) override;
  CellularBearer* GetActiveBearer() const override;
  void RequirePIN(const std::string& pin,
                  bool require,
                  Error* error,
                  const ResultCallback& callback) override;
  void EnterPIN(const std::string& pin,
                Error* error,
                const ResultCallback& callback) override;
  void UnblockPIN(const std::string& unblock_code,
                  const std::string& pin,
                  Error* error,
                  const ResultCallback& callback) override;
  void ChangePIN(const std::string& old_pin,
                 const std::string& new_pin,
                 Error* error,
                 const ResultCallback& callback) override;

  virtual void GetProperties();

  // Location proxy methods
  void SetupLocation(uint32_t sources,
                     bool signal_location,
                     const ResultCallback& callback) override;

  void GetLocation(const StringCallback& callback) override;

  bool IsLocationUpdateSupported() override;

 protected:
  virtual void InitProxies();
  void ReleaseProxies() override;

  // Updates the |sim_path_| variable and creates a new proxy to the
  // DBUS ModemManager1.Sim interface.
  // TODO(armansito): Put this method in a 3GPP-only subclass.
  virtual void OnSimPathChanged(const std::string& sim_path);

  // Updates the online payment portal information, if any, for the cellular
  // provider.
  void UpdateServiceOLP() override;

  // Post-payment activation handlers.
  virtual void UpdatePendingActivationState();

  // Returns the operator-specific form of |mdn|, which is passed to the online
  // payment portal of a cellular operator.
  std::string GetMdnForOLP(const MobileOperatorInfo* operator_info) const;

 private:
  struct ModemModes {
    ModemModes()
        : allowed_modes(MM_MODEM_MODE_NONE),
          preferred_mode(MM_MODEM_MODE_NONE) {}

    ModemModes(uint32_t allowed, MMModemMode preferred)
        : allowed_modes(allowed),
          preferred_mode(preferred) {}

    uint32_t allowed_modes;        // Bits based on MMModemMode.
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

  // Plugin strings via ModemManager.
  static const char kAltairLTEMMPlugin[];
  static const char kNovatelLTEMMPlugin[];
  static const char kTelitMMPlugin[];

  static const int64_t kEnterPinTimeoutMilliseconds;
  static const int64_t kRegistrationDroppedUpdateTimeoutMilliseconds;
  static const int kSetPowerStateTimeoutMilliseconds;


  // Root path. The SIM path is reported by ModemManager to be the root path
  // when no SIM is present.
  static const char kRootPath[];

  friend class CellularTest;
  friend class CellularCapabilityUniversalTest;
  friend class CellularCapabilityUniversalCdmaTest;
  FRIEND_TEST(CellularCapabilityUniversalCdmaMainTest, PropertiesChanged);
  FRIEND_TEST(CellularCapabilityUniversalMainTest, Connect);
  FRIEND_TEST(CellularCapabilityUniversalMainTest, ConnectApns);
  FRIEND_TEST(CellularCapabilityUniversalMainTest, DisconnectNoProxy);
  FRIEND_TEST(CellularCapabilityUniversalMainTest, FillConnectPropertyMap);
  FRIEND_TEST(CellularCapabilityUniversalMainTest, GetMdnForOLP);
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
  FRIEND_TEST(CellularCapabilityUniversalMainTest, StopModemAltair);
  FRIEND_TEST(CellularCapabilityUniversalMainTest,
              StopModemAltairDeleteBearerFailure);
  FRIEND_TEST(CellularCapabilityUniversalMainTest, StopModemAltairNotConnected);
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
  FRIEND_TEST(CellularCapabilityUniversalMainTest, UpdateServiceOLP);
  FRIEND_TEST(CellularCapabilityUniversalTimerTest, CompleteActivation);
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
    int32_t retries_left;
  };

  // Methods used in starting a modem
  void EnableModem(bool deferralbe,
                   Error* error,
                   const ResultCallback& callback);
  void EnableModemCompleted(bool deferrable,
                            const ResultCallback& callback,
                            const Error& error);

  // Methods used in stopping a modem
  void Stop_DeleteActiveBearer(const ResultCallback& callback);
  void Stop_DeleteActiveBearerCompleted(const ResultCallback& callback,
                                        const Error& error);
  void Stop_Disable(const ResultCallback& callback);
  void Stop_DisableCompleted(const ResultCallback& callback,
                             const Error& error);
  void Stop_PowerDown(const ResultCallback& callback);
  void Stop_PowerDownCompleted(const ResultCallback& callback,
                               const Error& error);

  void Register(const ResultCallback& callback);

  // Updates |active_bearer_| to match the currently active bearer.
  void UpdateActiveBearer();

  Stringmap ParseScanResult(const ScanResult& result);

  KeyValueStore SimLockStatusToProperty(Error* error);

  void FillConnectPropertyMap(KeyValueStore* properties);

  void HelpRegisterConstDerivedKeyValueStore(
      const std::string& name,
      KeyValueStore(CellularCapabilityUniversal::*get)(Error* error));

  // Returns true if a connect error should be retried.  This function
  // abstracts modem specific behavior for modems which do a lousy job
  // of returning specific errors on connect failures.
  bool RetriableConnectError(const Error& error) const;

  // Signal callbacks
  void OnModemStateChangedSignal(int32_t old_state,
                                 int32_t new_state,
                                 uint32_t reason);

  // Property Change notification handlers
  void OnModemPropertiesChanged(
      const KeyValueStore& properties,
      const std::vector<std::string>& invalidated_properties);

  void OnSignalQualityChanged(uint32_t quality);

  void OnModemCurrentCapabilitiesChanged(uint32_t current_capabilities);
  void OnMdnChanged(const std::string& mdn);
  void OnModemRevisionChanged(const std::string& revision);
  void OnModemHardwareRevisionChanged(const std::string& hardware_revision);
  void OnModemDevicePathChanged(const std::string& path);
  void OnModemStateChanged(Cellular::ModemState state);
  void OnAccessTechnologiesChanged(uint32_t access_technologies);
  void OnBearersChanged(const RpcIdentifiers& bearers);
  void OnLockRetriesChanged(const LockRetryData& lock_retries);
  void OnLockTypeChanged(MMModemLock unlock_required);
  void OnSimLockStatusChanged();

  // Returns false if the MDN is empty or if the MDN consists of all 0s.
  bool IsMdnValid() const;

  // 3GPP property change handlers
  virtual void OnModem3gppPropertiesChanged(
      const KeyValueStore& properties,
      const std::vector<std::string>& invalidated_properties);
  void On3gppRegistrationChanged(MMModem3gppRegistrationState state,
                                 const std::string& operator_code,
                                 const std::string& operator_name);
  void Handle3gppRegistrationChange(MMModem3gppRegistrationState updated_state,
                                    const std::string& updated_operator_code,
                                    const std::string& updated_operator_name);
  void OnSubscriptionStateChanged(SubscriptionState updated_subscription_state);
  void OnFacilityLocksChanged(uint32_t locks);
  void OnPcoChanged(const PcoList& pco_list);

  // SIM property change handlers
  // TODO(armansito): Put these methods in a 3GPP-only subclass.
  void OnSimPropertiesChanged(
      const KeyValueStore& props,
      const std::vector<std::string>& invalidated_properties);
  void OnSpnChanged(const std::string& spn);
  void OnSimIdentifierChanged(const std::string& id);
  void OnOperatorIdChanged(const std::string& operator_id);
  void OnOperatorNameChanged(const std::string& operator_name);

  // Method callbacks
  void OnRegisterReply(const ResultCallback& callback,
                       const Error& error);
  void OnResetReply(const ResultCallback& callback, const Error& error);
  void OnScanReply(const ResultStringmapsCallback& callback,
                   const ScanResults& results,
                   const Error& error);
  void OnConnectReply(const ResultCallback& callback,
                      const std::string& bearer,
                      const Error& error);
  void OnSetupLocationReply(const Error& error);
  void OnGetLocationReply(const StringCallback& callback,
                          const std::map<uint32_t, brillo::Any>& results,
                          const Error& error);

  // Returns true, if |sim_path| constitutes a valid SIM path. Currently, a
  // path is accepted to be valid, as long as it is not equal to one of ""
  // and "/".
  bool IsValidSimPath(const std::string& sim_path) const;

  // Returns the normalized version of |mdn| by keeping only digits in |mdn|
  // and removing other non-digit characters.
  std::string NormalizeMdn(const std::string& mdn) const;

  // Post-payment activation handlers.
  void ResetAfterActivation();
  void UpdateServiceActivationState();
  void OnResetAfterActivationReply(const Error& error);

  static bool IsRegisteredState(MMModem3gppRegistrationState state);

  void set_active_bearer_for_test(std::unique_ptr<CellularBearer> bearer) {
    active_bearer_ = std::move(bearer);
  }

  std::unique_ptr<mm1::ModemModem3gppProxyInterface> modem_3gpp_proxy_;
  std::unique_ptr<mm1::ModemProxyInterface> modem_proxy_;
  std::unique_ptr<mm1::ModemSimpleProxyInterface> modem_simple_proxy_;
  std::unique_ptr<mm1::SimProxyInterface> sim_proxy_;
  std::unique_ptr<mm1::ModemLocationProxyInterface> modem_location_proxy_;
  // Used to enrich information about the network operator in |ParseScanResult|.
  // TODO(pprabhu) Instead instantiate a local |MobileOperatorInfo| instance
  // once the context has been separated out. (crbug.com/363874)
  std::unique_ptr<MobileOperatorInfo> mobile_operator_info_;

  MMModem3gppRegistrationState registration_state_;

  // Bits based on MMModemCapabilities
  uint32_t current_capabilities_;  // Technologies supported without a reload
  uint32_t access_technologies_;   // Bits based on MMModemAccessTechnology

  Stringmap serving_operator_;
  std::string spn_;
  std::string desired_network_;

  // Properties.
  std::deque<Stringmap> apn_try_list_;
  bool resetting_;
  SimLockStatus sim_lock_status_;
  SubscriptionState subscription_state_;
  std::string sim_path_;
  std::unique_ptr<CellularBearer> active_bearer_;
  RpcIdentifiers bearer_paths_;
  bool reset_done_;

  // If the modem is not in a state to be enabled when StartModem is called,
  // enabling is deferred using this callback.
  base::Closure deferred_enable_modem_callback_;

  // Sometimes flaky cellular network causes the 3GPP registration state to
  // rapidly change from registered --> searching and back. Delay such updates
  // a little to smooth over temporary registration loss.
  base::CancelableClosure registration_dropped_update_callback_;
  int64_t registration_dropped_update_timeout_milliseconds_;

  base::WeakPtrFactory<CellularCapabilityUniversal> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(CellularCapabilityUniversal);
};

}  // namespace shill

#endif  // SHILL_CELLULAR_CELLULAR_CAPABILITY_UNIVERSAL_H_
