// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NETWORK_STATUS_TOOL_H
#define NETWORK_STATUS_TOOL_H

#include <string>

#include <base/basictypes.h>
#include <dbus-c++/dbus.h>

namespace debugd {

class NetworkStatusTool {
 public:
  NetworkStatusTool();
  ~NetworkStatusTool();

  std::string GetNetworkStatus(DBus::Error& error); // NOLINT
 private:
  DISALLOW_COPY_AND_ASSIGN(NetworkStatusTool);
};

};  // namespace debugd

#endif  // NETWORK_STATUS_TOOL_H
