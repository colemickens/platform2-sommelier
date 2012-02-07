// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
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
class ProxyFactory;

class Cellular : public Device {
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
    bool Equals(const Operator &oper) const { return dict_ == oper.dict_; }

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

  // |owner| is the ModemManager DBus service owner (e.g., ":1.17"). |path| is
  // the ModemManager.Modem DBus object path (e.g.,
  // "/org/chromium/ModemManager/Gobi/0").
  Cellular(ControlInterface *control_interface,
           EventDispatcher *dispatcher,
           Metrics *metrics,
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

  // Asynchronously disconnects the modem from the network. Populates |error| on
  // failure, leaves it unchanged otherwise.
  void Disconnect(Error *error);

  // Asynchronously activates the modem. Returns an error on failure.
  void Activate(const std::string &carrier, ReturnerInterface *returner);

  const CellularServiceRefPtr &service() const { return service_; }

  static std::string GetStateString(State state);

  std::string CreateFriendlyServiceName();

  State state() const { return state_; }

  void set_modem_state(ModemState state) { modem_state_ = state; }
  ModemState modem_state() const { return modem_state_; }

  mobile_provider_db *provider_db() const { return provider_db_; }

  const std::string &dbus_owner() const { return dbus_owner_; }
  const std::string &dbus_path() const { return dbus_path_; }

  const Operator &home_provider() const { return home_provider_; }
  void set_home_provider(const Operator &oper);

  void HandleNewSignalQuality(uint32 strength);

  // Processes a change in the modem registration state, possibly creating,
  // destroying or updating the CellularService.
  void HandleNewRegistrationState();

  void OnModemManagerPropertiesChanged(const DBusPropertiesMap &properties);

  // Inherited from Device.
  virtual void Start();
  virtual void Stop();
  virtual bool TechnologyIs(Technology::Identifier type) const;
  virtual void LinkEvent(unsigned int flags, unsigned int change);
  virtual void Scan(Error *error);
  virtual void RegisterOnNetwork(const std::string &network_id,
                                 ReturnerInterface *returner);
  virtual void RequirePIN(
      const std::string &pin, bool require, ReturnerInterface *returner);
  virtual void EnterPIN(const std::string &pin, ReturnerInterface *returner);
  virtual void UnblockPIN(const std::string &unblock_code,
                          const std::string &pin,
                          ReturnerInterface *returner);
  virtual void ChangePIN(const std::string &old_pin,
                         const std::string &new_pin,
                         ReturnerInterface *returner);

  void OnModemEnabled();
  void OnModemDisabled();
  void OnConnected();
  void OnConnectFailed();
  void OnDisconnected();
  void OnDisconnectFailed();

 private:
  friend class CellularTest;
  friend class CellularCapabilityTest;
  friend class CellularCapabilityCDMATest;
  friend class CellularCapabilityGSMTest;
  friend class ModemTest;
  FRIEND_TEST(CellularCapabilityCDMATest, CreateFriendlyServiceName);
  FRIEND_TEST(CellularCapabilityGSMTest, CreateFriendlyServiceName);
  FRIEND_TEST(CellularServiceTest, FriendlyName);
  FRIEND_TEST(CellularTest, CreateService);
  FRIEND_TEST(CellularTest, Connect);
  FRIEND_TEST(CellularTest, DisableModem);
  FRIEND_TEST(CellularTest, DisconnectModem);
  FRIEND_TEST(CellularTest, StartConnected);
  FRIEND_TEST(CellularTest, StartCDMARegister);
  FRIEND_TEST(CellularTest, StartGSMRegister);
  FRIEND_TEST(CellularTest, StartLinked);
  FRIEND_TEST(CellularCapabilityTest, AllowRoaming);
  FRIEND_TEST(CellularCapabilityTest, EnableModemFail);
  FRIEND_TEST(CellularCapabilityTest, EnableModemSucceed);
  FRIEND_TEST(CellularCapabilityTest, GetModemInfo);
  FRIEND_TEST(CellularCapabilityTest, GetModemStatus);

  void SetState(State state);

  void ConnectTask(const DBusPropertiesMap &properties);
  void DisconnectTask();
  void DisconnectModem();

  // Invoked when the modem is connected to the cellular network to transition
  // to the network-connected state and bring the network interface up.
  void EstablishLink();

  void InitCapability(Type type, ProxyFactory *proxy_factory);

  void HandleNewRegistrationStateTask();

  void CreateService();
  void DestroyService();

  State state_;
  ModemState modem_state_;

  scoped_ptr<CellularCapability> capability_;

  const std::string dbus_owner_;  // ModemManager.Modem
  const std::string dbus_path_;  // ModemManager.Modem

  mobile_provider_db *provider_db_;

  CellularServiceRefPtr service_;

  ScopedRunnableMethodFactory<Cellular> task_factory_;

  // Properties
  Operator home_provider_;

  DISALLOW_COPY_AND_ASSIGN(Cellular);
};

}  // namespace shill

#endif  // SHILL_CELLULAR_
