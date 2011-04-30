// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOGIN_MANAGER_MOCK_UPSTART_SIGNAL_EMITTER_H_
#define LOGIN_MANAGER_MOCK_UPSTART_SIGNAL_EMITTER_H_

#include "login_manager/upstart_signal_emitter.h"

namespace login_manager {

// Mock implementation of UpstartSignalEmitter that always reports success.
class MockUpstartSignalEmitter : public UpstartSignalEmitter {
 public:
  MockUpstartSignalEmitter() {}
  virtual ~MockUpstartSignalEmitter() {}
  virtual bool EmitSignal(const std::string& signal_name,
                          const std::string& args_str,
                          GError** error) {
    return true;
  }
};

}  // namespace login_manager

#endif  // LOGIN_MANAGER_MOCK_UPSTART_SIGNAL_EMITTER_H_
