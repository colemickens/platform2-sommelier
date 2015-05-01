// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMIUMOS_WIDE_PROFILING_SAMPLE_INFO_READER_H_
#define CHROMIUMOS_WIDE_PROFILING_SAMPLE_INFO_READER_H_

#include <stdint.h>

namespace quipper {

// Forward declarations of structures.
union perf_event;
typedef perf_event event_t;
struct perf_sample;

class SampleInfoReader {
 public:
  SampleInfoReader(uint64_t sample_type,
                   uint64_t read_format,
                   bool read_cross_endian)
    : sample_type_(sample_type),
      read_format_(read_format),
      read_cross_endian_(read_cross_endian) {}

  bool ReadPerfSampleInfo(const event_t& event,
                          struct perf_sample* sample) const;
  bool WritePerfSampleInfo(const perf_sample& sample,
                           event_t* event) const;

  // Given a general perf sample format |sample_type|, return the fields of that
  // format that are present in a sample for an event of type |event_type|.
  //
  // e.g. FORK and EXIT events have the fields {time, pid/tid, cpu, id}.
  // Given a sample type with fields {ip, time, pid/tid, and period}, return
  // the intersection of these two field sets: {time, pid/tid}.
  //
  // All field formats are bitfields, as defined by enum
  // perf_event_sample_format in kernel/perf_event.h.
  static uint64_t GetSampleFieldsForEventType(
      uint32_t event_type, uint64_t sample_type);

  // Returns the offset in bytes within a perf event structure at which the raw
  // perf sample data is located.
  static uint64_t GetPerfSampleDataOffset(const event_t& event);

 private:
  // Bitfield indicating which sample info fields are present in the event.
  // See enum perf_event_sample_format in kernel/perf_event.h.
  uint64_t sample_type_;

  // Bitfield indicating read info format. See enum perf_event_read_format in
  // kernel/perf_event.h.
  uint64_t read_format_;

  // Set this flag if values (uint32s and uint64s) should be endian-swapped
  // during reads.
  bool read_cross_endian_;
};

}  // namespace quipper

#endif  // CHROMIUMOS_WIDE_PROFILING_SAMPLE_INFO_READER_H_
