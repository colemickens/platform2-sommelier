// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLUETOOTH_DISPATCHER_TEST_HELPER_H_
#define BLUETOOTH_DISPATCHER_TEST_HELPER_H_

#include <string>

#include <dbus/message.h>
#include <dbus/object_proxy.h>

namespace bluetooth {

// A fake D-Bus method handler. What it does is reply with a test response
// message if the MethodCall is as expected.
void StubHandleMethod(const std::string& expected_interface_name,
                      const std::string& expected_method_name,
                      const std::string& expected_payload,
                      const std::string& response_string,
                      const std::string& error_name,
                      const std::string& error_message,
                      dbus::MethodCall* method_call,
                      int timeout_ms,
                      dbus::ObjectProxy::ResponseCallback callback,
                      dbus::ObjectProxy::ErrorCallback error_callback);

}  // namespace bluetooth

#endif  // BLUETOOTH_DISPATCHER_TEST_HELPER_H_
