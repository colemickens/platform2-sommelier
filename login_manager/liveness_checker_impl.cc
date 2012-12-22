// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/liveness_checker_impl.h"

#include <base/basictypes.h>
#include <base/bind.h>
#include <base/callback.h>
#include <base/cancelable_callback.h>
#include <base/compiler_specific.h>
#include <base/location.h>
#include <base/memory/ref_counted.h>
#include <base/memory/weak_ptr.h>
#include <base/message_loop_proxy.h>
#include <base/time.h>
#include <chromeos/dbus/service_constants.h>
#include <dbus/dbus.h>

#include "login_manager/scoped_dbus_pending_call.h"
#include "login_manager/process_manager_service_interface.h"
#include "login_manager/system_utils.h"

namespace login_manager {

LivenessCheckerImpl::LivenessCheckerImpl(
    ProcessManagerServiceInterface* manager,
    SystemUtils* utils,
    const scoped_refptr<base::MessageLoopProxy>& loop,
    bool enable_aborting,
    base::TimeDelta interval)
    : manager_(manager),
      system_(utils),
      loop_proxy_(loop),
      enable_aborting_(enable_aborting),
      interval_(interval),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {
}

LivenessCheckerImpl::~LivenessCheckerImpl() {
  Stop();
}

void LivenessCheckerImpl::Start() {
  Stop();  // To be certain.
  outstanding_liveness_ping_.reset();
  liveness_check_.Reset(
      base::Bind(&LivenessCheckerImpl::CheckAndSendLivenessPing,
                 weak_ptr_factory_.GetWeakPtr(),
                 interval_));
  loop_proxy_->PostDelayedTask(
      FROM_HERE,
      liveness_check_.callback(),
      interval_);
}

void LivenessCheckerImpl::Stop() {
  weak_ptr_factory_.InvalidateWeakPtrs();
  liveness_check_.Cancel();
  if (outstanding_liveness_ping_.get()) {
    system_->CancelAsyncMethodCall(outstanding_liveness_ping_->Get());
    outstanding_liveness_ping_.reset();
  }
}

bool LivenessCheckerImpl::IsRunning() {
  return !liveness_check_.IsCancelled();
}

void LivenessCheckerImpl::CheckAndSendLivenessPing(base::TimeDelta interval) {
  // If there's an un-acked ping, the browser needs to be taken down.
  if (outstanding_liveness_ping_.get() &&
      !system_->CheckAsyncMethodSuccess(outstanding_liveness_ping_->Get())) {
    LOG(WARNING) << "Browser hang detected!";
    if (enable_aborting_) {
      LOG(WARNING) << "Aborting browser process.";
      manager_->AbortBrowser();
      // HandleChildExit() will reap the process and restart if needed.
      Stop();
      return;
    }
  }

  DLOG(INFO) << "Sending a liveness ping to the browser.";
  outstanding_liveness_ping_ =
      system_->CallAsyncMethodOnChromium(chromeos::kCheckLiveness);
  DLOG(INFO) << "Scheduling liveness check in " << interval.InSeconds() << "s.";
  liveness_check_.Reset(
      base::Bind(&LivenessCheckerImpl::CheckAndSendLivenessPing,
                 weak_ptr_factory_.GetWeakPtr(), interval));
  loop_proxy_->PostDelayedTask(
      FROM_HERE,
      liveness_check_.callback(),
      interval);
}

}  // namespace login_manager
