// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef _VPN_MANAGER_DAEMON_MOCK_H_
#define _VPN_MANAGER_DAEMON_MOCK_H_

#include <gmock/gmock.h>

#include "vpn-manager/daemon.h"

namespace vpn_manager {

class DaemonMock : public Daemon {
 public:
  DaemonMock() : Daemon("") {}
  virtual ~DaemonMock() {}

  MOCK_METHOD0(ClearProcess, void());
  MOCK_METHOD0(CreateProcess, chromeos::Process *());
  MOCK_METHOD0(FindProcess, bool());
  MOCK_METHOD0(IsRunning, bool());
  MOCK_METHOD0(Terminate, bool());
  MOCK_CONST_METHOD0(GetPid, pid_t());
};

}  // namespace vpn_manager

#endif  // _VPN_MANAGER_DAEMON_MOCK_H_
