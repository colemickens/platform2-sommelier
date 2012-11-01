// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sstream>

#include "perf_protobuf_io.h"
#include "perf_reader.h"
#include "perf_recorder.h"

bool ParseArguments(int argc, char * argv[],
                    std::string * perf_command_line,
                    int * duration)
{
  if (argc < 3) {
    LOG(ERROR) << "Invalid command line.";
    LOG(ERROR) << "Usage: " << argv[0] <<
               " <duration in seconds>" <<
               " <path to perf>" <<
               " <perf arguments>";
    return false;
  }

  std::stringstream ss;
  ss << argv[1];
  ss >> *duration;

  for( int i=2 ; i<argc ; i++ )
  {
    *perf_command_line += " ";
    *perf_command_line += argv[i];
  }
  return true;
}

// Usage is:
// <exe> <duration in seconds> <perf command line>
int main(int argc, char * argv[])
{
  PerfDataProto perf_data_proto;
  PerfRecorder perf_recorder;
  bool ret;
  std::string perf_command_line;
  int perf_duration;

  ret = ParseArguments(argc, argv, &perf_command_line, &perf_duration);
  if (!ret)
    return 1;

  ret = perf_recorder.RecordAndConvertToProtobuf(perf_command_line,
                                                 atoi(argv[1]),
                                                 &perf_data_proto);
  if (ret == false)
    return 1;
  ret = WriteProtobufToFile(perf_data_proto,
                            "/dev/stdout");
  if (ret == false)
    return 1;
  return 0;
}
