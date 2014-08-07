// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEBUGD_SRC_PERF_TOOL_H_
#define DEBUGD_SRC_PERF_TOOL_H_

#include <stdint.h>

#include <string>
#include <vector>

#include <base/basictypes.h>
#include <dbus-c++/dbus.h>

#include "debugd/src/random_selector.h"

namespace debugd {

class PerfTool {
 public:
  PerfTool();
  ~PerfTool();

  // Randomly runs the perf tool in various modes and collects various events
  // for |duration_secs| seconds and returns a protobuf containing the collected
  // data.
  std::vector<uint8_t> GetRichPerfData(const uint32_t& duration_secs,
                                       DBus::Error* error);
 private:
  // Helper function that runs perf for a given |duration_secs| returning the
  // collected data in |data_string|.
  void GetPerfDataHelper(const uint32_t& duration_secs,
                         const std::string& perf_command_line,
                         DBus::Error* error,
                         std::string* data_string);

  RandomSelector random_selector_;

  DISALLOW_COPY_AND_ASSIGN(PerfTool);
};

}  // namespace debugd

#endif  // DEBUGD_SRC_PERF_TOOL_H_
