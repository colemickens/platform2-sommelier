// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEBUGD_SRC_PERF_TOOL_H_
#define DEBUGD_SRC_PERF_TOOL_H_

#include <stdint.h>
#include <sys/utsname.h>

#include <string>
#include <vector>

#include <base/files/scoped_file.h>
#include <base/macros.h>
#include <brillo/errors/error.h>

namespace debugd {

class PerfTool {
 public:
  PerfTool();
  ~PerfTool() = default;

  // Runs the perf tool with the request command for |duration_secs| seconds
  // and returns either a perf_data or perf_stat protobuf in serialized form.
  bool GetPerfOutput(uint32_t duration_secs,
                     const std::vector<std::string>& perf_args,
                     std::vector<uint8_t>* perf_data,
                     std::vector<uint8_t>* perf_stat,
                     int32_t* status,
                     brillo::ErrorPtr* error);

  // Runs the perf tool with the request command for |duration_secs| seconds
  // and returns either a perf_data or perf_stat protobuf in serialized form
  // over the passed stdout_fd file descriptor, or nothing if there was an
  // error.
  bool GetPerfOutputFd(uint32_t duration_secs,
                       const std::vector<std::string>& perf_args,
                       const base::ScopedFD& stdout_fd,
                       brillo::ErrorPtr* error);

 private:
  DISALLOW_COPY_AND_ASSIGN(PerfTool);
};

}  // namespace debugd

#endif  // DEBUGD_SRC_PERF_TOOL_H_
