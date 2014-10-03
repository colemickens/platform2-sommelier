// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PEERD_MANAGER_H_
#define PEERD_MANAGER_H_

#include <map>
#include <set>
#include <stdint.h>
#include <string>
#include <vector>

#include <base/macros.h>
#include <chromeos/dbus/dbus_object.h>
#include <chromeos/errors/error.h>

#include "peerd/avahi_client.h"
#include "peerd/ip_addr.h"
#include "peerd/peer_manager_interface.h"
#include "peerd/published_peer.h"
#include "peerd/technologies.h"
#include "peerd/typedefs.h"

namespace dbus {
class Bus;
}  // namespace dbus

namespace chromeos {
namespace dbus_utils {
class ExportedObjectManager;
}  // dbus_utils
}  // chromeos

namespace peerd {

namespace errors {
namespace manager {

extern const char kInvalidServiceToken[];
extern const char kInvalidMonitoringTechnology[];
extern const char kInvalidMonitoringToken[];

}  // namespace manager
}  // namespace errors

// Manages global state of peerd.
class Manager {
 public:
  explicit Manager(chromeos::dbus_utils::ExportedObjectManager* object_manager);
  virtual ~Manager() = default;
  void RegisterAsync(const CompletionAction& completion_callback);

  // DBus handlers
  std::string StartMonitoring(
      chromeos::ErrorPtr* error,
      const std::vector<technologies::tech_t>& requested_technologies);

  void StopMonitoring(
      chromeos::ErrorPtr* error,
      const std::string& monitoring_token);

  std::string ExposeService(
      chromeos::ErrorPtr* error,
      const std::string& service_id,
      const std::map<std::string, std::string>& service_info);

  void RemoveExposedService(
      chromeos::ErrorPtr* error,
      const std::string& service_token);

  void SetFriendlyName(
      chromeos::ErrorPtr* error,
      const std::string& friendly_name);

  void SetNote(chromeos::ErrorPtr* error, const std::string& note);

  std::string Ping(chromeos::ErrorPtr* error);

 private:
  // Used in unit tests to inject mocks.
  Manager(std::unique_ptr<chromeos::dbus_utils::DBusObject> dbus_object,
          std::unique_ptr<PublishedPeer> self,
          std::unique_ptr<PeerManagerInterface> peer_manager,
          std::unique_ptr<AvahiClient> avahi_client);

  // Called from AvahiClient.
  void ShouldRefreshAvahiPublisher();

  std::unique_ptr<chromeos::dbus_utils::DBusObject> dbus_object_;
  std::unique_ptr<PublishedPeer> self_;
  std::unique_ptr<PeerManagerInterface> peer_manager_;
  std::unique_ptr<AvahiClient> avahi_client_;
  std::map<std::string, std::string> service_token_to_id_;
  std::map<std::string, technologies::tech_t> monitoring_requests_;
  size_t services_added_{0};
  size_t monitoring_tokens_issued_{0};

  friend class ManagerDBusProxyTest;
  friend class ManagerTest;
  DISALLOW_COPY_AND_ASSIGN(Manager);
};

}  // namespace peerd

#endif  // PEERD_MANAGER_H_
