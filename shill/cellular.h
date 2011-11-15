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
#include "shill/event_dispatcher.h"
#include "shill/modem_proxy_interface.h"
#include "shill/refptr_types.h"

struct mobile_provider_db;

namespace shill {

class CellularCapability;
class Error;
class ModemSimpleProxyInterface;
class ProxyFactory;

class Cellular : public Device,
                 public ModemProxyDelegate {
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
           const std::string &path,
           mobile_provider_db *provider_db);
  virtual ~Cellular();

  // Asynchronously connects the modem to the network. Populates |error| on
  // failure, leaves it unchanged otherwise.
  void Connect(Error *error);

  // Asynchronously activates the modem. Populates |error| on failure, leaves it
  // unchanged otherwise.
  void Activate(const std::string &carrier, Error *error);

  const CellularServiceRefPtr &service() const { return service_; }

  static std::string GetStateString(State state);

  State state() const { return state_; }

  void set_modem_state(ModemState state) { modem_state_ = state; }
  ModemState modem_state() const { return modem_state_; }

  const SimLockStatus &sim_lock_status() const { return sim_lock_status_; }
  void set_sim_lock_status(const SimLockStatus &s) { sim_lock_status_ = s; }

  mobile_provider_db *provider_db() const { return provider_db_; }

  const std::string &dbus_owner() const { return dbus_owner_; }
  const std::string &dbus_path() const { return dbus_path_; }

  void set_gsm_network_id(const std::string &id) { gsm_.network_id = id; }
  void set_gsm_operator_name(const std::string &name) {
    gsm_.operator_name = name;
  }

  uint32 gsm_access_technology() const { return gsm_.access_technology; }

  uint32 cdma_activation_state() const { return cdma_.activation_state; }
  void set_cdma_activation_state(uint32 state) {
    cdma_.activation_state = state;
  }

  void set_cdma_payment_url(const std::string &url) { cdma_.payment_url = url; }

  const std::string &meid() const { return meid_; }
  void set_meid(const std::string &meid) { meid_ = meid; }

  const std::string &imei() const { return imei_; }
  void set_imei(const std::string &imei) { imei_ = imei; }

  const std::string &imsi() const { return imsi_; }
  void set_imsi(const std::string &imsi) { imsi_ = imsi; }

  const std::string &spn() const { return gsm_.spn; }
  void set_spn(const std::string &spn) { gsm_.spn = spn; }

  const std::string &mdn() const { return mdn_; }
  void set_mdn(const std::string &mdn) { mdn_ = mdn; }

  const std::string &min() const { return min_; }
  void set_min(const std::string &min) { min_ = min; }

  ProxyFactory *proxy_factory() const { return proxy_factory_; }

  void SetGSMAccessTechnology(uint32 access_technology);

  void HandleNewSignalQuality(uint32 strength);

  // Processes a change in the modem registration state, possibly creating,
  // destroying or updating the CellularService.
  void HandleNewRegistrationState();

  void HandleNewCDMAActivationState(uint32 error);

  // Updates the GSM operator name and country based on a newly obtained network
  // id.
  void UpdateGSMOperatorInfo();

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
  friend class CellularCapabilityCDMATest;
  friend class CellularCapabilityGSMTest;
  FRIEND_TEST(CellularTest, CreateService);
  FRIEND_TEST(CellularTest, Connect);
  FRIEND_TEST(CellularTest, GetCDMAActivationStateString);
  FRIEND_TEST(CellularTest, GetCDMAActivationErrorString);
  FRIEND_TEST(CellularTest, GetModemInfo);
  FRIEND_TEST(CellularTest, GetModemStatus);
  FRIEND_TEST(CellularTest, GetTypeString);
  FRIEND_TEST(CellularTest, InitProxiesCDMA);
  FRIEND_TEST(CellularTest, InitProxiesGSM);
  FRIEND_TEST(CellularTest, SetGSMAccessTechnology);
  FRIEND_TEST(CellularTest, StartConnected);
  FRIEND_TEST(CellularTest, StartCDMARegister);
  FRIEND_TEST(CellularTest, StartGSMRegister);
  FRIEND_TEST(CellularTest, StartLinked);
  FRIEND_TEST(CellularTest, UpdateGSMOperatorInfo);

  struct CDMA {
    CDMA();

    uint32 activation_state;

    uint16 prl_version;
    std::string payment_url;
    std::string usage_url;
  };

  struct GSM {
    GSM();

    uint32 access_technology;
    std::string network_id;
    std::string operator_name;
    std::string operator_country;
    std::string spn;
  };

  static const char kPhoneNumberCDMA[];
  static const char kPhoneNumberGSM[];

  void SetState(State state);

  void ConnectTask(const DBusPropertiesMap &properties);

  // Invoked when the modem is connected to the cellular network to transition
  // to the network-connected state and bring the network interface up.
  void EstablishLink();

  StrIntPair SimLockStatusToProperty(Error *error);

  void HelpRegisterDerivedStrIntPair(
      const std::string &name,
      StrIntPair(Cellular::*get)(Error *),
      void(Cellular::*set)(const StrIntPair&, Error *));

  void InitCapability();
  void InitProxies();

  std::string GetTypeString() const;

  static std::string GetCDMAActivationStateString(uint32 state);
  static std::string GetCDMAActivationErrorString(uint32 error);

  void EnableModem();
  void GetModemStatus();

  // Obtains modem's manufacturer, model ID, and hardware revision.
  void GetModemInfo();

  void HandleNewRegistrationStateTask();

  void CreateService();

  // Updates the serving operator on the active service.
  void UpdateServingOperator();

  // Signal callbacks inherited from ModemProxyDelegate.
  virtual void OnModemStateChanged(uint32 old_state,
                                   uint32 new_state,
                                   uint32 reason);

  // Store cached copies of singletons for speed/ease of testing.
  ProxyFactory *proxy_factory_;

  Type type_;
  State state_;
  ModemState modem_state_;

  scoped_ptr<CellularCapability> capability_;

  const std::string dbus_owner_;  // ModemManager.Modem
  const std::string dbus_path_;  // ModemManager.Modem
  scoped_ptr<ModemProxyInterface> proxy_;
  scoped_ptr<ModemSimpleProxyInterface> simple_proxy_;

  mobile_provider_db *provider_db_;

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
  SimLockStatus sim_lock_status_;
  Operator home_provider_;

  DISALLOW_COPY_AND_ASSIGN(Cellular);
};

}  // namespace shill

#endif  // SHILL_CELLULAR_
