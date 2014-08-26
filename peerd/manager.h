// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PEERD_MANAGER_H_
#define PEERD_MANAGER_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include <base/basictypes.h>
#include <chromeos/async_event_sequencer.h>
#include <chromeos/dbus/dbus_object.h>

struct sockaddr_storage;

namespace dbus {
class Bus;
}  // namespace dbus

namespace peerd {

// Manages global state of peerd.
class Manager {
 public:
  explicit Manager(const scoped_refptr<dbus::Bus>& bus);
  virtual ~Manager() = default;
  void RegisterAsync(
      const chromeos::dbus_utils::AsyncEventSequencer::CompletionAction&
          completion_callback);

  // DBus handlers
  std::string StartMonitoring(
      chromeos::ErrorPtr* error,
      const std::set<std::string>& technologies);

  void StopMonitoring(
      chromeos::ErrorPtr* error,
      const std::string& monitoring_token);

  std::string ExposeIpService(
      chromeos::ErrorPtr* error,
      const std::string& service_id,
      const std::vector<sockaddr_storage>& addresses,
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

  friend class ManagerDBusProxyTest;
  DISALLOW_COPY_AND_ASSIGN(Manager);
};

}  // namespace peerd

#endif  // PEERD_MANAGER_H_
