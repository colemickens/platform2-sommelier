// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_CELLULAR_
#define SHILL_CELLULAR_

#include <string>

#include <base/basictypes.h>
#include <base/task.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST

#include "shill/dbus_properties.h"
#include "shill/device.h"
#include "shill/modem_cdma_proxy_interface.h"
#include "shill/modem_gsm_card_proxy_interface.h"
#include "shill/modem_gsm_network_proxy_interface.h"
#include "shill/modem_proxy_interface.h"
#include "shill/refptr_types.h"
#include "shill/shill_event.h"

namespace shill {

class Error;
class ModemSimpleProxyInterface;
class ProxyFactory;

class Cellular : public Device,
                 public ModemCDMAProxyListener,
                 public ModemGSMCardProxyListener,
                 public ModemGSMNetworkProxyListener,
                 public ModemProxyListener {
 public:
  enum Type {
    kTypeGSM,
    kTypeCDMA
  };

  // The device states progress linearly from Disabled to Linked.
  enum State {
    // This is the initial state of the modem and indicates that the modem radio
    // is not turned on.
    kStateDisabled,
    // This state indicates that the modem radio is turned on, and it should be
    // possible to measure signal strength.
    kStateEnabled,
    // The modem has registered with a network and has signal quality
    // measurements. A cellular service object is created.
    kStateRegistered,
    // The modem has connected to a network.
    kStateConnected,
    // The network interface is UP.
    kStateLinked,
  };

  // These should be kept in sync with ModemManager's mm-modem.h:MMModemState.
  enum ModemState {
    kModemStateUnknown = 0,
    kModemStateDisabled = 10,
    kModemStateDisabling = 20,
    kModemStateEnabling = 30,
    kModemStateEnabled = 40,
    kModemStateSearching = 50,
    kModemStateRegistered = 60,
    kModemStateDisconnecting = 70,
    kModemStateConnecting = 80,
    kModemStateConnected = 90,
  };

  class Operator {
   public:
    Operator();
    ~Operator();

    void CopyFrom(const Operator &oper);

    const std::string &GetName() const;
    void SetName(const std::string &name);

    const std::string &GetCode() const;
    void SetCode(const std::string &code);

    const std::string &GetCountry() const;
    void SetCountry(const std::string &country);

    const Stringmap &ToDict() const;

   private:
    Stringmap dict_;

    DISALLOW_COPY_AND_ASSIGN(Operator);
  };

  struct SimLockStatus {
   public:
    SimLockStatus() : retries_left(0) {}
    SimLockStatus(const std::string &in_lock_type, uint32 in_retries_left)
        : lock_type(in_lock_type),
          retries_left(in_retries_left) {}

    std::string lock_type;
    uint32 retries_left;
  };

  static const char kConnectPropertyPhoneNumber[];

  // |owner| is the ModemManager DBus service owner (e.g., ":1.17"). |path| is
  // the ModemManager.Modem DBus object path (e.g.,
  // "/org/chromium/ModemManager/Gobi/0").
  Cellular(ControlInterface *control_interface,
           EventDispatcher *dispatcher,
           Manager *manager,
           const std::string &link_name,
           const std::string &address,
           int interface_index,
           Type type,
           const std::string &owner,
           const std::string &path);
  virtual ~Cellular();

  // Asynchronously connects the modem to the network. Populates |error| on
  // failure, leaves it unchanged otherwise.
  void Connect(Error *error);

  // Asynchronously activates the modem. Populates |error| on failure, leaves it
  // unchanged otherwise.
  void Activate(const std::string &carrier, Error *error);

  void set_modem_state(ModemState state) { modem_state_ = state; }
  ModemState modem_state() const { return modem_state_; }

  const SimLockStatus &sim_lock_status() const { return sim_lock_status_; }
  void set_sim_lock_status(const SimLockStatus &s) { sim_lock_status_ = s; }

  void SetGSMAccessTechnology(uint32 access_technology);

  // Inherited from Device.
  virtual void Start();
  virtual void Stop();
  virtual bool TechnologyIs(Technology::Identifier type) const;
  virtual void LinkEvent(unsigned int flags, unsigned int change);
  virtual void Scan(Error *error);
  virtual void RegisterOnNetwork(const std::string &network_id, Error *error);
  virtual void RequirePIN(const std::string &pin, bool require, Error *error);
  virtual void EnterPIN(const std::string &pin, Error *error);
  virtual void UnblockPIN(const std::string &unblock_code,
                          const std::string &pin,
                          Error *error);
  virtual void ChangePIN(const std::string &old_pin,
                         const std::string &new_pin,
                         Error *error);

 private:
  friend class CellularTest;
  FRIEND_TEST(CellularTest, Activate);
  FRIEND_TEST(CellularTest, ActivateError);
  FRIEND_TEST(CellularTest, CreateService);
  FRIEND_TEST(CellularTest, ChangePIN);
  FRIEND_TEST(CellularTest, ChangePINError);
  FRIEND_TEST(CellularTest, Connect);
  FRIEND_TEST(CellularTest, GetCDMAActivationStateString);
  FRIEND_TEST(CellularTest, GetCDMAActivationErrorString);
  FRIEND_TEST(CellularTest, GetCDMAIdentifiers);
  FRIEND_TEST(CellularTest, GetCDMANetworkTechnologyString);
  FRIEND_TEST(CellularTest, GetCDMARegistrationState);
  FRIEND_TEST(CellularTest, GetCDMARoamingStateString);
  FRIEND_TEST(CellularTest, GetCDMASignalQuality);
  FRIEND_TEST(CellularTest, GetGSMIdentifiers);
  FRIEND_TEST(CellularTest, GetGSMNetworkTechnologyString);
  FRIEND_TEST(CellularTest, GetGSMRoamingStateString);
  FRIEND_TEST(CellularTest, GetGSMSignalQuality);
  FRIEND_TEST(CellularTest, GetModemInfo);
  FRIEND_TEST(CellularTest, GetModemStatus);
  FRIEND_TEST(CellularTest, GetStateString);
  FRIEND_TEST(CellularTest, GetTypeString);
  FRIEND_TEST(CellularTest, EnterPIN);
  FRIEND_TEST(CellularTest, EnterPINError);
  FRIEND_TEST(CellularTest, InitProxiesCDMA);
  FRIEND_TEST(CellularTest, InitProxiesGSM);
  FRIEND_TEST(CellularTest, ParseScanResult);
  FRIEND_TEST(CellularTest, RegisterOnNetwork);
  FRIEND_TEST(CellularTest, RegisterOnNetworkError);
  FRIEND_TEST(CellularTest, RequirePIN);
  FRIEND_TEST(CellularTest, RequirePINError);
  FRIEND_TEST(CellularTest, SetGSMAccessTechnology);
  FRIEND_TEST(CellularTest, Scan);
  FRIEND_TEST(CellularTest, StartConnected);
  FRIEND_TEST(CellularTest, StartCDMARegister);
  FRIEND_TEST(CellularTest, StartGSMRegister);
  FRIEND_TEST(CellularTest, StartLinked);
  FRIEND_TEST(CellularTest, UnblockPIN);
  FRIEND_TEST(CellularTest, UnblockPINError);

  struct CDMA {
    CDMA();

    uint32 registration_state_evdo;
    uint32 registration_state_1x;
    uint32 activation_state;

    uint16 prl_version;
    std::string payment_url;
    std::string usage_url;
  };

  struct GSM {
    GSM();

    uint32 registration_state;
    uint32 access_technology;
    std::string network_id;
    std::string operator_name;
    std::string spn;
  };

  static const char kNetworkPropertyAccessTechnology[];
  static const char kNetworkPropertyID[];
  static const char kNetworkPropertyLongName[];
  static const char kNetworkPropertyShortName[];
  static const char kNetworkPropertyStatus[];
  static const char kPhoneNumberCDMA[];
  static const char kPhoneNumberGSM[];

  void SetState(State state);

  void ConnectTask(const DBusPropertiesMap &properties);
  void ScanTask();
  void ActivateTask(const std::string &carrier);
  void RegisterOnNetworkTask(const std::string &network_id);
  void RequirePINTask(const std::string &pin, bool require);
  void EnterPINTask(const std::string &pin);
  void UnblockPINTask(const std::string &unblock_code, const std::string &pin);
  void ChangePINTask(const std::string &old_pin, const std::string &new_pin);

  // Invoked when the modem is connected to the cellular network to transition
  // to the network-connected state and bring the network interface up.
  void EstablishLink();

  StrIntPair SimLockStatusToProperty();

  void HelpRegisterDerivedStrIntPair(
      const std::string &name,
      StrIntPair(Cellular::*get)(void),
      void(Cellular::*set)(const StrIntPair&, Error *));

  void InitProxies();

  std::string GetTypeString() const;
  static std::string GetStateString(State state);

  // Returns an empty string if the network technology is unknown.
  std::string GetNetworkTechnologyString() const;

  std::string GetRoamingStateString() const;

  static std::string GetCDMAActivationStateString(uint32 state);
  static std::string GetCDMAActivationErrorString(uint32 error);

  void EnableModem();
  void GetModemStatus();
  void GetGSMProperties();
  void RegisterGSMModem();

  // Obtains the modem identifiers: MEID for CDMA; IMEI, IMSI, SPN and MSISDN
  // for GSM.
  void GetModemIdentifiers();
  void GetCDMAIdentifiers();
  void GetGSMIdentifiers();

  // Obtains modem's manufacturer, model ID, and hardware revision.
  void GetModemInfo();

  void GetModemRegistrationState();
  void GetCDMARegistrationState();
  void GetGSMRegistrationState();

  // Processes a change in the modem registration state, possibly creating,
  // destroying or updating the CellularService.
  void HandleNewRegistrationState();
  void HandleNewRegistrationStateTask();

  void CreateService();

  void GetModemSignalQuality();
  uint32 GetCDMASignalQuality();
  uint32 GetGSMSignalQuality();

  void HandleNewSignalQuality(uint32 strength);

  void HandleNewCDMAActivationState(uint32 error);

  Stringmap ParseScanResult(
      const ModemGSMNetworkProxyInterface::ScanResult &result);

  // Signal callbacks inherited from ModemCDMAProxyListener.
  virtual void OnCDMAActivationStateChanged(
      uint32 activation_state,
      uint32 activation_error,
      const DBusPropertiesMap &status_changes);
  virtual void OnCDMARegistrationStateChanged(uint32 state_1x,
                                              uint32 state_evdo);
  virtual void OnCDMASignalQualityChanged(uint32 strength);

  // Signal callbacks inherited from ModemGSMNetworkProxyListener.
  virtual void OnGSMNetworkModeChanged(uint32 mode);
  virtual void OnGSMRegistrationInfoChanged(uint32 status,
                                            const std::string &operator_code,
                                            const std::string &operator_name);
  virtual void OnGSMSignalQualityChanged(uint32 quality);

  // Signal callbacks inherited from ModemProxyListener.
  virtual void OnModemStateChanged(uint32 old_state,
                                   uint32 new_state,
                                   uint32 reason);

  // Store cached copies of singletons for speed/ease of testing.
  ProxyFactory *proxy_factory_;

  Type type_;
  State state_;
  ModemState modem_state_;

  const std::string dbus_owner_;  // ModemManager.Modem
  const std::string dbus_path_;  // ModemManager.Modem
  scoped_ptr<ModemProxyInterface> proxy_;
  scoped_ptr<ModemSimpleProxyInterface> simple_proxy_;
  scoped_ptr<ModemCDMAProxyInterface> cdma_proxy_;
  scoped_ptr<ModemGSMCardProxyInterface> gsm_card_proxy_;
  scoped_ptr<ModemGSMNetworkProxyInterface> gsm_network_proxy_;

  CDMA cdma_;
  GSM gsm_;

  CellularServiceRefPtr service_;

  ScopedRunnableMethodFactory<Cellular> task_factory_;

  // Properties
  bool allow_roaming_;
  std::string carrier_;
  std::string meid_;
  std::string imei_;
  std::string imsi_;
  std::string esn_;
  std::string mdn_;
  std::string min_;
  std::string model_id_;
  std::string manufacturer_;
  std::string firmware_revision_;
  std::string hardware_revision_;
  bool scanning_;
  uint16 scan_interval_;
  std::string selected_network_;
  Stringmaps found_networks_;
  SimLockStatus sim_lock_status_;
  Operator home_provider_;

  DISALLOW_COPY_AND_ASSIGN(Cellular);
};

}  // namespace shill

#endif  // SHILL_CELLULAR_
