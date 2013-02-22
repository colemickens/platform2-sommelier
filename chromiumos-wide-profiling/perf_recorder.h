// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PERF_RECORDER_H_
#define PERF_RECORDER_H_

#include <string>

#include <base/basictypes.h>

#include "perf_reader.h"
#include "perf_data.pb.h"

class PerfRecorder {
 public:
  PerfRecorder() {}
  bool RecordAndConvertToProtobuf(const std::string& perf_command,
                                  const int time,
                                  PerfDataProto* perf_data);
 private:
  std::string GetSleepCommand(const int time);
  DISALLOW_COPY_AND_ASSIGN(PerfRecorder);
};

#endif  // PERF_RECORDER_H_
