// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_TOOL_H_
#define STORAGE_TOOL_H_

#include <string>

#include <base/basictypes.h>
#include <dbus-c++/dbus.h>

#include "subprocess_tool.h"

namespace debugd {

class StorageTool : public SubprocessTool {
 public:
  StorageTool();
  virtual ~StorageTool();

  std::string Smartctl(const std::string& option, DBus::Error* error);
  std::string Start(const DBus::FileDescriptor& outfd, DBus::Error* error);

 private:
  DISALLOW_COPY_AND_ASSIGN(StorageTool);
};

}  // namespace debugd

#endif  // STORAGE_TOOL_H_
