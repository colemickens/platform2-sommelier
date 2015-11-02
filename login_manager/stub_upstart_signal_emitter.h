// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOGIN_MANAGER_STUB_UPSTART_SIGNAL_EMITTER_H_
#define LOGIN_MANAGER_STUB_UPSTART_SIGNAL_EMITTER_H_

#include <string>
#include <vector>

#include <base/memory/scoped_ptr.h>
#include <dbus/message.h>

#include "login_manager/upstart_signal_emitter.h"

namespace login_manager {

// Stub implementation of UpstartSignalEmitter that always reports success.
class StubUpstartSignalEmitter : public UpstartSignalEmitter {
 public:
  // Delegate to aid in testing since the UpstartSignalEmitter ownership is
  // transferred, not shared.
  class Delegate {
   public:
    virtual ~Delegate() {}

    // Called when UpstartSignalEmitter::EmitSignal is called.
    virtual void OnSignalEmitted(
        const std::string& signal_name,
        const std::vector<std::string>& args_keyvals) = 0;
  };

  // Mock implementation of Delegate.
  class MockDelegate : public Delegate {
   public:
    MockDelegate() {}
    ~MockDelegate() override {}

    MOCK_METHOD2(OnSignalEmitted, void(const std::string&,
                                       const std::vector<std::string>&));
  };

  explicit StubUpstartSignalEmitter(Delegate* delegate)
      : UpstartSignalEmitter(NULL),
        delegate_(delegate) {}
  virtual ~StubUpstartSignalEmitter() {}
  virtual scoped_ptr<dbus::Response> EmitSignal(
      const std::string& signal_name,
      const std::vector<std::string>& args_keyvals) {
    delegate_->OnSignalEmitted(signal_name, args_keyvals);
    return scoped_ptr<dbus::Response>(dbus::Response::CreateEmpty());
  }

 private:
  Delegate* delegate_;  // Owned by the caller.
};

}  // namespace login_manager

#endif  // LOGIN_MANAGER_STUB_UPSTART_SIGNAL_EMITTER_H_
