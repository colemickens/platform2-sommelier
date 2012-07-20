// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NETIF_TOOL_H
#define NETIF_TOOL_H

#include <string>

#include <base/basictypes.h>
#include <dbus-c++/dbus.h>

namespace debugd {

class NetifTool {
 public:
  NetifTool();
  ~NetifTool();

  std::string GetInterfaces(DBus::Error* error);
 private:
  DISALLOW_COPY_AND_ASSIGN(NetifTool);
};

};  // namespace debugd

#endif  // NETIF_TOOL_H
