// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PERF_READER_H_
#define PERF_READER_H_

#include <string>
#include <vector>

#include "base/basictypes.h"

#include "quipper_string.h"
#include "kernel/perf_internals.h"

namespace quipper {

struct PerfFileAttr {
  struct perf_event_attr attr;
  std::vector<u64> ids;
};

struct PerfEventAndSampleInfo {
  struct perf_sample sample_info;
  event_t event;
};

struct PerfMetadata {
  u32 type;
  u64 size;

  PerfMetadata() {}
};

// Based on code in tools/perf/util/header.c, the metadata for strings
// (hostname, osrelease, etc.) is of the following format:
struct PerfStringMetadata : public PerfMetadata {
  u32 len;
  string data;

  PerfStringMetadata() : PerfMetadata() {}
};


class PerfReader {
 public:
  PerfReader() : sample_type_(0),
                 is_cross_endian_(0) {}
  ~PerfReader();

  bool ReadFile(const string& filename);
  bool ReadFileData(const std::vector<char>& data);
  bool WriteFile(const string& filename);
  bool RegenerateHeader();

  // Accessor funcs.
  const std::vector<PerfFileAttr>& attrs() const {
    return attrs_;
  }

  const std::vector<PerfEventAndSampleInfo>& events() const {
    return events_;
  }

  const std::vector<perf_trace_event_type>& event_types() const {
    return event_types_;
  }

 protected:
  bool ReadHeader(const std::vector<char>& data);
  bool ReadAttrs(const std::vector<char>& data);
  bool ReadEventTypes(const std::vector<char>& data);
  bool ReadData(const std::vector<char>& data);
  // Reads metadata in normal mode.  Does not support cross endianness.
  bool ReadMetadata(const std::vector<char>& data);

  // For reading the various formats of metadata.
  bool ReadStringMetadata(const std::vector<char>& data, u32 type,
                          size_t offset, size_t size);

  // Read perf data from piped perf output data.
  bool ReadPipedData(const std::vector<char>& data);

  bool WriteHeader(std::vector<char>* data) const;
  bool WriteAttrs(std::vector<char>* data) const;
  bool WriteEventTypes(std::vector<char>* data) const;
  bool WriteData(std::vector<char>* data) const;
  bool WriteMetadata(std::vector<char>* data) const;

  // For writing the various types of metadata.
  bool WriteStringMetadata(u32 type, size_t offset,
                           const PerfMetadata** metadata_handle,
                           std::vector<char>* data) const;

  // For reading event blocks within piped perf data.
  bool ReadAttrEventBlock(const struct attr_event& attr_event);
  bool ReadEventTypeEventBlock(const struct event_type_event& event_type_event);
  bool ReadEventDescEventBlock(const struct event_desc_event& event_desc_event);
  bool ReadPerfEventBlock(const event_t& event);

  std::vector<PerfFileAttr> attrs_;
  std::vector<perf_trace_event_type> event_types_;
  std::vector<PerfEventAndSampleInfo> events_;
  std::vector<PerfStringMetadata> string_metadata_;
  uint64 sample_type_;
  uint64 metadata_mask_;

  // Indicates that the perf data being read is from machine with a different
  // endianness than the current machine.
  bool is_cross_endian_;

 private:
  // The file header is either a normal header or a piped header.
  union {
    struct perf_file_header header_;
    struct perf_pipe_file_header piped_header_;
  };
  struct perf_file_header out_header_;

  DISALLOW_COPY_AND_ASSIGN(PerfReader);
};

}  // namespace quipper

#endif  // PERF_READER_H_
