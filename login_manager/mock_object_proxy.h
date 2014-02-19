// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOGIN_MANAGER_MOCK_OBJECT_PROXY_H_
#define LOGIN_MANAGER_MOCK_OBJECT_PROXY_H_

#include <string>

#include <dbus/object_path.h>
#include <dbus/object_proxy.h>
#include <gmock/gmock.h>

namespace login_manager {

// Mock for ObjectProxy.
class MockObjectProxy : public dbus::ObjectProxy {
 public:
  MockObjectProxy()
      : dbus::ObjectProxy(NULL,
                          "",
                          dbus::ObjectPath(""),
                          dbus::ObjectProxy::DEFAULT_OPTIONS) {
  }

  MOCK_METHOD2(CallMethodAndBlock,
               dbus::Response*(dbus::MethodCall* method_call, int timeout_ms));
  MOCK_METHOD3(CallMethod, void(dbus::MethodCall* method_call,
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

 protected:
  virtual ~MockObjectProxy() {}
};

}  // namespace login_manager

#endif  // LOGIN_MANAGER_MOCK_OBJECT_PROXY_H_
