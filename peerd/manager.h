// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PEERD_MANAGER_H_
#define PEERD_MANAGER_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include <base/macros.h>
#include <chromeos/dbus/dbus_object.h>
#include <chromeos/errors/error.h>

#include "peerd/ip_addr.h"
#include "peerd/peer.h"
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
      const std::set<std::string>& technologies);

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
  chromeos::dbus_utils::DBusObject dbus_object_;
  std::unique_ptr<Peer> self_;
  std::map<std::string, std::string> service_token_to_id_;
  size_t services_added_{0};

  friend class ManagerDBusProxyTest;
  DISALLOW_COPY_AND_ASSIGN(Manager);
};

}  // namespace peerd

#endif  // PEERD_MANAGER_H_
