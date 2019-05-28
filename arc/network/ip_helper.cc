// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arc/network/ip_helper.h"

#include <utility>

#include <base/bind.h>
#include <base/logging.h>

namespace arc_networkd {

IpHelper::IpHelper(base::ScopedFD control_fd)
    : msg_dispatcher_(std::move(control_fd)) {}

int IpHelper::OnInit() {
  // Prevent the main process from sending us any signals.
  if (setsid() < 0) {
    PLOG(ERROR) << "Failed to created a new session with setsid: exiting";
    return -1;
  }

  msg_dispatcher_.RegisterFailureHandler(
      base::Bind(&IpHelper::OnParentProcessExit, weak_factory_.GetWeakPtr()));

  msg_dispatcher_.RegisterGuestMessageHandler(
      base::Bind(&IpHelper::OnGuestMessage, weak_factory_.GetWeakPtr()));

  msg_dispatcher_.RegisterDeviceMessageHandler(
      base::Bind(&IpHelper::OnDeviceMessage, weak_factory_.GetWeakPtr()));

  arc_helper_ = ArcHelper::New();
  if (!arc_helper_) {
    LOG(ERROR) << "Aborting setup flow";
    return -1;
  }

  return Daemon::OnInit();
}

void IpHelper::OnParentProcessExit() {
  LOG(ERROR) << "Quitting because the parent process died";
  Quit();
}

void IpHelper::OnGuestMessage(const GuestMessage& msg) {
  if (msg.type() != GuestMessage::ARC)
    return;

  if (msg.event() == GuestMessage::START && msg.has_arc_pid()) {
    arc_helper_->Start(msg.arc_pid());
    return;
  }

  if (msg.event() == GuestMessage::STOP && msg.has_arc_pid())
    arc_helper_->Stop(msg.arc_pid());
}

void IpHelper::OnDeviceMessage(const DeviceMessage& msg) {
  arc_helper_->HandleCommand(msg);
}

}  // namespace arc_networkd
