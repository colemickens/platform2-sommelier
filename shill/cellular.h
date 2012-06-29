// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_CELLULAR_
#define SHILL_CELLULAR_

#include <string>
#include <vector>

#include <base/basictypes.h>
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
    kTypeCDMA,
    kTypeUniversal,  // ModemManager1
    kTypeInvalid,
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

  enum ModemState {
    kModemStateUnknown = 0,
    kModemStateInitializing = 1,
    kModemStateLocked = 2,
    kModemStateDisabled = 3,
    kModemStateDisabling = 4,
    kModemStateEnabling = 5,
    kModemStateEnabled = 6,
    kModemStateSearching = 7,
    kModemStateRegistered = 8,
    kModemStateDisconnecting = 9,
    kModemStateConnecting = 10,
    kModemStateConnected = 11,
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

  // |owner| is the ModemManager DBus service owner (e.g., ":1.17").
  // |path| is the ModemManager.Modem DBus object path (e.g.,
  // "/org/chromium/ModemManager/Gobi/0").
  // |service| is the modem mananager service name (e.g.,
  // /org/freeDesktop/ModemManager, /org/freedesktop/ModemManager1
  // or /org/chromium/ModemManager).
  Cellular(ControlInterface *control_interface,
           EventDispatcher *dispatcher,
           Metrics *metrics,
           Manager *manager,
           const std::string &link_name,
           const std::string &address,
           int interface_index,
           Type type,
           const std::string &owner,
           const std::string &service,
           const std::string &path,
           mobile_provider_db *provider_db);
  virtual ~Cellular();

  // Load configuration for the device from |storage|.
  virtual bool Load(StoreInterface *storage);

  // Save configuration for the device to |storage|.
  virtual bool Save(StoreInterface *storage);

  // Asynchronously connects the modem to the network. Populates |error| on
  // failure, leaves it unchanged otherwise.
  void Connect(Error *error);

  // Asynchronously disconnects the modem from the network. Populates |error| on
  // failure, leaves it unchanged otherwise.
  void Disconnect(Error *error);

  // Asynchronously activates the modem. Returns an error on failure.
  void Activate(const std::string &carrier, Error *error,
                const ResultCallback &callback);

  const CellularServiceRefPtr &service() const { return service_; }

  // Deregisters and destructs the current service and destroys the connection,
  // if any. This also eliminates the circular references between this device
  // and the associated service, allowing eventual device destruction.
  virtual void DestroyService();

  static std::string GetStateString(State state);

  std::string CreateFriendlyServiceName();

  State state() const { return state_; }

  void set_modem_state(ModemState state) { modem_state_ = state; }
  ModemState modem_state() const { return modem_state_; }
  bool IsUnderlyingDeviceEnabled() const;
  static bool IsEnabledModemState(ModemState state);

  mobile_provider_db *provider_db() const { return provider_db_; }

  const std::string &dbus_owner() const { return dbus_owner_; }
  const std::string &dbus_path() const { return dbus_path_; }

  const Operator &home_provider() const { return home_provider_; }
  void set_home_provider(const Operator &oper);

  void HandleNewSignalQuality(uint32 strength);

  // Processes a change in the modem registration state, possibly creating,
  // destroying or updating the CellularService.
  void HandleNewRegistrationState();

  virtual void OnDBusPropertiesChanged(
      const std::string &interface,
      const DBusPropertiesMap &changed_properties,
      const std::vector<std::string> &invalidated_properties);

  // Inherited from Device.
  virtual void Start(Error *error, const EnabledStateChangedCallback &callback);
  virtual void Stop(Error *error, const EnabledStateChangedCallback &callback);
  virtual void LinkEvent(unsigned int flags, unsigned int change);
  virtual void Scan(Error *error);
  virtual void RegisterOnNetwork(const std::string &network_id,
                                 Error *error,
                                 const ResultCallback &callback);
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

  void StartModemCallback(const EnabledStateChangedCallback &callback,
                          const Error &error);
  void StopModemCallback(const EnabledStateChangedCallback &callback,
                         const Error &error);
  void OnConnecting();
  void OnConnected();
  void OnConnectFailed(const Error &error);
  void OnDisconnected();
  void OnDisconnectFailed();
  std::string GetTechnologyFamily(Error *error);
  void OnModemStateChanged(ModemState old_state,
                           ModemState new_state,
                           uint32 reason);

  // accessor to read the allow roaming property
  bool allow_roaming_property() const { return allow_roaming_; }

 private:
  friend class CellularTest;
  friend class CellularCapabilityTest;
  friend class CellularCapabilityCDMATest;
  friend class CellularCapabilityGSMTest;
  friend class CellularCapabilityUniversalTest;
  friend class CellularServiceTest;
  friend class ModemTest;
  FRIEND_TEST(CellularCapabilityCDMATest, CreateFriendlyServiceName);
  FRIEND_TEST(CellularCapabilityCDMATest, GetRegistrationState);
  FRIEND_TEST(CellularCapabilityGSMTest, CreateFriendlyServiceName);
  FRIEND_TEST(CellularServiceTest, FriendlyName);
  FRIEND_TEST(CellularTest, CreateService);
  FRIEND_TEST(CellularTest, Connect);
  FRIEND_TEST(CellularTest, ConnectAddsTerminationAction);
  FRIEND_TEST(CellularTest, ConnectFailure);
  FRIEND_TEST(CellularTest, ConnectFailureNoService);
  FRIEND_TEST(CellularTest, DisableModem);
  FRIEND_TEST(CellularTest, Disconnect);
  FRIEND_TEST(CellularTest, ModemStateChangeDisable);
  FRIEND_TEST(CellularTest, ModemStateChangeEnable);
  FRIEND_TEST(CellularTest, SetAllowRoaming);
  FRIEND_TEST(CellularTest, StartModemCallback);
  FRIEND_TEST(CellularTest, StartModemCallbackFail);
  FRIEND_TEST(CellularTest, StopModemCallback);
  FRIEND_TEST(CellularTest, StopModemCallbackFail);
  FRIEND_TEST(CellularTest, StartConnected);
  FRIEND_TEST(CellularTest, StartCDMARegister);
  FRIEND_TEST(CellularTest, StartGSMRegister);
  FRIEND_TEST(CellularTest, StartLinked);
  FRIEND_TEST(CellularCapabilityTest, AllowRoaming);
  FRIEND_TEST(CellularCapabilityTest, EnableModemFail);
  FRIEND_TEST(CellularCapabilityTest, EnableModemSucceed);
  FRIEND_TEST(CellularCapabilityTest, FinishEnable);
  FRIEND_TEST(CellularCapabilityTest, GetModemInfo);
  FRIEND_TEST(CellularCapabilityTest, GetModemStatus);
  FRIEND_TEST(CellularCapabilityUniversalTest, StopModemConnected);
  FRIEND_TEST(CellularCapabilityUniversalTest, Connect);

  // Names of properties in storage
  static const char kAllowRoaming[];

  void SetState(State state);

  // Invoked when the modem is connected to the cellular network to transition
  // to the network-connected state and bring the network interface up.
  void EstablishLink();

  void InitCapability(Type type, ProxyFactory *proxy_factory);

  void CreateService();

  // HelpRegisterDerived*: Expose a property over RPC, with the name |name|.
  //
  // Reads of the property will be handled by invoking |get|.
  // Writes to the property will be handled by invoking |set|.
  // Clearing the property will be handled by PropertyStore.
  void HelpRegisterDerivedBool(
      const std::string &name,
      bool(Cellular::*get)(Error *error),
      void(Cellular::*set)(const bool &value, Error *error));
  void HelpRegisterDerivedString(
      const std::string &name,
      std::string(Cellular::*get)(Error *error),
      void(Cellular::*set)(const std::string &value, Error *error));

  void OnConnectReply(const Error &error);
  void OnDisconnectReply(const Error &error);

  // DBUS accessors to read/modify the allow roaming property
  bool GetAllowRoaming(Error */*error*/) { return allow_roaming_; }
  void SetAllowRoaming(const bool &value, Error *error);

  // When shill terminates or ChromeOS suspends, this function is called to
  // disconnect from the cellular network.
  void StartTermination();

  State state_;
  ModemState modem_state_;

  scoped_ptr<CellularCapability> capability_;

  const std::string dbus_owner_;  // :x.y
  const std::string dbus_service_;  // org.*.ModemManager*
  const std::string dbus_path_;  // ModemManager.Modem

  mobile_provider_db *provider_db_;

  CellularServiceRefPtr service_;

  // Properties
  Operator home_provider_;

  // User preference to allow or disallow roaming
  bool allow_roaming_;

  DISALLOW_COPY_AND_ASSIGN(Cellular);
};

}  // namespace shill

#endif  // SHILL_CELLULAR_
