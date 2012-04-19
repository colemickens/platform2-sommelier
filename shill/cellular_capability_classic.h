// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_CELLULAR_CAPABILITY_CLASSIC_
#define SHILL_CELLULAR_CAPABILITY_CLASSIC_

#include <string>
#include <vector>

#include <base/basictypes.h>
#include <base/callback.h>
#include <base/memory/scoped_ptr.h>
#include <base/memory/weak_ptr.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST

#include "shill/cellular_capability.h"
#include "shill/dbus_properties.h"
#include "shill/modem_proxy_interface.h"
#include "shill/modem_simple_proxy_interface.h"

namespace shill {

class Cellular;
class Error;
class EventDispatcher;
class ProxyFactory;

// CellularCapabilityClassic handles modems using the
// org.chromium.ModemManager DBUS interface.
class CellularCapabilityClassic : public CellularCapability {
 public:
  static const char kConnectPropertyApn[];
  static const char kConnectPropertyApnUsername[];
  static const char kConnectPropertyApnPassword[];
  static const char kConnectPropertyHomeOnly[];
  static const char kConnectPropertyPhoneNumber[];

  // |cellular| is the parent Cellular device.
  CellularCapabilityClassic(Cellular *cellular, ProxyFactory *proxy_factory);
  virtual ~CellularCapabilityClassic();

  virtual void StopModem(Error *error, const ResultCallback &callback);
  virtual void Connect(const DBusPropertiesMap &properties, Error *error,
                       const ResultCallback &callback);
  virtual void Disconnect(Error *error, const ResultCallback &callback);

  virtual void Activate(const std::string &carrier,
                        Error *error, const ResultCallback &callback);

  // Network registration.
  virtual void RegisterOnNetwork(const std::string &network_id,
                                 Error *error,
                                 const ResultCallback &callback);

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

  virtual void Scan(Error *error, const ResultCallback &callback);

 protected:
  // The following five methods are only ever called as
  // callbacks (from the main loop), which is why they
  // don't take an Error * argument.
  virtual void EnableModem(const ResultCallback &callback);
  virtual void DisableModem(const ResultCallback &callback);
  virtual void GetModemStatus(const ResultCallback &callback);
  virtual void GetModemInfo(const ResultCallback &callback);
  virtual void GetProperties(const ResultCallback &callback) = 0;

  void FinishEnable(const ResultCallback &callback);
  void FinishDisable(const ResultCallback &callback);
  virtual void InitProxies();
  virtual void ReleaseProxies();

  static void OnUnsupportedOperation(const char *operation, Error *error);

  // Properties
  bool scanning_supported_;
  std::string meid_;
  std::string imsi_;
  std::string imei_;
  std::string esn_;
  std::string mdn_;
  std::string min_;
  std::string model_id_;
  std::string manufacturer_;
  std::string firmware_revision_;
  std::string hardware_revision_;
  std::string carrier_;

  scoped_ptr<ModemSimpleProxyInterface> simple_proxy_;

 private:
  friend class CellularTest;
  friend class CellularCapabilityTest;
  friend class CellularCapabilityGSMTest;
  FRIEND_TEST(CellularCapabilityGSMTest, SetProxy);
  FRIEND_TEST(CellularCapabilityGSMTest, SetStorageIdentifier);
  FRIEND_TEST(CellularCapabilityGSMTest, UpdateStatus);
  FRIEND_TEST(CellularCapabilityTest, AllowRoaming);
  FRIEND_TEST(CellularCapabilityTest, EnableModemFail);
  FRIEND_TEST(CellularCapabilityTest, EnableModemSucceed);
  FRIEND_TEST(CellularCapabilityTest, FinishEnable);
  FRIEND_TEST(CellularCapabilityTest, GetModemInfo);
  FRIEND_TEST(CellularCapabilityTest, GetModemStatus);
  FRIEND_TEST(CellularCapabilityTest, TryApns);
  FRIEND_TEST(CellularServiceTest, FriendlyName);
  FRIEND_TEST(CellularTest, StartCDMARegister);
  FRIEND_TEST(CellularTest, StartConnected);
  FRIEND_TEST(CellularTest, StartGSMRegister);
  FRIEND_TEST(CellularTest, StartLinked);
  FRIEND_TEST(CellularTest, Connect);
  FRIEND_TEST(CellularTest, ConnectFailure);
  FRIEND_TEST(CellularTest, Disconnect);

  void HelpRegisterDerivedBool(
      const std::string &name,
      bool(CellularCapability::*get)(Error *error),
      void(CellularCapability::*set)(const bool &value, Error *error));

  // Method reply and signal callbacks from Modem interface
  void OnModemStateChangedSignal(
      uint32 old_state, uint32 new_state, uint32 reason);
  void OnGetModemInfoReply(const ResultCallback &callback,
                           const ModemHardwareInfo &info,
                           const Error &error);

  // Method reply callbacks from Modem.Simple interface
  void OnGetModemStatusReply(const ResultCallback &callback,
                             const DBusPropertiesMap &props,
                             const Error &error);

  Cellular *cellular_;
  base::WeakPtrFactory<CellularCapabilityClassic> weak_ptr_factory_;

  scoped_ptr<ModemProxyInterface> proxy_;

  DISALLOW_COPY_AND_ASSIGN(CellularCapabilityClassic);
};

}  // namespace shill

#endif  // SHILL_CELLULAR_CAPABILITY_CLASSIC_
