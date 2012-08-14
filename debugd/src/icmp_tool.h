// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ICMP_TOOL_H
#define ICMP_TOOL_H

#include <string>

#include <base/basictypes.h>
#include <dbus-c++/dbus.h>

namespace debugd {

class ICMPTool {
 public:
  ICMPTool();
  ~ICMPTool();

  std::string TestICMP(const std::string& host, DBus::Error* error);
 private:
  DISALLOW_COPY_AND_ASSIGN(ICMPTool);
};

};  // namespace debugd

#endif  // ICMP_TOOL_H
