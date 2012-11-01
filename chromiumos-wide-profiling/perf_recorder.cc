// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <stddef.h>

#include "perf_recorder.h"
#include "perf_serializer.h"

std::string PerfRecorder::GetSleepCommand(const int time) {
  std::stringstream ss;
  ss << "sleep " << time;
  return ss.str();
}

bool PerfRecorder::RecordAndConvertToProtobuf(
    std::string perf_command,
    const int time,
    PerfDataProto * perf_data) {
  FILE * fp;
  int ret;

  char temp_file[L_tmpnam];
  if (tmpnam(temp_file) == NULL)
    return false;
  // Add -o - to the command line.
  perf_command += std::string(" -o ") +
      temp_file +
      " -- " + GetSleepCommand(time);


  fp = popen(perf_command.c_str(), "r");
  ret = pclose(fp);
  if (ret != 0)
    return false;

  // Now parse the output of perf into a protobuf.
  PerfReader perf_reader;
  perf_reader.ReadFile(temp_file);

  ret = remove(temp_file);
  if (ret != 0)
    return false;

  // Now convert it into a protobuf.
  PerfSerializer perf_serializer;
  return perf_serializer.SerializeReader(&perf_reader, perf_data);
}
