// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_CELLULAR_CAPABILITY_
#define SHILL_CELLULAR_CAPABILITY_

#include <string>

#include <base/basictypes.h>
#include <base/memory/scoped_ptr.h>
#include <base/memory/scoped_vector.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST

#include "shill/async_call_handler.h"
#include "shill/dbus_properties.h"
#include "shill/modem_proxy_interface.h"
#include "shill/modem_simple_proxy_interface.h"

class Task;

namespace shill {

class Cellular;
class Error;
class EventDispatcher;
class ProxyFactory;

// Cellular devices instantiate subclasses of CellularCapability that
// handle the specific modem technologies and capabilities.
class CellularCapability : public ModemProxyDelegate,
                           public ModemSimpleProxyDelegate {
 public:
  static const int kTimeoutActivate;
  static const int kTimeoutConnect;
  static const int kTimeoutDefault;
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
  virtual void StartModem();
  virtual void StopModem();
  // EnableModem ensures that the modem device is powered.
  virtual void EnableModem(AsyncCallHandler *call_handler);
  virtual void DisableModem(AsyncCallHandler *call_handler);
  virtual void GetModemStatus(AsyncCallHandler *call_handler);
  virtual void GetModemInfo(AsyncCallHandler *call_handler);
  virtual void Connect(const DBusPropertiesMap &properties);
  virtual void Disconnect();

  // Activates the modem. Returns an Error on failure.
  // The default implementation fails by returning a kNotSupported error
  // to the caller.
  virtual void Activate(const std::string &carrier,
                        AsyncCallHandler *call_handler);

  // Network registration.
  virtual void RegisterOnNetwork(const std::string &network_id,
                                 AsyncCallHandler *call_handler);
  virtual bool IsRegistered() = 0;

  virtual void GetProperties(AsyncCallHandler *call_handler) = 0;

  // Retrieves the current cellular signal strength.
  virtual void GetSignalQuality() = 0;

  virtual void GetRegistrationState(AsyncCallHandler *call_handler) = 0;

  virtual std::string CreateFriendlyServiceName() = 0;

  // PIN management. The default implementation fails by returning an error.
  virtual void RequirePIN(
      const std::string &pin, bool require, AsyncCallHandler *call_handler);
  virtual void EnterPIN(const std::string &pin, AsyncCallHandler *call_handler);
  virtual void UnblockPIN(const std::string &unblock_code,
                          const std::string &pin,
                          AsyncCallHandler *call_handler);
  virtual void ChangePIN(const std::string &old_pin,
                         const std::string &new_pin,
                         AsyncCallHandler *call_handler);

  // Network scanning. The default implementation fails by invoking
  // the reply handler with an error.
  virtual void Scan(AsyncCallHandler *call_handler);

  // Returns an empty string if the network technology is unknown.
  virtual std::string GetNetworkTechnologyString() const = 0;

  virtual std::string GetRoamingStateString() const = 0;

  virtual void OnModemManagerPropertiesChanged(
      const DBusPropertiesMap &properties) = 0;

  // Method and signal callbacks inherited from ModemProxyDelegate.
  virtual void OnModemStateChanged(
      uint32 old_state, uint32 new_state, uint32 reason);
  virtual void OnModemEnableCallback(const Error &error,
                                     AsyncCallHandler *call_handler);
  virtual void OnGetModemInfoCallback(const ModemHardwareInfo &info,
                                      const Error &error,
                                      AsyncCallHandler *call_handler);

  // Method and signal callbacks inherited from ModemSimpleProxyDelegate.
  virtual void OnGetModemStatusCallback(const DBusPropertiesMap &props,
                                        const Error &error,
                                        AsyncCallHandler *call_handler);
  virtual void OnConnectCallback(const Error &error,
                                 AsyncCallHandler *call_handler);
  virtual void OnDisconnectCallback(const Error &error,
                                    AsyncCallHandler *call_handler);

 protected:
  class MultiStepAsyncCallHandler : public AsyncCallHandler {
   public:
    explicit MultiStepAsyncCallHandler(EventDispatcher *dispatcher);
    virtual ~MultiStepAsyncCallHandler();

    // Override the non-error case
    virtual bool CompleteOperation();

    void AddTask(Task *task);
    void PostNextTask();

   private:
    EventDispatcher *dispatcher_;
    ScopedVector<Task> tasks_;

    DISALLOW_COPY_AND_ASSIGN(MultiStepAsyncCallHandler);
  };

  static void CompleteOperation(AsyncCallHandler *reply_handler);
  static void CompleteOperation(AsyncCallHandler *reply_handler,
                                const Error &error);
  // Generic synthetic callback by which a Capability class can indicate
  // that a requested operation is not supported.
  void static OnUnsupportedOperation(const char *operation,
                                     AsyncCallHandler *call_handler);

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
  FRIEND_TEST(CellularCapabilityTest, GetModemInfo);
  FRIEND_TEST(CellularCapabilityTest, GetModemStatus);
  FRIEND_TEST(CellularServiceTest, FriendlyName);
  FRIEND_TEST(CellularTest, StartCDMARegister);
  FRIEND_TEST(CellularTest, StartConnected);
  FRIEND_TEST(CellularTest, StartGSMRegister);
  FRIEND_TEST(CellularTest, StartLinked);
  FRIEND_TEST(CellularTest, Connect);
  FRIEND_TEST(CellularTest, DisconnectModem);

  Cellular *cellular_;

  // Store cached copies of singletons for speed/ease of testing.
  ProxyFactory *proxy_factory_;

  scoped_ptr<ModemProxyInterface> proxy_;
  scoped_ptr<ModemSimpleProxyInterface> simple_proxy_;

  DISALLOW_COPY_AND_ASSIGN(CellularCapability);
};

}  // namespace shill

#endif  // SHILL_CELLULAR_CAPABILITY_
