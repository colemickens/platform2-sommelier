// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VM_TOOLS_NOTIFICATIOND_NOTIFICATION_SHELL_INTERFACE_H_
#define VM_TOOLS_NOTIFICATIOND_NOTIFICATION_SHELL_INTERFACE_H_

#include <string>

namespace vm_tools {
namespace notificationd {

// Interface for handling notification shell events.
class NotificationShellInterface {
 public:
  NotificationShellInterface() = default;
  virtual ~NotificationShellInterface() = default;

  // Callback for closed event in notification shell.
  virtual void OnClosed(const std::string& notification_key, bool by_user) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(NotificationShellInterface);
};

}  // namespace notificationd
}  // namespace vm_tools

#endif  // VM_TOOLS_NOTIFICATIOND_NOTIFICATION_SHELL_INTERFACE_H_
