// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEBUGD_SRC_PERF_TOOL_H_
#define DEBUGD_SRC_PERF_TOOL_H_

#include <stdint.h>
#include <sys/utsname.h>

#include <memory>
#include <string>
#include <vector>

#include <base/macros.h>
#include <dbus-c++/dbus.h>

namespace debugd {

class PerfTool {
 public:
  PerfTool();
  ~PerfTool() = default;

  // Runs the perf tool with the request command for |duration_secs| seconds
  // and returns either a perf_data or perf_stat protobuf in serialized form.
  int GetPerfOutput(const uint32_t& duration_secs,
                    const std::vector<std::string>& perf_args,
                    std::vector<uint8_t>* perf_data,
                    std::vector<uint8_t>* perf_stat,
                    DBus::Error* error);

 private:
  // Helper function that runs perf for a given |duration_secs| returning the
  // collected data in |data_string|. Return value is the status from running
  // perf.
  int GetPerfOutputHelper(const uint32_t& duration_secs,
                          const std::vector<std::string>& perf_args,
                          DBus::Error* error,
                          std::string* data_string);

  DISALLOW_COPY_AND_ASSIGN(PerfTool);
};

}  // namespace debugd

#endif  // DEBUGD_SRC_PERF_TOOL_H_
