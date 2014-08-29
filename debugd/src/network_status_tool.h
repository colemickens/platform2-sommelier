// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEBUGD_SRC_NETWORK_STATUS_TOOL_H_
#define DEBUGD_SRC_NETWORK_STATUS_TOOL_H_

#include <string>

#include <base/basictypes.h>
#include <dbus-c++/dbus.h>

namespace debugd {

class NetworkStatusTool {
 public:
  NetworkStatusTool() = default;
  ~NetworkStatusTool() = default;

  std::string GetNetworkStatus(DBus::Error* error);

 private:
  DISALLOW_COPY_AND_ASSIGN(NetworkStatusTool);
};

}  // namespace debugd

#endif  // DEBUGD_SRC_NETWORK_STATUS_TOOL_H_
