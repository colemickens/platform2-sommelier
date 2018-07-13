// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLUETOOTH_DISPATCHER_DBUS_UTIL_H_
#define BLUETOOTH_DISPATCHER_DBUS_UTIL_H_

#include <memory>
#include <string>

#include <base/memory/ref_counted.h>
#include <dbus/bus.h>
#include <dbus/message.h>

namespace bluetooth {

// D-Bus utilities.
class DBusUtil {
 public:
  using ResponseSender =
      base::Callback<void(std::unique_ptr<dbus::Response> response)>;

  // Forwards a method call to another D-Bus service and send the response back
  // to the original sender.
  static void ForwardMethodCall(scoped_refptr<dbus::Bus> bus,
                                const std::string& destination_service,
                                dbus::MethodCall* method_call,
                                ResponseSender response_sender);
};

}  // namespace bluetooth

#endif  // BLUETOOTH_DISPATCHER_DBUS_UTIL_H_
