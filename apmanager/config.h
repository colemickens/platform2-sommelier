// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APMANAGER_CONFIG_H_
#define APMANAGER_CONFIG_H_

#include <memory>
#include <string>

#include <base/macros.h>
#include <chromeos/errors/error.h>

#include "apmanager/dbus_adaptors/org.chromium.apmanager.Config.h"

namespace apmanager {

class Device;
class Manager;

class Config
    : public org::chromium::apmanager::ConfigAdaptor,
      public org::chromium::apmanager::ConfigInterface {
 public:
  Config(Manager* manager, const std::string& service_path);
  virtual ~Config();

  // Register Config DBus object.
  void RegisterAsync(
      chromeos::dbus_utils::ExportedObjectManager* object_manager,
      chromeos::dbus_utils::AsyncEventSequencer* sequencer);

  // Generate a config file string for a hostapd instance. Raise appropriate
  // error when encounter invalid configuration. Return true if success,
  // false otherwise.
  virtual bool GenerateConfigFile(chromeos::ErrorPtr* error,
                                  std::string* config_str);

  // Claim and release the device needed for this configuration.
  virtual bool ClaimDevice();
  virtual bool ReleaseDevice();

  const std::string& control_interface() const { return control_interface_; }
  void set_control_interface(const std::string& control_interface) {
    control_interface_ = control_interface;
  }

  const dbus::ObjectPath& dbus_path() const { return dbus_path_; }

 private:
  // Keys used in hostapd config file.
  static const char kHostapdConfigKeyBridgeInterface[];
  static const char kHostapdConfigKeyChannel[];
  static const char kHostapdConfigKeyControlInterface[];
  static const char kHostapdConfigKeyDriver[];
  static const char kHostapdConfigKeyFragmThreshold[];
  static const char kHostapdConfigKeyHwMode[];
  static const char kHostapdConfigKeyIeee80211ac[];
  static const char kHostapdConfigKeyIeee80211n[];
  static const char kHostapdConfigKeyIgnoreBroadcastSsid[];
  static const char kHostapdConfigKeyInterface[];
  static const char kHostapdConfigKeyRsnPairwise[];
  static const char kHostapdConfigKeyRtsThreshold[];
  static const char kHostapdConfigKeySsid[];
  static const char kHostapdConfigKeyWepDefaultKey[];
  static const char kHostapdConfigKeyWepKey0[];
  static const char kHostapdConfigKeyWpa[];
  static const char kHostapdConfigKeyWpaKeyMgmt[];
  static const char kHostapdConfigKeyWpaPassphrase[];

  // Hardware mode value for hostapd config file.
  static const char kHostapdHwMode80211a[];
  static const char kHostapdHwMode80211b[];
  static const char kHostapdHwMode80211g[];

  // Default hostapd configuration values. User will not be able to configure
  // these.
  static const char kHostapdDefaultDriver[];
  static const char kHostapdDefaultRsnPairwise[];
  static const char kHostapdDefaultWpaKeyMgmt[];
  static const int kHostapdDefaultFragmThreshold;
  static const int kHostapdDefaultRtsThreshold;

  // Default config property values.
  static const uint16_t kPropertyDefaultChannel;;
  static const bool kPropertyDefaultHiddenNetwork;
  static const uint16_t kPropertyDefaultServerAddressIndex;

  // Append default hostapd configurations to the config file.
  bool AppendHostapdDefaults(chromeos::ErrorPtr* error,
                             std::string* config_str);

  // Append hardware mode related configurations to the config file.
  bool AppendHwMode(chromeos::ErrorPtr* error, std::string* config_str);

  // Determine/append interface configuration to the config file.
  bool AppendInterface(chromeos::ErrorPtr* error, std::string* config_str);

  // Append security related configurations to the config file.
  bool AppendSecurityMode(chromeos::ErrorPtr* error, std::string* config_str);

  Manager* manager_;
  dbus::ObjectPath dbus_path_;
  std::string control_interface_;
  std::unique_ptr<chromeos::dbus_utils::DBusObject> dbus_object_;
  scoped_refptr<Device> device_;

  DISALLOW_COPY_AND_ASSIGN(Config);
};

}  // namespace apmanager

#endif  // APMANAGER_CONFIG_H_
