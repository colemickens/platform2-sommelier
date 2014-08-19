// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PEERD_MANAGER_DBUS_PROXY_H_
#define PEERD_MANAGER_DBUS_PROXY_H_

#include <base/basictypes.h>
#include <dbus/bus.h>
#include <dbus/exported_object.h>
#include <dbus/message.h>
#include <gtest/gtest_prod.h>

#include "peerd/typedefs.h"

namespace peerd {

class Manager;

class ManagerDBusProxy {
 public:
  ManagerDBusProxy(const scoped_refptr<dbus::Bus>& bus,
                   Manager* manager);
  ~ManagerDBusProxy();

  void Init(const OnInitFinish& success_cb);

 private:
  void HandleStartMonitoring(
      dbus::MethodCall* method_call,
      dbus::ExportedObject::ResponseSender response_sender);

  void HandleStopMonitoring(
      dbus::MethodCall* method_call,
      dbus::ExportedObject::ResponseSender response_sender);

  void HandleExposeIpService(
      dbus::MethodCall* method_call,
      dbus::ExportedObject::ResponseSender response_sender);

  void HandleRemoveExposedService(
      dbus::MethodCall* method_call,
      dbus::ExportedObject::ResponseSender response_sender);

  void HandleSetFriendlyName(
      dbus::MethodCall* method_call,
      dbus::ExportedObject::ResponseSender response_sender);

  void HandleSetNote(
      dbus::MethodCall* method_call,
      dbus::ExportedObject::ResponseSender response_sender);

  void HandlePing(dbus::MethodCall* method_call,
                  dbus::ExportedObject::ResponseSender response_sender);

  scoped_refptr<dbus::Bus> bus_;
  // |exported_object_| is owned by the bus object.
  dbus::ExportedObject* exported_object_ = nullptr;
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
