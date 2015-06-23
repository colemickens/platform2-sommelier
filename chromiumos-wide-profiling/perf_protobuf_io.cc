// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromiumos-wide-profiling/perf_protobuf_io.h"

#include <vector>

#include "base/logging.h"

#include "chromiumos-wide-profiling/compat/string.h"
#include "chromiumos-wide-profiling/utils.h"

namespace quipper {

bool WriteProtobufToFile(const PerfDataProto& perf_data_proto,
                         const string& filename) {
  string output;
  perf_data_proto.SerializeToString(&output);

  return BufferToFile(filename, output);
}

bool ReadProtobufFromFile(PerfDataProto* perf_data_proto,
                          const string& filename) {
  std::vector<char> buffer;
  if (!FileToBuffer(filename, &buffer))
    return false;

  bool ret = perf_data_proto->ParseFromArray(buffer.data(), buffer.size());

  LOG(INFO) << "#events" << perf_data_proto->events_size();

  return ret;
}

}  // namespace quipper
