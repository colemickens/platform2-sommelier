// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WIMAX_STATUS_TOOL_H_
#define WIMAX_STATUS_TOOL_H_

#include <string>

#include <base/basictypes.h>
#include <dbus-c++/dbus.h>

namespace debugd {

class WiMaxStatusTool {
 public:
  WiMaxStatusTool();
  ~WiMaxStatusTool();

  std::string GetWiMaxStatus(DBus::Error* error);

 private:
  DISALLOW_COPY_AND_ASSIGN(WiMaxStatusTool);
};

}  // namespace debugd

#endif  // WIMAX_STATUS_TOOL_H_
