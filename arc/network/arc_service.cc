// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arc/network/arc_service.h"

#include <memory>
#include <string>
#include <utility>

#include "arc/network/ipc.pb.h"
#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/logging.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/string_util.h>

namespace arc_networkd {
namespace {

// TODO(garrick): Remove this workaround ASAP.
int GetContainerPID() {
  const base::FilePath path("/run/containers/android-run_oci/container.pid");
  std::string pid_str;
  if (!base::ReadFileToStringWithMaxSize(path, &pid_str, 16 /* max size */)) {
    LOG(ERROR) << "Failed to read pid file";
    return -1;
  }
  int pid;
  if (!base::StringToInt(base::TrimWhitespaceASCII(pid_str, base::TRIM_ALL),
                         &pid)) {
    LOG(ERROR) << "Failed to convert container pid string";
    return -1;
  }
  LOG(INFO) << "Read container pid as " << pid;
  return pid;
}

}  // namespace

ArcService::ArcService(DeviceManager* dev_mgr, bool is_legacy)
    : GuestService(is_legacy ? GuestMessage::ARC_LEGACY : GuestMessage::ARC,
                   dev_mgr) {}

void ArcService::OnStart() {
  int pid = GetContainerPID();
  if (pid <= 0) {
    LOG(ERROR) << "Cannot start service - invalid container PID";
    return;
  }

  GuestService::OnStart();

  GuestMessage msg;
  msg.set_event(GuestMessage::START);
  msg.set_arc_pid(pid);
  msg.set_type(guest_);
  DispatchMessage(msg);
}

void ArcService::OnStop() {
  GuestMessage msg;
  msg.set_event(GuestMessage::STOP);
  msg.set_type(guest_);
  DispatchMessage(msg);

  GuestService::OnStop();
}

void ArcService::OnDeviceAdded(Device* device) {}

void ArcService::OnDeviceRemoved(Device* device) {}

}  // namespace arc_networkd
