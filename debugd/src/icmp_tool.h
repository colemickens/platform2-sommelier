// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEBUGD_SRC_ICMP_TOOL_H_
#define DEBUGD_SRC_ICMP_TOOL_H_

#include <map>
#include <string>

#include <base/macros.h>

namespace debugd {

class ICMPTool {
 public:
  ICMPTool() = default;
  ~ICMPTool() = default;

  std::string TestICMP(const std::string& host);
  std::string TestICMPWithOptions(
      const std::string& host,
      const std::map<std::string, std::string>& options);

 private:
  DISALLOW_COPY_AND_ASSIGN(ICMPTool);
};

}  // namespace debugd

#endif  // DEBUGD_SRC_ICMP_TOOL_H_
