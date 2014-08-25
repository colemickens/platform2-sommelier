// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PEERD_MANAGER_DBUS_PROXY_H_
#define PEERD_MANAGER_DBUS_PROXY_H_

#include <map>
#include <string>
#include <vector>

#include <base/basictypes.h>
#include <chromeos/dbus/dbus_object.h>
#include <chromeos/error.h>
#include <dbus/bus.h>
#include <dbus/exported_object.h>
#include <dbus/message.h>
#include <gtest/gtest_prod.h>

struct sockaddr_storage;

namespace peerd {

class Manager;

class ManagerDBusProxy {
 public:
  ManagerDBusProxy(const scoped_refptr<dbus::Bus>& bus,
                   Manager* manager);

  void RegisterAsync(
      const chromeos::dbus_utils::AsyncEventSequencer::CompletionAction&
          completion_callback);

 private:
  std::string HandleStartMonitoring(
      chromeos::ErrorPtr* error,
      const chromeos::dbus_utils::Dictionary& technologies);

  void HandleStopMonitoring(
      chromeos::ErrorPtr* error,
      const std::string& monitoring_token);

  std::string HandleExposeIpService(
      chromeos::ErrorPtr* error,
      const std::string& service_id,
      const std::vector<sockaddr_storage>& ip_addresses,
      const std::map<std::string, std::string>& service_info,
      const chromeos::dbus_utils::Dictionary& options);

  void HandleRemoveExposedService(
      chromeos::ErrorPtr* error,
      const std::string& service_token);

  void HandleSetFriendlyName(
      chromeos::ErrorPtr* error,
      const std::string& name);

  void HandleSetNote(chromeos::ErrorPtr* error, const std::string& note);

  std::string HandlePing(chromeos::ErrorPtr* error);

  scoped_refptr<dbus::Bus> bus_;
  chromeos::dbus_utils::DBusObject dbus_object_;
  Manager* manager_ = nullptr;  // Outlives this.

  FRIEND_TEST(ManagerDBusProxyTest, HandleStartMonitoring_NoArgs);
  FRIEND_TEST(ManagerDBusProxyTest, StartMonitoring_ReturnsString);
  FRIEND_TEST(ManagerDBusProxyTest, HandleStopMonitoring_NoArgs);
  FRIEND_TEST(ManagerDBusProxyTest, HandleStopMonitoring_ExtraArgs);
  FRIEND_TEST(ManagerDBusProxyTest, HandleExposeIpService_NoArgs);
  FRIEND_TEST(ManagerDBusProxyTest, HandleExposeIpService_OnlyServiceId);
  FRIEND_TEST(ManagerDBusProxyTest, HandleExposeIpService_MalformedIps);
  FRIEND_TEST(ManagerDBusProxyTest, HandleExposeIpService_MissingServiceDict);
  FRIEND_TEST(ManagerDBusProxyTest, HandleExposeIpService_MissingOptionsDict);
  FRIEND_TEST(ManagerDBusProxyTest, HandleExposeIpService_ReturnsServiceToken);
  FRIEND_TEST(ManagerDBusProxyTest, HandleExposeIpService_ExtraArgs);
  FRIEND_TEST(ManagerDBusProxyTest, HandleRemoveExposedService_NoArgs);
  FRIEND_TEST(ManagerDBusProxyTest, HandleRemoveExposedService_ExtraArgs);
  FRIEND_TEST(ManagerDBusProxyTest, HandleSetFriendlyName_NoArgs);
  FRIEND_TEST(ManagerDBusProxyTest, HandleSetFriendlyName_ExtraArgs);
  FRIEND_TEST(ManagerDBusProxyTest, HandleSetNote_NoArgs);
  FRIEND_TEST(ManagerDBusProxyTest, HandleSetNote_ExtraArgs);
  FRIEND_TEST(ManagerDBusProxyTest, HandlePing_ReturnsHelloWorld);
  FRIEND_TEST(ManagerDBusProxyTest, HandlePing_WithArgs);
  DISALLOW_COPY_AND_ASSIGN(ManagerDBusProxy);
};

}  // namespace peerd

#endif  // PEERD_MANAGER_DBUS_PROXY_H_
