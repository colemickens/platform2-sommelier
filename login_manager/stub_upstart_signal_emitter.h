// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOGIN_MANAGER_STUB_UPSTART_SIGNAL_EMITTER_H_
#define LOGIN_MANAGER_STUB_UPSTART_SIGNAL_EMITTER_H_

#include "login_manager/upstart_signal_emitter.h"

namespace login_manager {

// Stub implementation of UpstartSignalEmitter that always reports success.
class StubUpstartSignalEmitter : public UpstartSignalEmitter {
 public:
  StubUpstartSignalEmitter() {}
  virtual ~StubUpstartSignalEmitter() {}
  virtual bool EmitSignal(const std::string& signal_name,
                          const std::vector<std::string>& args_keyvals,
                          GError** error) {
    return true;
  }
};

}  // namespace login_manager

#endif  // LOGIN_MANAGER_STUB_UPSTART_SIGNAL_EMITTER_H_
