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

struct PerfFileAttr {
  struct perf_event_attr attr;
  std::vector<u64> ids;
};

class PerfReader {
 public:
  PerfReader() : sample_type_(0) {}

  bool ReadFile(const string& filename);
  bool ReadFileData(const std::vector<char>& data);
  bool WriteFile(const string& filename);
  bool RegenerateHeader();

  // Accessor funcs.
  const std::vector<PerfFileAttr>& attrs() const {
    return attrs_;
  }

  const std::vector<event_t>& events() const {
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
  bool WriteHeader(std::vector<char>* data) const;
  bool WriteAttrs(std::vector<char>* data) const;
  bool WriteEventTypes(std::vector<char>* data) const;
  bool WriteData(std::vector<char>* data) const;

  std::vector<PerfFileAttr> attrs_;
  std::vector<event_t> events_;
  std::vector<perf_trace_event_type> event_types_;
  uint64 sample_type_;

 private:
  struct perf_file_header header_;
  struct perf_file_header out_header_;

  DISALLOW_COPY_AND_ASSIGN(PerfReader);
};

#endif  // PERF_READER_H_
