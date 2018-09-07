// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VM_TOOLS_NOTIFICATIOND_NOTIFICATION_DAEMON_H_
#define VM_TOOLS_NOTIFICATIOND_NOTIFICATION_DAEMON_H_

#include <memory>
#include <string>
#include <vector>

#include <base/macros.h>
#include <brillo/message_loops/base_message_loop.h>

#include "vm_tools/notificationd/dbus_interface.h"
#include "vm_tools/notificationd/dbus_service.h"
#include "vm_tools/notificationd/notification_shell_client.h"

namespace vm_tools {
namespace notificationd {

// Handles D-BUS server for notification and Wayland client for notification.
// Once the D-BUS server recieves notification event, the daemon forwards it via
// the Wayland client.
class NotificationDaemon : public DBusInterface {
 public:
  // Creates and returns a NotificationDaemon. Returns nullptr if the the daemon
  // failed to be initialized for any reason.
  static std::unique_ptr<NotificationDaemon> Create(
      const std::string& display_name,
      const std::string& virtwl_device,
      base::Closure quit_closure);

  ~NotificationDaemon() override = default;

  // DBusInterface overrides.
  bool GetCapabilities(std::vector<std::string>* out_capabilities) override;
  bool Notify(const NotifyArgument& input, uint32_t* out_id) override;
  bool GetServerInformation(ServerInformation* output) override;

 private:
  NotificationDaemon() = default;

  // Initializes the notification daemon. Returns true on success.
  bool Init(const std::string& display_name,
            const std::string& virtwl_device,
            base::Closure quit_closure);

  std::unique_ptr<NotificationShellClient> notification_shell_client_;
  std::unique_ptr<DBusService> dbus_service_;

  // Incremental notification id handled by this daemon.
  uint32_t id_count_ = 0;

  FRIEND_TEST(DBusServiceTest, GetCapabilities);
  FRIEND_TEST(DBusServiceTest, Notify);
  FRIEND_TEST(DBusServiceTest, GetServerInformation);
  DISALLOW_COPY_AND_ASSIGN(NotificationDaemon);
};

}  // namespace notificationd
}  // namespace vm_tools

#endif  // VM_TOOLS_NOTIFICATIOND_NOTIFICATION_DAEMON_H_
