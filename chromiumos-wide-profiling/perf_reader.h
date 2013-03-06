// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PERF_READER_H_
#define PERF_READER_H_

#include <vector>
#include <string>

#include "base/basictypes.h"

#include "kernel/perf_internals.h"

struct PerfFileAttr {
  struct perf_event_attr attr;
  std::vector<u64> ids;
};

class PerfReader {
 public:
  PerfReader() : sample_type_(0) {}
  bool ReadFile(const std::string& filename);
  bool ReadFileFromHandle(std::ifstream* in);
  bool WriteFile(const std::string& filename);
  bool WriteFileFromHandle(std::ofstream* out);
  bool RegenerateHeader();

  const std::vector<PerfFileAttr>& attrs() const {
    return attrs_;
  }

  std::vector<PerfFileAttr>* mutable_attrs() {
    return &attrs_;
  }

  const std::vector<event_t>& events() const {
    return events_;
  }

  std::vector<event_t>* mutable_events() {
    return &events_;
  }

  const std::vector<perf_trace_event_type>& event_types() const {
    return event_types_;
  }

  std::vector<perf_trace_event_type>* mutable_event_types() {
    return &event_types_;
  }

  uint64 sample_type() const {
    return sample_type_;
  }

 private:
  bool ReadHeader(std::ifstream* in);
  bool ReadAttrs(std::ifstream* in);
  bool ReadEventTypes(std::ifstream* in);
  bool ReadData(std::ifstream* in);
  bool WriteHeader(std::ofstream* out) const;
  bool WriteAttrs(std::ofstream* out) const;
  bool WriteEventTypes(std::ofstream* out) const;
  bool WriteData(std::ofstream* out) const;
  bool WriteEventFull(std::ofstream* out, const event_t& event) const;

  bool ProcessEvent(const event_t& event);

  std::vector<PerfFileAttr> attrs_;
  std::vector<event_t> events_;
  std::vector<perf_trace_event_type> event_types_;
  uint64 sample_type_;

  struct perf_file_header header_;
  struct perf_file_header out_header_;

  DISALLOW_COPY_AND_ASSIGN(PerfReader);
};

#endif  // PERF_READER_H_
