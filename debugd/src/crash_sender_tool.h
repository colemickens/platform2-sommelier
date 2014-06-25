// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRASH_SENDER_TOOL_H_
#define CRASH_SENDER_TOOL_H_

#include <base/basictypes.h>
#include <dbus-c++/dbus.h>

#include "subprocess_tool.h"

namespace debugd {

class CrashSenderTool : public SubprocessTool {
 public:
  CrashSenderTool();
  virtual ~CrashSenderTool();

  void UploadCrashes(DBus::Error* error);

 private:
  DISALLOW_COPY_AND_ASSIGN(CrashSenderTool);
};

}  // namespace debugd

#endif  // CRASH_SENDER_TOOL_H_
