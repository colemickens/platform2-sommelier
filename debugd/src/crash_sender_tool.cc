// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "debugd/src/crash_sender_tool.h"

#include "debugd/src/process_with_id.h"

namespace debugd {

void CrashSenderTool::UploadCrashes(DBus::Error* error) {
  // 'crash_sender' requires accessing user mounts to upload user crashes.
  ProcessWithId* p =
      CreateProcess(false /* sandboxed */, true /* access_root_mount_ns */);
  p->AddArg("/sbin/crash_sender");
  p->AddStringOption("-e", "SECONDS_SEND_SPREAD=1");
  p->Run();
}

}  // namespace debugd
