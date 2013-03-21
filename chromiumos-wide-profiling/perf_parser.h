// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PERF_PARSER_H_
#define PERF_PARSER_H_

#include <map>

#include "base/basictypes.h"

#include "address_mapper.h"
#include "perf_reader.h"

struct ParsedEvent {
  // TODO(sque): to save space, |raw_event| should be a pointer.
  event_t raw_event;                // Contains perf event info.
  struct perf_sample sample_info;   // Contains perf sample info.

  struct DSOAndOffset {
    string dso_name;
    uint64 offset;
  } dso_and_offset;
};

struct PerfEventStats {
  // Number of each type of event.
  uint32 num_sample_events;
  uint32 num_mmap_events;
  uint32 num_fork_events;
  uint32 num_exit_events;

  // Number of sample events that were successfully mapped using the address
  // mapper.  The mapping is recorded regardless of whether the address in the
  // perf sample event itself was assigned the remapped address.  The latter is
  // indicated by |did_remap|.
  uint32 num_sample_events_mapped;

  // Whether address remapping was enabled during event parsing.
  bool did_remap;
};

class PerfParser : public PerfReader {
 public:
  PerfParser() : do_remap_(false) {}
  ~PerfParser();

  // Gets parsed event/sample info from raw event data.
  bool ParseRawEvents();

  // Composes raw event data from event/sample info.
  bool GenerateRawEvents();

  const std::vector<ParsedEvent>& parsed_events() const {
    return parsed_events_;
  }

  // Returns an array of pointers to |parsed_events_| sorted by sample time.
  // The first time this is called, it will create the sorted array.
  const std::vector<ParsedEvent*>& GetEventsSortedByTime() const {
    return parsed_events_sorted_by_time_;
  }

  bool do_remap() const {
    return do_remap_;
  }

  void set_do_remap(bool do_remap) {
    do_remap_ = do_remap;
  }

  const PerfEventStats& stats() const {
    return stats_;
  }

 protected:
  // Sort |parsed_events_| by time, storing the results in
  // |parsed_events_sorted_by_time_|.
  void SortParsedEvents();

  // Used for processing events.  e.g. remapping with synthetic addresses.
  bool ProcessEvents();
  bool MapSampleEvent(struct ip_event*);
  bool MapMmapEvent(struct mmap_event*);
  bool MapForkEvent(const struct fork_event&);

  // Does a sample event remap and then returns DSO name and offset of sample.
  bool MapSampleEventAndGetNameAndOffset(struct ip_event* event,
                                         string* name,
                                         uint64* offset);

  void ResetAddressMappers();

  std::vector<ParsedEvent> parsed_events_;
  std::vector<ParsedEvent*> parsed_events_sorted_by_time_;

  // For synthetic address mapping.
  bool do_remap_;
  AddressMapper kernel_mapper_;
  std::map<uint32, AddressMapper*> process_mappers_;
  std::map<uint32, uint32> child_to_parent_pid_map_;

  PerfEventStats stats_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PerfParser);
};

#endif  // PERF_PARSER_H_
