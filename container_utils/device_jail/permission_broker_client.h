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
  PermissionBrokerClient(dbus::ObjectProxy* broker_proxy,
                         base::MessageLoop* message_loop)
      : broker_proxy_(broker_proxy),
        message_loop_(message_loop) {}
  virtual ~PermissionBrokerClient() {}

  // PermissionBrokerClientInterface overrides.
  int Open(const std::string& path) override;

 private:
  dbus::ObjectProxy* broker_proxy_;  // weak
  base::MessageLoop* message_loop_;  // weak
};

}  // namespace device_jail

#endif  // CONTAINER_UTILS_DEVICE_JAIL_PERMISSION_BROKER_CLIENT_H_
