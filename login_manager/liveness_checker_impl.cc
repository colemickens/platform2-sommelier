// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/liveness_checker_impl.h"

#include <signal.h>

#include <base/basictypes.h>
#include <base/bind.h>
#include <base/callback.h>
#include <base/cancelable_callback.h>
#include <base/compiler_specific.h>
#include <base/location.h>
#include <base/memory/weak_ptr.h>
#include <base/message_loop/message_loop_proxy.h>
#include <base/time/time.h>
#include <chromeos/dbus/service_constants.h>
#include <dbus/message.h>
#include <dbus/object_proxy.h>

#include "login_manager/process_manager_service_interface.h"

namespace login_manager {

LivenessCheckerImpl::LivenessCheckerImpl(
    ProcessManagerServiceInterface* manager,
    dbus::ObjectProxy* chrome_dbus_proxy,
    const scoped_refptr<base::MessageLoopProxy>& loop,
    bool enable_aborting,
    base::TimeDelta interval)
    : manager_(manager),
      chrome_dbus_proxy_(chrome_dbus_proxy),
      loop_proxy_(loop),
      enable_aborting_(enable_aborting),
      interval_(interval),
      last_ping_acked_(true),
      weak_ptr_factory_(this) {
}

LivenessCheckerImpl::~LivenessCheckerImpl() {
  Stop();
}

void LivenessCheckerImpl::Start() {
  Stop();  // To be certain.
  last_ping_acked_ = true;
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
}

bool LivenessCheckerImpl::IsRunning() {
  return !liveness_check_.IsCancelled();
}

void LivenessCheckerImpl::CheckAndSendLivenessPing(base::TimeDelta interval) {
  // If there's an un-acked ping, the browser needs to be taken down.
  if (!last_ping_acked_) {
    LOG(WARNING) << "Browser hang detected!";
    if (enable_aborting_) {
      // Note: If this log message is changed, the desktopui_HangDetector
      // autotest must be updated.
      LOG(WARNING) << "Aborting browser process.";
      manager_->AbortBrowser(SIGFPE,
                             "Browser did not respond to DBus liveness check.");
      // HandleChildExit() will reap the process and restart if needed.
      Stop();
      return;
    }
  }

  DLOG(INFO) << "Sending a liveness ping to the browser.";
  last_ping_acked_ = false;
  dbus::MethodCall ping(chromeos::kLibCrosServiceInterface,
                        chromeos::kCheckLiveness);
  chrome_dbus_proxy_->CallMethod(&ping,
                                 dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                                 base::Bind(&LivenessCheckerImpl::HandleAck,
                                            weak_ptr_factory_.GetWeakPtr()));

  DLOG(INFO) << "Scheduling liveness check in " << interval.InSeconds() << "s.";
  liveness_check_.Reset(
      base::Bind(&LivenessCheckerImpl::CheckAndSendLivenessPing,
                 weak_ptr_factory_.GetWeakPtr(), interval));
  loop_proxy_->PostDelayedTask(
      FROM_HERE,
      liveness_check_.callback(),
      interval);
}

void LivenessCheckerImpl::HandleAck(dbus::Response* response) {
  last_ping_acked_ = (response != NULL);
}

}  // namespace login_manager
