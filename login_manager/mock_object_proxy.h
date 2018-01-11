// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOGIN_MANAGER_MOCK_OBJECT_PROXY_H_
#define LOGIN_MANAGER_MOCK_OBJECT_PROXY_H_

#include <memory>
#include <string>

#include <dbus/object_path.h>
#include <dbus/object_proxy.h>
#include <gmock/gmock.h>

namespace dbus {
class MethodCall;
class Response;
}  // namespace dbus

namespace login_manager {

// Mock for ObjectProxy.
class MockObjectProxy : public dbus::ObjectProxy {
 public:
  MockObjectProxy()
      : dbus::ObjectProxy(NULL,
                          "",
                          dbus::ObjectPath(""),
                          dbus::ObjectProxy::DEFAULT_OPTIONS) {}
  // GMock doesn't support the return type of unique_ptr<> because unique_ptr is
  // uncopyable. This is a workaround which defines |MockCallMethodAndBlock| as
  // a mock method and makes |CallMethodAndBlock| call the mocked method.
  // Use |MockCallMethodAndBlock| for setting/testing expectations.
  MOCK_METHOD2(MockCallMethodAndBlock,
               dbus::Response*(dbus::MethodCall* method_call, int timeout_ms));
  std::unique_ptr<dbus::Response> CallMethodAndBlock(
      dbus::MethodCall* method_call, int timeout_ms) override {
    return std::unique_ptr<dbus::Response>(
        MockCallMethodAndBlock(method_call, timeout_ms));
  }

  MOCK_METHOD3(CallMethod,
               void(dbus::MethodCall* method_call,
                    int timeout_ms,
                    dbus::ObjectProxy::ResponseCallback callback));
  MOCK_METHOD4(CallMethodWithErrorCallback,
               void(dbus::MethodCall* method_call,
                    int timeout_ms,
                    dbus::ObjectProxy::ResponseCallback callback,
                    dbus::ObjectProxy::ErrorCallback error_callback));
  MOCK_METHOD4(
      ConnectToSignal,
      void(const std::string& interface_name,
           const std::string& signal_name,
           dbus::ObjectProxy::SignalCallback signal_callback,
           dbus::ObjectProxy::OnConnectedCallback on_connected_callback));
  MOCK_METHOD0(Detach, void());
  MOCK_METHOD1(WaitForServiceToBeAvailable,
               void(WaitForServiceToBeAvailableCallback callback));

 protected:
  ~MockObjectProxy() override {}
};

}  // namespace login_manager

#endif  // LOGIN_MANAGER_MOCK_OBJECT_PROXY_H_
