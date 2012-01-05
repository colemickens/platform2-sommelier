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

class WiFiService : public Service {
 public:
  WiFiService(ControlInterface *control_interface,
              EventDispatcher *dispatcher,
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

  virtual bool TechnologyIs(const Technology::Identifier type) const;
  virtual bool IsConnecting() const;

  virtual void AddEndpoint(WiFiEndpointConstRefPtr endpoint);
  virtual void RemoveEndpoint(WiFiEndpointConstRefPtr endpoint);
  bool NumEndpoints() const { return endpoints_.size(); }

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

  virtual bool HasEndpoints() const { return !endpoints_.empty(); }
  virtual bool IsVisible() const;
  bool IsSecurityMatch(const std::string &security) const;
  bool hidden_ssid() const { return hidden_ssid_; }

  virtual void InitializeCustomMetrics() const;
  virtual void SendPostReadyStateMetrics() const;

 protected:
  virtual bool IsAutoConnectable() const;

 private:
  friend class WiFiServiceSecurityTest;
  FRIEND_TEST(MetricsTest, WiFiServicePostReady);
  FRIEND_TEST(WiFiServiceTest, AutoConnect);
  FRIEND_TEST(WiFiServiceTest, ConnectTaskRSN);
  FRIEND_TEST(WiFiServiceTest, ConnectTaskWPA);
  FRIEND_TEST(WiFiServiceTest, ConnectTaskPSK);
  FRIEND_TEST(WiFiServiceTest, ConnectTaskWEP);
  FRIEND_TEST(WiFiServiceTest, IsAutoConnectable);
  FRIEND_TEST(WiFiServiceTest, LoadHidden);

  static const char kStorageHiddenSSID[];
  static const char kStorageMode[];
  static const char kStoragePassphrase[];
  static const char kStorageSecurity[];
  static const char kStorageSSID[];

  void HelpRegisterDerivedString(
      PropertyStore *store,
      const std::string &name,
      std::string(WiFiService::*get)(Error *error),
      void(WiFiService::*set)(const std::string &value, Error *error));

  void ConnectTask();
  void DisconnectTask();

  std::string GetDeviceRpcId(Error *error);
  void UpdateConnectable();

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

  // replace non-ASCII characters with '?'. return true if one or more
  // characters were changed
  static bool SanitizeSSID(std::string *ssid);

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

  // Properties
  std::string passphrase_;
  bool need_passphrase_;
  std::string security_;
  uint8 strength_;
  // TODO(cmasone): see if the below can be pulled from the endpoint associated
  // with this service instead.
  const std::string mode_;
  std::string auth_mode_;
  bool hidden_ssid_;
  uint16 frequency_;
  uint16 physical_mode_;
  std::string hex_ssid_;
  std::string storage_identifier_;

  ScopedRunnableMethodFactory<WiFiService> task_factory_;
  WiFiRefPtr wifi_;
  std::set<WiFiEndpointConstRefPtr> endpoints_;
  const std::vector<uint8_t> ssid_;
  DISALLOW_COPY_AND_ASSIGN(WiFiService);
};

}  // namespace shill

#endif  // SHILL_WIFI_SERVICE_
