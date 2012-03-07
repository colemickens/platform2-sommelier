// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_CELLULAR_CAPABILITY_
#define SHILL_CELLULAR_CAPABILITY_

#include <string>
#include <vector>

#include <base/basictypes.h>
#include <base/callback.h>
#include <base/memory/scoped_ptr.h>
#include <base/memory/weak_ptr.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST

#include "shill/dbus_properties.h"
#include "shill/modem_proxy_interface.h"
#include "shill/modem_simple_proxy_interface.h"

namespace shill {

class Cellular;
class Error;
class EventDispatcher;
class ProxyFactory;

typedef std::vector<base::Closure> CellularTaskList;

// Cellular devices instantiate subclasses of CellularCapability that
// handle the specific modem technologies and capabilities.
class CellularCapability {
 public:
  static const int kTimeoutActivate;
  static const int kTimeoutConnect;
  static const int kTimeoutDefault;
  static const int kTimeoutEnable;
  static const int kTimeoutRegister;
  static const int kTimeoutScan;

  static const char kConnectPropertyPhoneNumber[];
  static const char kPropertyIMSI[];


  // |cellular| is the parent Cellular device.
  CellularCapability(Cellular *cellular, ProxyFactory *proxy_factory);
  virtual ~CellularCapability();

  Cellular *cellular() const { return cellular_; }
  ProxyFactory *proxy_factory() const { return proxy_factory_; }

  // Invoked by the parent Cellular device when a new service is created.
  virtual void OnServiceCreated() = 0;

  virtual void UpdateStatus(const DBusPropertiesMap &properties) = 0;

  virtual void SetupConnectProperties(DBusPropertiesMap *properties) = 0;

  bool allow_roaming() const { return allow_roaming_; }

  // StartModem attempts to put the modem in a state in which it is
  // usable for creating services and establishing connections (if
  // network conditions permit). It potentially consists of multiple
  // non-blocking calls to the modem-manager server. After each call,
  // control is passed back up to the main loop. Each time a reply to
  // a non-blocking call is received, the operation advances to the next
  // step, until either an error occurs in one of them, or all the steps
  // have been completed, at which point StartModem() is finished.
  virtual void StartModem(Error *error,
                          const ResultCallback &callback) = 0;
  // StopModem is also a multi-step operation consisting of several
  // non-blocking calls.
  virtual void StopModem(Error *error, const ResultCallback &callback) = 0;
  virtual void Connect(const DBusPropertiesMap &properties, Error *error,
                       const ResultCallback &callback);
  virtual void Disconnect(Error *error, const ResultCallback &callback);

  // Activates the modem. Returns an Error on failure.
  // The default implementation fails by returning a kNotSupported error
  // to the caller.
  virtual void Activate(const std::string &carrier,
                        Error *error, const ResultCallback &callback);

  // Network registration.
  virtual void RegisterOnNetwork(const std::string &network_id,
                                 Error *error,
                                 const ResultCallback &callback);
  virtual bool IsRegistered() = 0;

  virtual std::string CreateFriendlyServiceName() = 0;

  // PIN management. The default implementation fails by returning an error.
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

  // Network scanning. The default implementation fails by invoking
  // the reply handler with an error.
  virtual void Scan(Error *error, const ResultCallback &callback);

  // Returns an empty string if the network technology is unknown.
  virtual std::string GetNetworkTechnologyString() const = 0;

  virtual std::string GetRoamingStateString() const = 0;

  virtual void GetSignalQuality() = 0;

  virtual void OnModemManagerPropertiesChanged(
      const DBusPropertiesMap &properties) = 0;

 protected:
  // The following five methods are only ever called as
  // callbacks (from the main loop), which is why they
  // don't take an Error * argument.
  virtual void EnableModem(const ResultCallback &callback);
  virtual void DisableModem(const ResultCallback &callback);
  virtual void GetModemStatus(const ResultCallback &callback);
  virtual void GetModemInfo(const ResultCallback &callback);
  virtual void GetProperties(const ResultCallback &callback) = 0;

  virtual void GetRegistrationState() = 0;

  void FinishEnable(const ResultCallback &callback);
  void FinishDisable(const ResultCallback &callback);
  virtual void InitProxies();
  virtual void ReleaseProxies();

  static void OnUnsupportedOperation(const char *operation, Error *error);

  // Run the next task in a list.
  // Precondition: |tasks| is not empty.
  void RunNextStep(CellularTaskList *tasks);
  void StepCompletedCallback(const ResultCallback &callback,
                             CellularTaskList *tasks, const Error &error);
  // Properties
  bool allow_roaming_;
  bool scanning_supported_;
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

 private:
  friend class CellularTest;
  friend class CellularCapabilityTest;
  FRIEND_TEST(CellularCapabilityGSMTest, SetStorageIdentifier);
  FRIEND_TEST(CellularCapabilityGSMTest, UpdateStatus);
  FRIEND_TEST(CellularCapabilityTest, AllowRoaming);
  FRIEND_TEST(CellularCapabilityTest, EnableModemFail);
  FRIEND_TEST(CellularCapabilityTest, EnableModemSucceed);
  FRIEND_TEST(CellularCapabilityTest, FinishEnable);
  FRIEND_TEST(CellularCapabilityTest, GetModemInfo);
  FRIEND_TEST(CellularCapabilityTest, GetModemStatus);
  FRIEND_TEST(CellularServiceTest, FriendlyName);
  FRIEND_TEST(CellularTest, StartCDMARegister);
  FRIEND_TEST(CellularTest, StartConnected);
  FRIEND_TEST(CellularTest, StartGSMRegister);
  FRIEND_TEST(CellularTest, StartLinked);
  FRIEND_TEST(CellularTest, Connect);
  FRIEND_TEST(CellularTest, Disconnect);

  void HelpRegisterDerivedBool(
      const std::string &name,
      bool(CellularCapability::*get)(Error *error),
      void(CellularCapability::*set)(const bool &value, Error *error));

  bool GetAllowRoaming(Error */*error*/) { return allow_roaming_; }
  void SetAllowRoaming(const bool &value, Error *error);

  // Method reply and signal callbacks from Modem interface
  virtual void OnModemStateChangedSignal(
      uint32 old_state, uint32 new_state, uint32 reason);
  virtual void OnGetModemInfoReply(const ResultCallback &callback,
                                   const ModemHardwareInfo &info,
                                   const Error &error);

  // Method reply callbacks from Modem.Simple interface
  virtual void OnGetModemStatusReply(const ResultCallback &callback,
                                     const DBusPropertiesMap &props,
                                     const Error &error);
  virtual void OnConnectReply(const ResultCallback &callback,
                              const Error &error);
  virtual void OnDisconnectReply(const ResultCallback &callback,
                                 const Error &error);

  Cellular *cellular_;

  // Store cached copies of singletons for speed/ease of testing.
  ProxyFactory *proxy_factory_;

  scoped_ptr<ModemProxyInterface> proxy_;
  scoped_ptr<ModemSimpleProxyInterface> simple_proxy_;
  base::WeakPtrFactory<CellularCapability> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(CellularCapability);
};

}  // namespace shill

#endif  // SHILL_CELLULAR_CAPABILITY_
