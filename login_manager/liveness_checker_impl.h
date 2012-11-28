// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOGIN_MANAGER_LIVENESS_CHECKER_IMPL_H_
#define LOGIN_MANAGER_LIVENESS_CHECKER_IMPL_H_

#include <base/basictypes.h>
#include <base/cancelable_callback.h>
#include <base/memory/ref_counted.h>
#include <base/memory/weak_ptr.h>
#include <base/time.h>

#include "login_manager/liveness_checker.h"

namespace base {
class MessageLoopProxy;
}  // namespace base

namespace login_manager {
class ScopedDBusPendingCall;
class SessionManagerService;
class SystemUtils;

// An implementation of LivenessChecker that pings the browser over DBus,
// and expects the response to a ping to come in reliably before the next
// ping is sent.  If not, it may ask |manager| to abort the browser process.
//
// Actual aborting behavior is controlled by the enable_aborting flag.
class LivenessCheckerImpl : public LivenessChecker {
 public:
  LivenessCheckerImpl(SessionManagerService* manager,
                      SystemUtils* utils,
                      const scoped_refptr<base::MessageLoopProxy>& loop,
                      bool enable_aborting,
                      base::TimeDelta interval);
  virtual ~LivenessCheckerImpl();

  // Implementation of LivenessChecker.
  void Start();
  void Stop();
  bool IsRunning();

  // If a liveness check is outstanding, kills the browser and clears liveness
  // tracking state.  This instance will be stopped at that point in time.
  // If no ping is outstanding, sends a liveness check to the browser over DBus,
  // then reschedules itself after interval.
  void CheckAndSendLivenessPing(base::TimeDelta interval);

 private:
  SessionManagerService* manager_;
  SystemUtils* system_;
  scoped_refptr<base::MessageLoopProxy> loop_proxy_;

  const bool enable_aborting_;
  const base::TimeDelta interval_;
  scoped_ptr<ScopedDBusPendingCall> outstanding_liveness_ping_;
  base::CancelableClosure liveness_check_;
  base::WeakPtrFactory<LivenessCheckerImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(LivenessCheckerImpl);
};
}  // namespace login_manager

#endif  // LOGIN_MANAGER_LIVENESS_CHECKER_IMPL_H_
