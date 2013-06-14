// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_CELLULAR_CAPABILITY_H_
#define SHILL_CELLULAR_CAPABILITY_H_

#include <string>
#include <vector>

#include <base/basictypes.h>
#include <base/callback.h>
#include <base/memory/scoped_ptr.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST

#include "shill/callbacks.h"
#include "shill/cellular.h"
#include "shill/dbus_properties.h"
#include "shill/metrics.h"

namespace shill {

class Cellular;
class Error;
class ModemInfo;
class ProxyFactory;

// Cellular devices instantiate subclasses of CellularCapability that
// handle the specific modem technologies and capabilities.
//
// The CellularCapability is directly subclassed by:
// *  CelllularCapabilityUniversal which handles all modems managed by
//    a modem manager using the the org.chromium.ModemManager1 DBUS
//    interface.
// *  CellularCapabilityClassic which handles all modems managed by a
//    modem manager using the older org.chromium.ModemManager DBUS
//    interface.  This class is further subclassed to represent CDMA
//    and GSM modems.
//
// Pictorially:
//
// CellularCapability
//       |
//       |-- CellularCapabilityUniversal
//       |            |
//       |            |-- CellularCapabilityUniversalCDMA
//       |
//       |-- CellularCapabilityClassic
//                    |
//                    |-- CellularCapabilityGSM
//                    |
//                    |-- CellularCapabilityCDMA
//
// TODO(armansito): Currently, 3GPP logic is handled by
// CellularCapabilityUniversal. Eventually CellularCapabilityUniversal will
// only serve as the abstract base class for ModemManager1 3GPP and CDMA
// capabilities.
class CellularCapability {
 public:
  static const int kTimeoutActivate;
  static const int kTimeoutConnect;
  static const int kTimeoutDefault;
  static const int kTimeoutDisconnect;
  static const int kTimeoutEnable;
  static const int kTimeoutRegister;
  static const int kTimeoutReset;
  static const int kTimeoutScan;

  static const char kModemPropertyIMSI[];
  static const char kModemPropertyState[];

  // |cellular| is the parent Cellular device.
  CellularCapability(Cellular *cellular,
                     ProxyFactory *proxy_factory,
                     ModemInfo *modem_info);
  virtual ~CellularCapability();

  Cellular *cellular() const { return cellular_; }
  ProxyFactory *proxy_factory() const { return proxy_factory_; }
  ModemInfo *modem_info() const { return modem_info_; }

  // Invoked by the parent Cellular device when a new service is created.
  virtual void OnServiceCreated() = 0;

  virtual void SetupConnectProperties(DBusPropertiesMap *properties) = 0;

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
  // StopModem disconnects and disables a modem asynchronously.
  // |callback| is invoked when this completes and the result is passed
  // to the callback.
  virtual void StopModem(Error *error, const ResultCallback &callback) = 0;
  virtual void Connect(const DBusPropertiesMap &properties, Error *error,
                       const ResultCallback &callback) = 0;
  virtual void Disconnect(Error *error, const ResultCallback &callback) = 0;
  // Called when a disconnect completes, successful or not.
  virtual void DisconnectCleanup() = 0;

  // Activates the modem. Returns an Error on failure.
  virtual void Activate(const std::string &carrier,
                        Error *error, const ResultCallback &callback) = 0;

  // Initiates the necessary to steps to verify that the cellular service has
  // been activated. Once these steps have been completed, the service should
  // be marked as activated.
  //
  // The default implementation fails by returning a kNotSupported error
  // to the caller.
  virtual void CompleteActivation(Error *error);

  // Returns true if service activation is required. Returns false by default
  // in this base class.
  virtual bool IsServiceActivationRequired() const;

  // Network registration.
  virtual void RegisterOnNetwork(const std::string &network_id,
                                 Error *error,
                                 const ResultCallback &callback) = 0;
  virtual bool IsRegistered() = 0;
  // If we are informed by means of something other than a signal indicating
  // a registration state change that the modem has unregistered from the
  // network, we need to update the network-type-specific capability object.
  virtual void SetUnregistered(bool searching) = 0;

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

  // The default implementation fails by returning an error.
  virtual void Reset(Error *error, const ResultCallback &callback);
  virtual void SetCarrier(const std::string &carrier,
                          Error *error, const ResultCallback &callback);

  // Asks the modem to scan for networks.
  //
  // The default implementation fails by filling error with
  // kNotSupported.
  //
  // Subclasses should implement this by fetching scan results
  // asynchronously.  When the results are ready, update the
  // flimflam::kFoundNetworksProperty and send a property change
  // notification.  Finally, callback must be invoked to inform the
  // caller that the scan has completed.
  //
  // Errors are not generally reported, but on error the
  // kFoundNetworksProperty should be cleared and a property change
  // notification sent out.
  //
  // TODO(jglasgow): Refactor to reuse code by putting notification
  // logic into Cellular or CellularCapability.
  //
  // TODO(jglasgow): Implement real error handling.
  virtual void Scan(Error *error, const ResultCallback &callback);

  // Returns an empty string if the network technology is unknown.
  virtual std::string GetNetworkTechnologyString() const = 0;

  virtual std::string GetRoamingStateString() const = 0;

  virtual void GetSignalQuality() = 0;

  virtual std::string GetTypeString() const = 0;

  // Called when ModemManager has sent a property change notification
  // signal over DBUS.
  virtual void OnDBusPropertiesChanged(
      const std::string &interface,
      const DBusPropertiesMap &changed_properties,
      const std::vector<std::string> &invalidated_properties) = 0;

  // Should this device allow roaming?
  // The decision to allow roaming or not is based on the home
  // provider as well as on the user modifiable "allow_roaming"
  // property.
  virtual bool AllowRoaming() = 0;

  virtual bool IsActivating() const;

  // Returns true if the cellular device should initiate passive traffic
  // monitoring to trigger active out-of-credit detection checks. This
  // implementation returns false by default.
  virtual bool ShouldDetectOutOfCredit() const;

 protected:
  // Releases all proxies held by the object.  This is most useful
  // during unit tests.
  virtual void ReleaseProxies() = 0;

  static void OnUnsupportedOperation(const char *operation, Error *error);

  // accessor for subclasses to read the allow roaming property
  bool allow_roaming_property() const {
    return cellular_->allow_roaming_property();
  }

 private:
  friend class CellularCapabilityGSMTest;
  friend class CellularCapabilityTest;
  friend class CellularCapabilityUniversalTest;
  friend class CellularCapabilityUniversalCDMATest;
  friend class CellularTest;
  FRIEND_TEST(CellularCapabilityTest, AllowRoaming);
  FRIEND_TEST(CellularTest, Connect);
  FRIEND_TEST(CellularTest, TearDown);

  Cellular *cellular_;

  ProxyFactory *proxy_factory_;
  ModemInfo *modem_info_;

  DISALLOW_COPY_AND_ASSIGN(CellularCapability);
};

}  // namespace shill

#endif  // SHILL_CELLULAR_CAPABILITY_H_
