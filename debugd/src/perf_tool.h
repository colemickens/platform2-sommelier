// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PERF_TOOL_H
#define PERF_TOOL_H

#include <string>

#include <base/basictypes.h>
#include <dbus-c++/dbus.h>

namespace debugd {

class PerfTool {
 public:
  PerfTool();
  ~PerfTool();

  std::vector<uint8> GetPerfData(const uint32_t& duration,
                                         DBus::Error& error); // NOLINT
 private:
  void GetPerfDataHelper(const uint32_t& duration,
                         DBus::Error& error,
                         std::string* data_string); // NOLINT
  DISALLOW_COPY_AND_ASSIGN(PerfTool);
};

};  // namespace debugd

#endif  // !PERF_TOOL_H

