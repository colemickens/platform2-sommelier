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

  PerfEventAndSampleInfo() {
    memset(&sample_info, 0, sizeof(sample_info));
    memset(&event, 0, sizeof(event));
  }
};

// Based on code in tools/perf/util/header.c, the metadata are of the following
// formats:

// Based on kernel/perf_internals.h
const size_t kBuildIDArraySize = 20;
const size_t kBuildIDStringLength = kBuildIDArraySize * 2;

struct CStringWithLength {
  u32 len;
  string str;
};

struct PerfStringMetadata {
  u32 type;
  std::vector<CStringWithLength> data;
};

struct PerfUint32Metadata {
  u32 type;
  std::vector<uint32> data;
};

struct PerfUint64Metadata {
  u32 type;
  std::vector<uint64> data;
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
  bool ReadAttr(const std::vector<char>& data, size_t* offset);
  bool ReadEventAttr(const std::vector<char>& data, size_t* offset,
                     perf_event_attr* attr);
  bool ReadUniqueIDs(const std::vector<char>& data, size_t num_ids,
                     size_t* offset, std::vector<u64>* ids);

  bool ReadEventTypes(const std::vector<char>& data);
  bool ReadEventType(const std::vector<char>& data, size_t* offset);

  bool ReadData(const std::vector<char>& data);

  // Reads metadata in normal mode.
  bool ReadMetadata(const std::vector<char>& data);
  bool ReadBuildIDMetadata(const std::vector<char>& data, u32 type,
                           size_t offset, size_t size);
  bool ReadStringMetadata(const std::vector<char>& data, u32 type,
                          size_t offset, size_t size);
  bool ReadUint32Metadata(const std::vector<char>& data, u32 type,
                          size_t offset, size_t size);
  bool ReadUint64Metadata(const std::vector<char>& data, u32 type,
                          size_t offset, size_t size);
  bool ReadEventDescMetadata(const std::vector<char>& data, u32 type,
                             size_t offset, size_t size);

  // Read perf data from piped perf output data.
  bool ReadPipedData(const std::vector<char>& data);

  bool WriteHeader(std::vector<char>* data) const;
  bool WriteAttrs(std::vector<char>* data) const;
  bool WriteEventTypes(std::vector<char>* data) const;
  bool WriteData(std::vector<char>* data) const;
  bool WriteMetadata(std::vector<char>* data) const;

  // For writing the various types of metadata.
  bool WriteBuildIDMetadata(u32 type, size_t* offset,
                            std::vector<char>* data) const;
  bool WriteStringMetadata(u32 type, size_t* offset,
                           std::vector<char>* data) const;
  bool WriteUint32Metadata(u32 type, size_t* offset,
                           std::vector<char>* data) const;
  bool WriteUint64Metadata(u32 type, size_t* offset,
                           std::vector<char>* data) const;
  bool WriteEventDescMetadata(u32 type, size_t* offset,
                              std::vector<char>* data) const;

  // For reading event blocks within piped perf data.
  bool ReadAttrEventBlock(const std::vector<char>& data, size_t offset,
                          size_t size);
  bool ReadPerfEventBlock(const event_t& event);

  // Returns the number of types of metadata stored.
  size_t GetNumMetadata() const;

  // For computing the sizes of the various types of metadata.
  size_t GetBuildIDMetadataSize() const;
  size_t GetStringMetadataSize() const;
  size_t GetUint32MetadataSize() const;
  size_t GetUint64MetadataSize() const;
  size_t GetEventDescMetadataSize() const;

  // Returns true if we should write the number of strings for the string
  // metadata of type |type|.
  bool NeedsNumberOfStringData(u32 type) const;

  std::vector<PerfFileAttr> attrs_;
  std::vector<perf_trace_event_type> event_types_;
  std::vector<PerfEventAndSampleInfo> events_;
  std::vector<build_id_event*> build_id_events_;
  std::vector<PerfStringMetadata> string_metadata_;
  std::vector<PerfUint32Metadata> uint32_metadata_;
  std::vector<PerfUint64Metadata> uint64_metadata_;
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
