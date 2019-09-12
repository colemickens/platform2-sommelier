// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VPN_MANAGER_DAEMON_MOCK_H_
#define VPN_MANAGER_DAEMON_MOCK_H_

#include <gmock/gmock.h>

#include "vpn-manager/daemon.h"

namespace vpn_manager {

class DaemonMock : public Daemon {
 public:
  DaemonMock() : Daemon("") {}
  ~DaemonMock() override {}

  MOCK_METHOD(void, ClearProcess, (), (override));
  MOCK_METHOD(brillo::Process*, CreateProcess, (), (override));
  MOCK_METHOD(brillo::Process*,
              CreateProcessWithResourceLimits,
              (const ResourceLimits&),
              (override));
  MOCK_METHOD(bool, FindProcess, (), (override));
  MOCK_METHOD(bool, IsRunning, (), (override));
  MOCK_METHOD(bool, Terminate, (), (override));
  MOCK_METHOD(pid_t, GetPid, (), (const, override));
};

}  // namespace vpn_manager

#endif  // VPN_MANAGER_DAEMON_MOCK_H_
