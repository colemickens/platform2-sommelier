// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "perf_parser.h"

#include <algorithm>
#include <cstdio>
#include <set>

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
  return (e1->sample_info->time < e2->sample_info->time);
}

// Name of the kernel swapper process.
const char kSwapperCommandName[] = "swapper";

}  // namespace

PerfParser::PerfParser() : do_remap_(false),
                           kernel_mapper_(new AddressMapper),
                           discard_unused_events_(false) {}

PerfParser::~PerfParser() {
  ResetAddressMappers();
  delete kernel_mapper_;
}

void PerfParser::SetOptions(const PerfParser::Options& options) {
  do_remap_ = options.do_remap;
  discard_unused_events_ = options.discard_unused_events;
}

bool PerfParser::ParseRawEvents() {
  ResetAddressMappers();
  parsed_events_.resize(events_.size());
  for (size_t i = 0; i < events_.size(); ++i) {
    ParsedEvent& parsed_event = parsed_events_[i];
    parsed_event.raw_event = &events_[i].event;
    parsed_event.sample_info = &events_[i].sample_info;
  }
  SortParsedEvents();
  ProcessEvents();

  if (!discard_unused_events_)
    return true;

  // Some MMAP events' mapped regions will not have any samples.  These MMAP
  // events should be dropped.  |parsed_events_| should be reconstructed without
  // these events.
  size_t write_index = 0;
  size_t read_index;
  for (read_index = 0; read_index < parsed_events_.size(); ++read_index) {
    const ParsedEvent& event = parsed_events_[read_index];
    if (event.raw_event->header.type == PERF_RECORD_MMAP &&
        event.num_samples_in_mmap_region == 0) {
      continue;
    }
    if (read_index != write_index)
      parsed_events_[write_index] = event;
    ++write_index;
  }
  CHECK_LE(write_index, parsed_events_.size());
  parsed_events_.resize(write_index);

  // Now regenerate the sorted event list again.  These are pointers to events
  // so they must be regenerated after a resize() of the ParsedEvent vector.
  SortParsedEvents();

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
    event_t& event = *parsed_event.raw_event;
    switch (event.header.type) {
      case PERF_RECORD_SAMPLE:
        VLOG(1) << "IP: " << std::hex << event.ip.ip;
        ++stats_.num_sample_events;

        if (MapSampleEvent(&parsed_event))
          ++stats_.num_sample_events_mapped;
        break;
      case PERF_RECORD_MMAP:
        VLOG(1) << "MMAP: " << event.mmap.filename;
        ++stats_.num_mmap_events;
        // Use the array index of the current mmap event as a unique identifier.
        CHECK(MapMmapEvent(&event.mmap, i)) << "Unable to map MMAP event!";
        // No samples in this MMAP region yet, hopefully.
        parsed_event.num_samples_in_mmap_region = 0;
        break;
      case PERF_RECORD_FORK:
        VLOG(1) << "FORK: " << event.fork.ppid << ":" << event.fork.ptid
                << " -> " << event.fork.pid << ":" << event.fork.tid;
        ++stats_.num_fork_events;
        CHECK(MapForkEvent(event.fork)) << "Unable to map FORK event!";
        break;
      case PERF_RECORD_EXIT:
        // EXIT events have the same structure as FORK events.
        VLOG(1) << "EXIT: " << event.fork.ppid << ":" << event.fork.ptid;
        ++stats_.num_exit_events;
        break;
      case PERF_RECORD_COMM:
        VLOG(1) << "COMM: " << event.comm.pid << ":" << event.comm.tid << ": "
                << event.comm.comm;
        ++stats_.num_comm_events;
        pidtid_to_comm_map_[std::make_pair(event.comm.pid, event.comm.tid)] =
            event.comm.comm;
        break;
      case PERF_RECORD_LOST:
      case PERF_RECORD_THROTTLE:
      case PERF_RECORD_UNTHROTTLE:
      case PERF_RECORD_READ:
      case PERF_RECORD_MAX:
        VLOG(1) << "Parsed event type: " << event.header.type
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
  LOG(INFO) << "  " << stats_.num_comm_events << " COMM events";
  LOG(INFO) << "  " << stats_.num_fork_events << " FORK events";
  LOG(INFO) << "  " << stats_.num_exit_events << " EXIT events";
  LOG(INFO) << "  " << stats_.num_sample_events << " SAMPLE events";
  LOG(INFO) << "    " << stats_.num_sample_events_mapped
            << " of these were mapped";
  stats_.did_remap = do_remap_;
  return true;
}

bool PerfParser::MapSampleEvent(ParsedEvent* parsed_event) {
  bool mapping_failed = false;

  // Find the associated command.
  uint32 pid = parsed_event->sample_info->pid;
  uint32 tid = parsed_event->sample_info->tid;
  PidTid pidtid = std::make_pair(pid, tid);
  if (pidtid_to_comm_map_.find(pidtid) != pidtid_to_comm_map_.end()) {
    parsed_event->command = pidtid_to_comm_map_[pidtid];
  } else if (pid == 0) {
    parsed_event->command = kSwapperCommandName;
  } else {  // If no command found, use the pid as command.
    char string_buf[sizeof(parsed_event->raw_event->comm.comm)];
    snprintf(string_buf, arraysize(string_buf), "%u", pid);
    parsed_event->command = string_buf;
  }

  struct ip_event& event = parsed_event->raw_event->ip;

  // Map the event IP itself.
  string& name = parsed_event->dso_and_offset.dso_name;
  uint64& offset = parsed_event->dso_and_offset.offset;
  if (!MapIPAndPidAndGetNameAndOffset(event.ip,
                                      event.pid,
                                      reinterpret_cast<uint64*>(&event.ip),
                                      &name,
                                      &offset)) {
    mapping_failed = true;
  }

  struct ip_callchain* callchain = parsed_event->sample_info->callchain;

  // Map the callchain IPs, if any.
  if (callchain) {
    parsed_event->callchain.resize(callchain->nr);
    for (unsigned int j = 0; j < callchain->nr; ++j) {
      if (!MapIPAndPidAndGetNameAndOffset(callchain->ips[j],
                                          event.pid,
                                          reinterpret_cast<uint64*>(
                                              &callchain->ips[j]),
                                          &parsed_event->callchain[j].dso_name,
                                          &parsed_event->callchain[j].offset)) {
        mapping_failed = true;
      }
    }
  }

  // Map branch stack addresses.
  struct branch_stack* branch_stack = parsed_event->sample_info->branch_stack;
  if (branch_stack) {
    parsed_event->branch_stack.resize(branch_stack->nr);
    for (unsigned int i = 0; i < branch_stack->nr; ++i) {
      struct branch_entry& entry = branch_stack->entries[i];
      ParsedEvent::BranchEntry& parsed_entry = parsed_event->branch_stack[i];
      if (!MapIPAndPidAndGetNameAndOffset(entry.from,
                                          event.pid,
                                          reinterpret_cast<uint64*>(
                                              &entry.from),
                                          &parsed_entry.from.dso_name,
                                          &parsed_entry.from.offset)) {
        mapping_failed = true;
      }
      if (!MapIPAndPidAndGetNameAndOffset(entry.to,
                                          event.pid,
                                          reinterpret_cast<uint64*>(&entry.to),
                                          &parsed_entry.to.dso_name,
                                          &parsed_entry.to.offset)) {
        mapping_failed = true;
      }
      parsed_entry.predicted = entry.flags.predicted;
      CHECK_NE(entry.flags.predicted, entry.flags.mispred);
    }
  }

  return !mapping_failed;
}

bool PerfParser::MapIPAndPidAndGetNameAndOffset(uint64 ip,
                                                uint32 pid,
                                                uint64* new_ip,
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
  if (kernel_mapper_->GetMappedAddress(ip, &mapped_addr)) {
    mapped = true;
    mapper = kernel_mapper_;
  } else {
    while (process_mappers_.find(pid) != process_mappers_.end()) {
      mapper = process_mappers_[pid];
      if (mapper->GetMappedAddress(ip, &mapped_addr)) {
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
    if (name && offset) {
      uint64 id = kuint64max;
      CHECK(mapper->GetMappedIDAndOffset(ip, &id, offset));
      // Make sure the ID points to a valid event.
      CHECK_LE(id, parsed_events_sorted_by_time_.size());
      ParsedEvent* parsed_event = parsed_events_sorted_by_time_[id];
      CHECK_EQ(parsed_event->raw_event->header.type, PERF_RECORD_MMAP);
      *name = parsed_event->raw_event->mmap.filename;
      ++parsed_event->num_samples_in_mmap_region;
    }
    if (do_remap_)
      *new_ip = mapped_addr;
  }
  return mapped;
}

bool PerfParser::MapMmapEvent(struct mmap_event* event, uint64 id) {
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
  if (pgoff == start) {
    // This handles the case where the mmap offset is the same as the start
    // address.  This is the case for the kernel DSO on ARM and i686, as well
    // as for some VDSO's.  e.g.
    //   event.mmap.start = 0x80008200
    //   event.mmap.len   = 0xffffffff7fff7dff
    //   event.mmap.pgoff = 0x80008200
    pgoff = 0;
  } else if (pgoff < len) {
    // This handles the case where the mmap offset somewhere between the start
    // and the end of the mmap region.  This is the case for the kernel DSO on
    // x86_64.  e.g.
    //   event.mmap.start = 0x0
    //   event.mmap.len   = 0xffffffff9fffffff
    //   event.mmap.pgoff = 0xffffffff81000190

    // Sanity check to make sure pgoff is valid.
    // TODO(sque): does not protect against wraparound.
    CHECK_GE(start + pgoff, start);
    len -= event->pgoff;
    start += event->pgoff;
    pgoff = 0;
  }
  if (!mapper->MapWithID(start, len, id, true))
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
  PidTid parent = std::make_pair(event.ppid, event.ptid);
  PidTid child = std::make_pair(event.pid, event.tid);
  if (parent != child &&
      pidtid_to_comm_map_.find(parent) != pidtid_to_comm_map_.end()) {
    pidtid_to_comm_map_[child] = pidtid_to_comm_map_[parent];
  }

  uint32 pid = event.pid;
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
