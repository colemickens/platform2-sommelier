// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTAINER_UTILS_DEVICE_JAIL_PERMISSION_BROKER_CLIENT_H_
#define CONTAINER_UTILS_DEVICE_JAIL_PERMISSION_BROKER_CLIENT_H_

#include <string>

#include <base/threading/thread.h>
#include <brillo/message_loops/base_message_loop.h>
#include <dbus/bus.h>

namespace dbus {
class ObjectProxy;
};  // namespace dbus

namespace device_jail {

class PermissionBrokerClientInterface {
 public:
  virtual ~PermissionBrokerClientInterface() {}

  // Opens the given file path with permission broker and returns
  // a file descriptor if successful, or -errno on failure.
  virtual int Open(const std::string& path) = 0;
};

class PermissionBrokerClient : public PermissionBrokerClientInterface {
 public:
  PermissionBrokerClient()
      : bus_(nullptr),
        broker_proxy_(nullptr),
        dbus_thread_("permission_broker_client") {}
  virtual ~PermissionBrokerClient() {}

  // Runs the client in its own thread.
  void Start(const base::Closure& after_init);

  // PermissionBrokerClientInterface overrides.
  int Open(const std::string& path) override;

 private:
  // Connects to the system bus and gets an object proxy for the
  // permission broker.
  void Init();

  scoped_refptr<dbus::Bus> bus_;
  dbus::ObjectProxy* broker_proxy_;  // owned by |bus_|

  base::Thread dbus_thread_;
};

}  // namespace device_jail

#endif  // CONTAINER_UTILS_DEVICE_JAIL_PERMISSION_BROKER_CLIENT_H_
