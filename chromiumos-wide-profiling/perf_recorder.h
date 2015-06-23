// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMIUMOS_WIDE_PROFILING_PERF_RECORDER_H_
#define CHROMIUMOS_WIDE_PROFILING_PERF_RECORDER_H_

#include <string>
#include <vector>

#include "base/macros.h"

#include "chromiumos-wide-profiling/compat/string.h"
#include "chromiumos-wide-profiling/perf_reader.h"

namespace quipper {

class PerfRecorder {
 public:
  PerfRecorder() {}

  // Runs the perf command specified in |perf_args| for |time| seconds. The
  // output is returned as a serialized protobuf in |output_string|. The
  // protobuf format depends on the provided perf command.
  bool RunCommandAndGetSerializedOutput(const std::vector<string>& perf_args,
                                        const int time,
                                        string* output_string);

 private:
  DISALLOW_COPY_AND_ASSIGN(PerfRecorder);
};

}  // namespace quipper

#endif  // CHROMIUMOS_WIDE_PROFILING_PERF_RECORDER_H_
