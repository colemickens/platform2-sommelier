// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fstream>

#include <google/protobuf/io/zero_copy_stream_impl_lite.h>

#include "perf_protobuf_io.h"
#include "utils.h"

bool WriteProtobufToFile(const PerfDataProto & perf_data_proto,
                         const std::string & filename)
{
  std::string target;
  google::protobuf::io::StringOutputStream stream(&target);
  google::protobuf::io::CodedOutputStream cstream(&stream);
  perf_data_proto.SerializeToCodedStream(&cstream);

  std::vector<char> buffer(target.begin(), target.end());

  return BufferToFile(filename, buffer);
}

bool ReadProtobufFromFile(PerfDataProto * perf_data_proto,
                          const std::string & filename)
{
  std::ifstream in(filename.c_str(), std::ios::binary);
  std::vector<char> buffer;
  if(!FileToBuffer(filename, &buffer))
    return false;

  google::protobuf::io::ArrayInputStream stream(buffer.data(), buffer.size());
  google::protobuf::io::CodedInputStream cstream(&stream);
  perf_data_proto->MergeFromCodedStream(&cstream);

  LOG(INFO) << "#events" << perf_data_proto->events_size();

  return true;
}
