// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_WIFI_SERVICE_
#define SHILL_WIFI_SERVICE_

#include <set>
#include <string>
#include <vector>

#include "shill/dbus_bindings/supplicant-interface.h"
#include "shill/event_dispatcher.h"
#include "shill/refptr_types.h"
#include "shill/service.h"

namespace shill {

class ControlInterface;
class EventDispatcher;
class Error;
class Manager;
class Metrics;
class NSS;

class WiFiService : public Service {
 public:
  // TODO(pstew): Storage constants shouldn't need to be public
  // crosbug.com/25813
  static const char kStorageHiddenSSID[];
  static const char kStorageMode[];
  static const char kStoragePassphrase[];
  static const char kStorageSecurity[];
  static const char kStorageSSID[];

  WiFiService(ControlInterface *control_interface,
              EventDispatcher *dispatcher,
              Metrics *metrics,
              Manager *manager,
              const WiFiRefPtr &device,
              const std::vector<uint8_t> &ssid,
              const std::string &mode,
              const std::string &security,
              bool hidden_ssid);
  ~WiFiService();

  // Inherited from Service.
  virtual void AutoConnect();
  virtual void Connect(Error *error);
  virtual void Disconnect(Error *error);

  virtual bool IsConnecting() const;

  virtual void AddEndpoint(const WiFiEndpointConstRefPtr endpoint);
  virtual void RemoveEndpoint(const WiFiEndpointConstRefPtr endpoint);
  bool NumEndpoints() const { return endpoints_.size(); }
  virtual bool Is8021x() const;


  // Called to update the identity of the currently connected endpoint.
  // To indicate that there is no currently connect endpoint, call with
  // |endpoint| set to NULL.
  void NotifyCurrentEndpoint(const WiFiEndpoint *endpoint);
  // Called to inform of changes in the properties of an endpoint.
  // (Not necessarily the currently connected endpoint.)
  void NotifyEndpointUpdated(const WiFiEndpoint &endpoint);

  // wifi_<MAC>_<BSSID>_<mode_string>_<security_string>
  std::string GetStorageIdentifier() const;
  static bool ParseStorageIdentifier(const std::string &storage_name,
                                     std::string *address,
                                     std::string *mode,
                                     std::string *security);

  const std::string &mode() const { return mode_; }
  const std::string &key_management() const { return GetEAPKeyManagement(); }
  const std::vector<uint8_t> &ssid() const { return ssid_; }

  void SetPassphrase(const std::string &passphrase, Error *error);

  // Overrride Load and Save from parent Service class.  We will call
  // the parent method.
  virtual bool IsLoadableFrom(StoreInterface *storage) const;
  virtual bool Load(StoreInterface *storage);
  virtual bool Save(StoreInterface *storage);
  virtual bool Unload();

  virtual bool HasEndpoints() const { return !endpoints_.empty(); }
  virtual bool IsVisible() const;
  bool IsSecurityMatch(const std::string &security) const;
  bool hidden_ssid() const { return hidden_ssid_; }

  virtual void InitializeCustomMetrics() const;
  virtual void SendPostReadyStateMetrics(
      int64 time_resume_to_ready_milliseconds) const;

  // Override from parent Service class to correctly update connectability
  // when the EAP credentials change for 802.1x networks.
  void set_eap(const EapCredentials& eap);

  // Override from parent Service class to register hidden services once they
  // have been configured.
  virtual void OnProfileConfigured();

 protected:
  virtual bool IsAutoConnectable(const char **reason) const;

 private:
  friend class WiFiServiceSecurityTest;
  friend class WiFiServiceUpdateFromEndpointsTest;  // SignalToStrength
  FRIEND_TEST(MetricsTest, WiFiServicePostReady);
  FRIEND_TEST(WiFiMainTest, CurrentBSSChangedUpdateServiceEndpoint);
  FRIEND_TEST(WiFiServiceTest, AutoConnect);
  FRIEND_TEST(WiFiServiceTest, ClearWriteOnlyDerivedProperty);  // passphrase_
  FRIEND_TEST(WiFiServiceTest, ConnectTask8021x);
  FRIEND_TEST(WiFiServiceTest, ConnectTaskDynamicWEP);
  FRIEND_TEST(WiFiServiceTest, ConnectTaskPSK);
  FRIEND_TEST(WiFiServiceTest, ConnectTaskRSN);
  FRIEND_TEST(WiFiServiceTest, ConnectTaskWEP);
  FRIEND_TEST(WiFiServiceTest, ConnectTaskWPA);
  FRIEND_TEST(WiFiServiceTest, IsAutoConnectable);
  FRIEND_TEST(WiFiServiceTest, LoadHidden);
  FRIEND_TEST(WiFiServiceTest, LoadAndUnloadPassphrase);
  FRIEND_TEST(WiFiServiceTest, Populate8021x);
  FRIEND_TEST(WiFiServiceTest, Populate8021xNoSystemCAs);
  FRIEND_TEST(WiFiServiceTest, Populate8021xUsingHardwareAuth);
  FRIEND_TEST(WiFiServiceTest, Populate8021xNSS);
  FRIEND_TEST(WiFiServiceTest, SignalToStrength);  // SignalToStrength

  static const char kAutoConnBusy[];
  static const char kAutoConnNoEndpoint[];

  // Override the base clase implementation, because we need to allow
  // arguments that aren't base class methods.
  void HelpRegisterWriteOnlyDerivedString(
      const std::string &name,
      void(WiFiService::*set)(const std::string &value, Error *error),
      void(WiFiService::*clear)(Error *error),
      const std::string *default_value);

  std::string GetDeviceRpcId(Error *error);
  void ClearPassphrase(Error *error);
  void UpdateConnectable();
  void UpdateFromEndpoints();

  static void ValidateWEPPassphrase(const std::string &passphrase,
                                    Error *error);
  static void ValidateWPAPassphrase(const std::string &passphrase,
                                    Error *error);
  static void ParseWEPPassphrase(const std::string &passphrase,
                                 int *key_index,
                                 std::vector<uint8> *password_bytes,
                                 Error *error);
  static bool CheckWEPIsHex(const std::string &passphrase, Error *error);
  static bool CheckWEPKeyIndex(const std::string &passphrase, Error *error);
  static bool CheckWEPPrefix(const std::string &passphrase, Error *error);

  // Maps a signal value, in dBm, to a "strength" value, from
  // |Service::kStrengthMin| to |Service:kStrengthMax|.
  static uint8 SignalToStrength(int16 signal_dbm);

  // "wpa", "rsn" and "psk" are equivalent from a configuration perspective.
  // This function maps them all into "psk".
  static std::string GetSecurityClass(const std::string &security);

  // Profile data for a WPA/RSN service can be stored under a number of
  // different names.  These functions create different storage identifiers
  // based on whether they are referred to by their generic "psk" name or
  // if they use the (legacy) specific "wpa" or "rsn" names.
  std::string GetGenericStorageIdentifier() const;
  std::string GetSpecificStorageIdentifier() const;
  std::string GetStorageIdentifierForSecurity(
      const std::string &security) const;

  // Populate the |params| map with available 802.1x EAP properties.
  void Populate8021xProperties(std::map<std::string, DBus::Variant> *params);

  // Properties
  std::string passphrase_;
  bool need_passphrase_;
  std::string security_;
  // TODO(cmasone): see if the below can be pulled from the endpoint associated
  // with this service instead.
  const std::string mode_;
  std::string auth_mode_;
  bool hidden_ssid_;
  uint16 frequency_;
  // TODO(quiche): I noticed this is not hooked up to anything.  In fact, it
  // was undefined until now. crosbug.com/26490
  uint16 physical_mode_;
  std::string hex_ssid_;
  std::string storage_identifier_;
  std::string bssid_;

  // Track whether or not we've warned about large signal values.
  // Used to avoid spamming the log.
  static bool logged_signal_warning;
  WiFiRefPtr wifi_;
  std::set<WiFiEndpointConstRefPtr> endpoints_;
  WiFiEndpointConstRefPtr current_endpoint_;
  const std::vector<uint8_t> ssid_;
  NSS *nss_;
  DISALLOW_COPY_AND_ASSIGN(WiFiService);
};

}  // namespace shill

#endif  // SHILL_WIFI_SERVICE_
