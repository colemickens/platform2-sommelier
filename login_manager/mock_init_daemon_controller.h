// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOGIN_MANAGER_MOCK_INIT_DAEMON_CONTROLLER_H_
#define LOGIN_MANAGER_MOCK_INIT_DAEMON_CONTROLLER_H_

#include <memory>
#include <string>
#include <vector>

#include <dbus/message.h>
#include <gmock/gmock.h>

#include "login_manager/init_daemon_controller.h"

namespace login_manager {

class MockInitDaemonController : public InitDaemonController {
 public:
  MockInitDaemonController();
  ~MockInitDaemonController() override;

  // Work around. gmock 1.7 does not support returning move-only-type value.
  // TODO(crbug.com/733104): Remove this when gmock is upgraded to 1.8.
  std::unique_ptr<dbus::Response> TriggerImpulse(
      const std::string& name,
      const std::vector<std::string>& arg_keyvals,
      TriggerMode mode) override {
    return std::unique_ptr<dbus::Response>(
        TriggerImpulseInternal(name, arg_keyvals, mode));
  }
  MOCK_METHOD3(TriggerImpulseInternal,
               dbus::Response*(const std::string&,
                               const std::vector<std::string>&,
                               TriggerMode));
};

}  // namespace login_manager

#endif  // LOGIN_MANAGER_MOCK_INIT_DAEMON_CONTROLLER_H_
