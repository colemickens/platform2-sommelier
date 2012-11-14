// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOGIN_MANAGER_LIVENESS_CHECKER_H_
#define LOGIN_MANAGER_LIVENESS_CHECKER_H_

#include <base/basictypes.h>
#include <base/cancelable_callback.h>
#include <base/memory/ref_counted.h>
#include <base/message_loop_proxy.h>

namespace login_manager {

// Provides an interface for classes that ping the browser to see if it's
// alive.
class LivenessChecker {
 public:
  LivenessChecker() {};
  virtual ~LivenessChecker() {};

  // Begin sending periodic liveness pings to the browser.
  virtual void Start() = 0;

  // Handle browser ACK of liveness ping.
  virtual void HandleLivenessConfirmed() = 0;

  // Stop sending periodic liveness pings to the browser.
  // Must be idempotent.
  virtual void Stop() = 0;

  // Returns true if this instance has been started and not yet stopped.
  virtual bool IsRunning() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(LivenessChecker);
};
}  // namespace login_manager

#endif  // LOGIN_MANAGER_LIVENESS_CHECKER_H_
