// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PERF_PARSER_H_
#define PERF_PARSER_H_

#include <map>
#include <utility>

#include "base/basictypes.h"

#include "perf_reader.h"

namespace quipper {

class AddressMapper;

struct ParsedEvent {
  // Stores address of the event pointer in |events_|.
  // We store an event_t** instead of an event_t* to avoid having multiple
  // copies of pointers returned by calloc.
  event_t** raw_event;

  // Time, extracted from sample info.  This is the only field that is copied
  // from the sample info, to make sorting the events simpler.
  uint64 time;

  // For mmap events, use this to count the number of samples that are in this
  // region.
  uint32 num_samples_in_mmap_region;

  string command;  // Command associated with this sample.

  struct DSOAndOffset {
    string dso_name;
    uint64 offset;
  } dso_and_offset;

  // DSO+offset info for callchain.
  std::vector<DSOAndOffset> callchain;

  // DSO + offset info for branch stack entries.
  struct BranchEntry {
    bool predicted;
    DSOAndOffset from;
    DSOAndOffset to;
  };
  std::vector<BranchEntry> branch_stack;
};

struct PerfEventStats {
  // Number of each type of event.
  uint32 num_sample_events;
  uint32 num_mmap_events;
  uint32 num_comm_events;
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
  PerfParser();
  ~PerfParser();

  struct Options {
    bool do_remap;
    bool discard_unused_events;

    Options() : do_remap(false),
                discard_unused_events(false) {}
  };

  // Pass in a struct containing various options.
  void SetOptions(const Options& options);

  // Gets parsed event/sample info from raw event data.
  bool ParseRawEvents();

  const std::vector<ParsedEvent>& parsed_events() const {
    return parsed_events_;
  }

  // Returns an array of pointers to |parsed_events_| sorted by sample time.
  // The first time this is called, it will create the sorted array.
  const std::vector<ParsedEvent*>& GetEventsSortedByTime() const {
    return parsed_events_sorted_by_time_;
  }

  const PerfEventStats& stats() const {
    return stats_;
  }

 protected:
  // Defines a type for a pid:tid pair.
  typedef std::pair<uint32, uint32> PidTid;

  // Sort |parsed_events_| by time, storing the results in
  // |parsed_events_sorted_by_time_|.
  void SortParsedEvents();

  // Used for processing events.  e.g. remapping with synthetic addresses.
  bool ProcessEvents();
  bool MapMmapEvent(struct mmap_event*, uint64 id);
  bool MapForkEvent(const struct fork_event&);

  // Does a sample event remap and then returns DSO name and offset of sample.
  bool MapSampleEvent(ParsedEvent* parsed_event);

  bool MapIPAndPidAndGetNameAndOffset(uint64 ip,
                                      uint32 pid,
                                      uint64* new_ip,
                                      string* name,
                                      uint64* offset);

  void ResetAddressMappers();

  std::vector<ParsedEvent> parsed_events_;
  std::vector<ParsedEvent*> parsed_events_sorted_by_time_;

  // For synthetic address mapping.
  bool do_remap_;
  AddressMapper* kernel_mapper_;
  std::map<uint32, AddressMapper*> process_mappers_;
  std::map<uint32, uint32> child_to_parent_pid_map_;

  // Maps pid/tid to commands.
  std::map<PidTid, string> pidtid_to_comm_map_;

  // Set this flag to discard non-sample events that don't have any associated
  // sample events. e.g. MMAP regions with no samples in them.
  bool discard_unused_events_;

  PerfEventStats stats_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PerfParser);
};

}  // namespace quipper

#endif  // PERF_PARSER_H_
