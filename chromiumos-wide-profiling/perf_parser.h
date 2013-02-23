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
};

class PerfParser : public PerfReader {
 public:
  PerfParser() : do_remap_(false) {}
  PerfParser(bool do_remap) : do_remap_(do_remap) {}
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

 protected:
  // Sort |parsed_events_| by time, storing the results in
  // |parsed_events_sorted_by_time_|.
  void SortParsedEvents();

  // Used for processing events.  e.g. remapping with synthetic addresses.
  bool ProcessEvents();
  bool MapSampleEvent(struct ip_event*);
  bool MapMmapEvent(struct mmap_event*);
  bool MapForkEvent(struct fork_event*);
  bool MapExitEvent(struct fork_event*);

  void ResetAddressMappers();

  std::vector<ParsedEvent> parsed_events_;
  std::vector<ParsedEvent*> parsed_events_sorted_by_time_;

  // For synthetic address mapping.
  bool do_remap_;
  AddressMapper kernel_mapper_;
  std::map<uint32, AddressMapper*> process_mappers_;
  std::map<uint32, uint32> child_to_parent_pid_map_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PerfParser);
};

#endif  // PERF_PARSER_H_
