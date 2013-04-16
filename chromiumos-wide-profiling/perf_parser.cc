// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "perf_parser.h"

#include <algorithm>

#include "base/logging.h"

#include "address_mapper.h"
#include "utils.h"

namespace quipper {

namespace {

// For kernel MMAP events, the pid is -1.
const uint32 kKernelPid = kuint32max;

// Returns true if |e1| has an earlier timestamp than |e2|.  The args are const
// pointers instead of references because of the way this function is used when
// calling std::stable_sort.
bool CompareParsedEventTimes(const quipper::ParsedEvent* e1,
                             const quipper::ParsedEvent* e2) {
  return (e1->sample_info.time < e2->sample_info.time);
}

}  // namespace

PerfParser::PerfParser() : do_remap_(false),
                           kernel_mapper_(new AddressMapper) {}

PerfParser::~PerfParser() {
  ResetAddressMappers();
  delete kernel_mapper_;
}

bool PerfParser::ParseRawEvents() {
  ResetAddressMappers();
  parsed_events_.resize(events_.size());
  for (size_t i = 0; i < events_.size(); ++i) {
    const event_t& raw_event = events_[i].event;
    ParsedEvent& parsed_event = parsed_events_[i];
    parsed_event.raw_event = raw_event;
    parsed_event.sample_info = events_[i].sample_info;
  }
  SortParsedEvents();
  ProcessEvents();
  return true;
}

bool PerfParser::GenerateRawEvents() {
  events_.resize(parsed_events_.size());
  for (size_t i = 0; i < events_.size(); ++i) {
    const ParsedEvent& parsed_event = parsed_events_[i];
    events_[i].event = parsed_event.raw_event;
    events_[i].sample_info = parsed_event.sample_info;
  }
  return true;
}

void PerfParser::SortParsedEvents() {
  parsed_events_sorted_by_time_.resize(parsed_events_.size());
  for (unsigned int i = 0; i < parsed_events_.size(); ++i)
    parsed_events_sorted_by_time_[i] = &parsed_events_[i];
  std::stable_sort(parsed_events_sorted_by_time_.begin(),
                   parsed_events_sorted_by_time_.end(),
                   CompareParsedEventTimes);
}

bool PerfParser::ProcessEvents() {
  memset(&stats_, 0, sizeof(stats_));
  for (unsigned int i = 0; i < parsed_events_sorted_by_time_.size(); ++i) {
    ParsedEvent& parsed_event = *parsed_events_sorted_by_time_[i];
    event_t& event = parsed_event.raw_event;
    switch (event.header.type) {
      case PERF_RECORD_SAMPLE:
        DLOG(INFO) << "IP: " << reinterpret_cast<void*>(event.ip.ip);
        ++stats_.num_sample_events;
        if (MapSampleEventAndGetNameAndOffset(
                &event.ip,
                &parsed_event.dso_and_offset.dso_name,
                &parsed_event.dso_and_offset.offset)) {
          ++stats_.num_sample_events_mapped;
        }
        break;
      case PERF_RECORD_MMAP:
        DLOG(INFO) << "MMAP: " << event.mmap.filename;
        ++stats_.num_mmap_events;
        CHECK(MapMmapEvent(&event.mmap)) << "Unable to map MMAP event!";
        break;
      case PERF_RECORD_FORK:
        DLOG(INFO) << "FORK: " << event.fork.ppid << ":" << event.fork.ptid
                   << " -> " << event.fork.pid << ":" << event.fork.tid;
        ++stats_.num_fork_events;
        CHECK(MapForkEvent(event.fork)) << "Unable to map FORK event!";
        break;
      case PERF_RECORD_EXIT:
        // EXIT events have the same structure as FORK events.
        DLOG(INFO) << "EXIT: " << event.fork.ppid << ":" << event.fork.ptid;
        // In an EXIT event, ppid:ptid and pid:tid must be the same.
        CHECK_EQ(event.fork.pid, event.fork.ppid);
        CHECK_EQ(event.fork.tid, event.fork.ptid);
        ++stats_.num_exit_events;
        break;
      case PERF_RECORD_LOST:
      case PERF_RECORD_COMM:
      case PERF_RECORD_THROTTLE:
      case PERF_RECORD_UNTHROTTLE:
      case PERF_RECORD_READ:
      case PERF_RECORD_MAX:
        DLOG(INFO) << "Parsed event type: " << event.header.type
                   << ". Doing nothing.";
        break;
      default:
        LOG(ERROR) << "Unknown event type: " << event.header.type;
        return false;
    }
  }
  // Print stats collected from parsing.
  LOG(INFO) << "Parser processed: ";
  LOG(INFO) << "  " << stats_.num_mmap_events << " MMAP events";
  LOG(INFO) << "  " << stats_.num_fork_events << " FORK events";
  LOG(INFO) << "  " << stats_.num_exit_events << " EXIT events";
  LOG(INFO) << "  " << stats_.num_sample_events << " SAMPLE events";
  LOG(INFO) << "    " << stats_.num_sample_events_mapped
            << " of these were mapped";
  stats_.did_remap = do_remap_;
  return true;
}

bool PerfParser::MapSampleEvent(struct ip_event* event) {
  return MapSampleEventAndGetNameAndOffset(event, NULL, NULL);
}

bool PerfParser::MapSampleEventAndGetNameAndOffset(struct ip_event* event,
                                                   string* name,
                                                   uint64* offset) {
  // Attempt to find the synthetic address of the IP sample in this order:
  // 1. Address space of the kernel.
  // 2. Address space of its own process.
  // 3. Address space of the parent process.
  uint64 mapped_addr;
  bool mapped = false;
  AddressMapper* mapper = NULL;
  CHECK(kernel_mapper_);
  if (kernel_mapper_->GetMappedAddress(event->ip, &mapped_addr)) {
    mapped = true;
    mapper = kernel_mapper_;
  } else {
    uint32 pid = event->pid;
    while (process_mappers_.find(pid) != process_mappers_.end()) {
      mapper = process_mappers_[pid];
      if (mapper->GetMappedAddress(event->ip, &mapped_addr)) {
        mapped = true;
        // For non-kernel addresses, the address is shifted to after where the
        // kernel objects are mapped.  Described more in MapMmapEvent().
        mapped_addr += kernel_mapper_->GetMaxMappedLength();
        break;
      }
      if (child_to_parent_pid_map_.find(pid) == child_to_parent_pid_map_.end())
        return false;
      pid = child_to_parent_pid_map_[pid];
    }
  }
  if (mapped) {
    if (name && offset)
      CHECK(mapper->GetMappedNameAndOffset(event->ip, name, offset));
    if (do_remap_)
      event->ip = mapped_addr;
  }
  return mapped;
}

bool PerfParser::MapMmapEvent(struct mmap_event* event) {
  AddressMapper* mapper = kernel_mapper_;
  CHECK(kernel_mapper_);
  uint32 pid = event->pid;

  // We need to hide only the real kernel addresses.  But the pid of kernel
  // mmaps may change over time, and thus we could mistakenly identify a kernel
  // mmap as a non-kernel mmap.  To plug this security hole, simply map all real
  // addresses (kernel and non-kernel) to synthetic addresses.
  if (pid != kKernelPid) {
    if (process_mappers_.find(pid) == process_mappers_.end())
      process_mappers_[pid] = new AddressMapper;
    mapper = process_mappers_[pid];
  }

  // Lengths need to be aligned to 4-byte blocks.
  uint64 len = AlignSize(event->len, sizeof(uint32));
  uint64 start = event->start;
  uint64 pgoff = event->pgoff;
  if (pgoff < len) {
    // Sanity check to make sure pgoff is valid.
    CHECK_GE((void*)(start + pgoff), (void*)start);
    len -= event->pgoff;
    start += event->pgoff;
    pgoff = 0;
  }
  if (!mapper->MapWithName(start, len, event->filename, true))
    return false;

  uint64 mapped_addr;
  CHECK(mapper->GetMappedAddress(start, &mapped_addr));
  if (do_remap_) {
    event->start = mapped_addr;
    // If this is a non-kernel DSO, shift it to after where the kernel objects
    // are mapped.  This allows for kernel addresses to be identified by address
    // rather than by pid, since kernel addresses are distinct from non-kernel
    // addresses even in quipper space.
    if (pid != kKernelPid)
      event->start += kernel_mapper_->GetMaxMappedLength();
    event->len = len;
    event->pgoff = pgoff;
  }
  return true;
}

bool PerfParser::MapForkEvent(const struct fork_event& event) {
  uint32 pid = event.pid;
  if (pid == event.ppid) {
    DLOG(INFO) << "Forked process should not have the same pid as the parent.";
    return true;
  }
  if (process_mappers_.find(pid) != process_mappers_.end()) {
    DLOG(INFO) << "Found an existing process mapper with the new process's ID.";
    return true;
  }
  if (child_to_parent_pid_map_.find(pid) != child_to_parent_pid_map_.end()) {
    LOG(ERROR) << "Forked process ID has previously been mapped to a parent "
               << "process.";
    return false;
  }

  process_mappers_[pid] = new AddressMapper;
  child_to_parent_pid_map_[pid] = event.ppid;
  return true;
}

void PerfParser::ResetAddressMappers() {
  std::map<uint32, AddressMapper*>::iterator iter;
  for (iter = process_mappers_.begin(); iter != process_mappers_.end(); ++iter)
    delete iter->second;
  process_mappers_.clear();
  child_to_parent_pid_map_.clear();
}

}  // namespace quipper
