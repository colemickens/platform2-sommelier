// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crash_sender_tool.h"

#include "process_with_id.h"

namespace debugd {

CrashSenderTool::CrashSenderTool() { }

CrashSenderTool::~CrashSenderTool() { }

void CrashSenderTool::UploadCrashes(DBus::Error& error) {  // NOLINT
  ProcessWithId* p = CreateProcess(false);
  p->AddArg("/sbin/crash_sender");
  p->AddStringOption("-e", "SECONDS_SEND_SPREAD=1");
  p->Run();
}

};  // namespace debugd
