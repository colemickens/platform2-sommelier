// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEBUGD_SRC_ICMP_TOOL_H_
#define DEBUGD_SRC_ICMP_TOOL_H_

#include <map>
#include <string>

#include <base/basictypes.h>
#include <dbus-c++/dbus.h>

namespace debugd {

class ICMPTool {
 public:
  ICMPTool();
  ~ICMPTool();

  std::string TestICMP(const std::string& host, DBus::Error* error);
  std::string TestICMPWithOptions(
      const std::string& host,
      const std::map<std::string, std::string>& options,
      DBus::Error* error);

 private:
  DISALLOW_COPY_AND_ASSIGN(ICMPTool);
};

}  // namespace debugd

#endif  // DEBUGD_SRC_ICMP_TOOL_H_
