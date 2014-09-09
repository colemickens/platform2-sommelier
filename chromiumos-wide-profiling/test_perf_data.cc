// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromiumos-wide-profiling/test_perf_data.h"

#include <ostream>  // NOLINT

#include "base/logging.h"

#include "chromiumos-wide-profiling/kernel/perf_internals.h"

namespace quipper {
namespace testing {

ExamplePerfDataFileHeader::ExamplePerfDataFileHeader(
    const size_t attr_count, const unsigned long features) {  // NOLINT
  CHECK_EQ(96, sizeof(perf_file_attr)) << "perf_file_attr has changed size!";
  const size_t attrs_size = attr_count * sizeof(perf_file_attr);
  header_ = {
    .magic = kPerfMagic,
    .size = 104,
    .attr_size = sizeof(perf_file_attr),
    .attrs = {.offset = 104, .size = attrs_size},
    .data = {.offset = 104 + attrs_size, .size = (1+14)*sizeof(u64)},
    .event_types = {0},
    .adds_features = {features, 0, 0, 0},
  };
}

void ExamplePerfDataFileHeader::WriteTo(std::ostream* out) const {
  out->write(reinterpret_cast<const char*>(&header_), sizeof(header_));
  CHECK_EQ(out->tellp(), header_.size);
  CHECK_EQ(out->tellp(), header_.attrs.offset);
}

void ExamplePerfFileAttr_Tracepoint::WriteTo(std::ostream* out) const {
  // Due to the unnamed union fields (eg, sample_period), this structure can't
  // be initialized with designated initializers.
  perf_event_attr attr = {};
  // See kernel src: tools/perf/util/evsel.c perf_evsel__newtp()
  attr.type = PERF_TYPE_TRACEPOINT,
  attr.size = sizeof(perf_event_attr),
  attr.config = tracepoint_event_id_,
  attr.sample_period = 1,
  attr.sample_type = (PERF_SAMPLE_IP |
                      PERF_SAMPLE_TID |
                      PERF_SAMPLE_TIME |
                      PERF_SAMPLE_CPU |
                      PERF_SAMPLE_PERIOD |
                      PERF_SAMPLE_RAW);

  const perf_file_attr file_attr = {
    .attr = attr,
    .ids = {.offset = 104, .size = 0},
  };
  out->write(reinterpret_cast<const char*>(&file_attr), sizeof(file_attr));
}

void ExamplePerfSampleEvent_Tracepoint::WriteTo(std::ostream* out) const {
  const sample_event event = {
    .header = {
      .type = PERF_RECORD_SAMPLE,
      .misc = 0x0002,
      .size = 0x0078,
    }
  };
  const u64 sample_event_array[] = {
    0x00007f999c38d15a,  // IP
    0x0000068d0000068d,  // TID (u32 pid, tid)
    0x0001e0211cbab7b9,  // TIME
    0x0000000000000000,  // CPU
    0x0000000000000001,  // PERIOD
    0x0000004900000044,  // RAW (u32 size = 0x44 = 68 = 4 + 8*sizeof(u64))
    0x000000090000068d,  //  .
    0x0000000000000000,  //  .
    0x0000100000000000,  //  .
    0x0000000300000000,  //  .
    0x0000002200000000,  //  .
    0xffffffff00000000,  //  .
    0x0000000000000000,  //  .
    0x0000000000000000,  //  .
  };
  CHECK_EQ(event.header.size,
           sizeof(event.header) + sizeof(sample_event_array));
  out->write(reinterpret_cast<const char*>(&event), sizeof(event));
  out->write(reinterpret_cast<const char*>(sample_event_array),
              sizeof(sample_event_array));
}

static const char kTraceMetadataValue[] =
    "\x17\x08\x44tracing0.5BLAHBLAHBLAH....";

const std::vector<char> ExampleTracingMetadata::Data::kTraceMetadata(
    kTraceMetadataValue, kTraceMetadataValue+sizeof(kTraceMetadataValue)-1);

void ExampleTracingMetadata::Data::WriteTo(std::ostream* out) const {
  const perf_file_section &index_entry = parent_->index_entry_.index_entry_;
  CHECK_EQ(out->tellp(), index_entry.offset);
  out->write(kTraceMetadata.data(), kTraceMetadata.size());
  CHECK_EQ(out->tellp(), index_entry.offset + index_entry.size);
}

}  // namespace testing
}  // namespace quipper
