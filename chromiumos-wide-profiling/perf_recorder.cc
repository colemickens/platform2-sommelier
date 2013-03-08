// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <stddef.h>

#include <sstream>

#include "perf_recorder.h"
#include "perf_serializer.h"
#include "quipper_string.h"
#include "utils.h"

string PerfRecorder::GetSleepCommand(const int time) {
  stringstream ss;
  ss << "sleep " << time;
  return ss.str();
}

bool PerfRecorder::RecordAndConvertToProtobuf(
    const string& perf_command,
    const int time,
    quipper::PerfDataProto* perf_data) {
  string temp_file;
  if (!CreateNamedTempFile(&temp_file))
    return false;
  // TODO(asharif): Use a pipe instead of a temporary file here.
  string full_perf_command = perf_command + string(" -o ") +
      temp_file + " -- " + GetSleepCommand(time);

  FILE* fp = popen(full_perf_command.c_str(), "r");
  if (pclose(fp))
    return false;

  // Now parse the output of perf into a protobuf.
  PerfReader perf_reader;
  perf_reader.ReadFile(temp_file);

  if (remove(temp_file.c_str()))
      return false;

  // Now convert it into a protobuf.
  PerfSerializer perf_serializer;
  return perf_serializer.SerializeReader(perf_reader, perf_data);
}
