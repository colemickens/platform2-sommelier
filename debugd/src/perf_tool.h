// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PERF_TOOL_H
#define PERF_TOOL_H

#include <string>

#include <base/basictypes.h>
#include <dbus-c++/dbus.h>

#include "random_selector.h"

namespace debugd {

class PerfTool {
 public:
  PerfTool();
  ~PerfTool();

  // Runs the perf tool for |duration_secs| seconds in systemwide mode and
  // returns a protobuf containing the collected data.
  std::vector<uint8> GetPerfData(const uint32_t& duration_secs,
                                 DBus::Error& error); // NOLINT

  // Randomly runs the perf tool in various modes and collects various events
  // for |duration_secs| seconds and returns a protobuf containing the collected
  // data.
  std::vector<uint8> GetRichPerfData(const uint32_t& duration_secs,
                                     DBus::Error& error); // NOLINT
 private:
  // Helper function that runs perf for a given |duration_secs| returning the
  // collected data in |data_string|.
  void GetPerfDataHelper(const uint32_t& duration_secs,
                         const std::string& perf_command_line,
                         DBus::Error& error,
                         std::string* data_string); // NOLINT

  RandomSelector random_selector_;

  DISALLOW_COPY_AND_ASSIGN(PerfTool);
};

};  // namespace debugd

#endif  // !PERF_TOOL_H

