// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLUETOOTH_DISPATCHER_CATCH_ALL_FORWARDER_H_
#define BLUETOOTH_DISPATCHER_CATCH_ALL_FORWARDER_H_

#include <string>

#include <base/memory/ref_counted.h>
#include <dbus/bus.h>

namespace bluetooth {

// Handles the forwarding of all method calls to a specified destination.
class CatchAllForwarder {
 public:
  // |from_bus|: the D-Bus connection where we listen to all method calls.
  // |to_bus|: the D-Bus connection where we forward the method calls through.
  // |destination|: the D-Bus address to which the method calls are forwarded.
  CatchAllForwarder(const scoped_refptr<dbus::Bus>& from_bus,
                    const scoped_refptr<dbus::Bus>& to_bus,
                    const std::string& destination);
  ~CatchAllForwarder();

  // Starts forwarding incoming method calls to |destination_|.
  void Init();
  // Stops forwarding method calls.
  void Shutdown();

 private:
  static DBusHandlerResult HandleMessageThunk(DBusConnection* connection,
                                              DBusMessage* raw_message,
                                              void* user_data);

  DBusHandlerResult HandleMessage(DBusConnection* connection,
                                  DBusMessage* raw_message);

  scoped_refptr<dbus::Bus> from_bus_;
  scoped_refptr<dbus::Bus> to_bus_;
  const std::string destination_;

  DISALLOW_COPY_AND_ASSIGN(CatchAllForwarder);
};

}  // namespace bluetooth

#endif  // BLUETOOTH_DISPATCHER_CATCH_ALL_FORWARDER_H_
