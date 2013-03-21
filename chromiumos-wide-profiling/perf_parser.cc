// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "perf_parser.h"

#include <algorithm>

#include "base/logging.h"

#include "utils.h"

namespace {

// For kernel MMAP events, the pid is -1.
const uint32 kKernelPid = kuint32max;

// Given a general perf sample format |sample_type|, return the fields of that
// format that are present in a sample for an event of type |event_type|.
//
// e.g. FORK and EXIT events have the fields {time, pid/tid, cpu, id}.
// Given a sample type with fields {ip, time, pid/tid, and period}, return
// the intersection of these two field sets: {time, pid/tid}.
//
// All field formats are bitfields, as defined by enum perf_event_sample_format
// in kernel/perf_event.h.
uint64 GetSampleFieldsForEventType(uint32 event_type, uint64 sample_type) {
  uint64 mask = kuint64max;
  switch (event_type) {
  case PERF_RECORD_SAMPLE:
    mask = PERF_SAMPLE_TIME | PERF_SAMPLE_ID | PERF_SAMPLE_CPU |
           PERF_SAMPLE_PERIOD;
    break;
  case PERF_RECORD_MMAP:
  case PERF_RECORD_FORK:
  case PERF_RECORD_EXIT:
  case PERF_RECORD_COMM:
    mask = PERF_SAMPLE_TID | PERF_SAMPLE_TIME | PERF_SAMPLE_ID |
           PERF_SAMPLE_CPU;
    break;
  // Not currently processing these events.
  case PERF_RECORD_LOST:
  case PERF_RECORD_THROTTLE:
  case PERF_RECORD_UNTHROTTLE:
  case PERF_RECORD_READ:
  case PERF_RECORD_MAX:
    break;
  default:
    LOG(FATAL) << "Unknown event type " << event_type;
  }
  return sample_type & mask;
}

// In perf data, strings are packed into the smallest number of 8-byte blocks
// possible, including the null terminator.
// e.g.
//    "0123"                ->  5 bytes -> packed into  8 bytes
//    "0123456"             ->  8 bytes -> packed into  8 bytes
//    "01234567"            ->  9 bytes -> packed into 16 bytes
//    "0123456789abcd"      -> 15 bytes -> packed into 16 bytes
//    "0123456789abcde"     -> 16 bytes -> packed into 16 bytes
//    "0123456789abcdef"    -> 17 bytes -> packed into 24 bytes
//
// This function returns the length of the 8-byte-aligned for storing |string|.
size_t GetUint64AlignedStringLength(const char* string) {
  return AlignSize(strlen(string) + 1, sizeof(uint64));
}

// Returns the offset in bytes within a perf event structure at which the raw
// perf sample data is located.
uint64 GetPerfSampleDataOffset(const event_t& event) {
  uint64 offset = kuint64max;
  switch (event.header.type) {
  case PERF_RECORD_SAMPLE:
    offset = sizeof(event.ip);
    break;
  case PERF_RECORD_MMAP:
    offset = sizeof(event.mmap) - sizeof(event.mmap.filename) +
             GetUint64AlignedStringLength(event.mmap.filename);
    break;
  case PERF_RECORD_FORK:
  case PERF_RECORD_EXIT:
    offset = sizeof(event.fork);
    break;
  case PERF_RECORD_COMM:
    offset = sizeof(event.comm) - sizeof(event.comm.comm) +
             GetUint64AlignedStringLength(event.comm.comm);
    break;
  case PERF_RECORD_LOST:
  case PERF_RECORD_THROTTLE:
  case PERF_RECORD_UNTHROTTLE:
  case PERF_RECORD_READ:
  case PERF_RECORD_MAX:
    LOG(FATAL) << "Unsupported event type " << event.header.type;
  default:
    LOG(FATAL) << "Unknown event type " << event.header.type;
  }
  // Make sure the offset was valid
  CHECK_NE(offset, kuint64max);
  CHECK_EQ(offset % sizeof(uint64), 0U);
  return offset;
}

size_t ReadPerfSampleFromData(const uint64* array,
                              const uint64 sample_fields,
                              struct perf_sample* sample) {
  size_t num_values_read = 0;
  for (int index = 0; (sample_fields >> index) > 0; ++index) {
    uint64 sample_type = (1 << index);
    union {
      uint32 val32[sizeof(uint64) / sizeof(uint32)];
      uint64 val64;
    };
    if (!(sample_type & sample_fields))
      continue;

    val64 = *array++;
    ++num_values_read;
    switch (sample_type) {
    case PERF_SAMPLE_IP:
      sample->ip = val64;
      break;
    case PERF_SAMPLE_TID:
      // TODO(sque): might have to check for endianness.
      sample->pid = val32[0];
      sample->tid = val32[1];
      break;
    case PERF_SAMPLE_TIME:
      sample->time = val64;
      break;
    case PERF_SAMPLE_ADDR:
      sample->addr = val64;
      break;
    case PERF_SAMPLE_ID:
      sample->id = val64;
      break;
    case PERF_SAMPLE_STREAM_ID:
      sample->stream_id = val64;
      break;
    case PERF_SAMPLE_CPU:
      sample->cpu = val32[0];
      break;
    case PERF_SAMPLE_PERIOD:
      sample->period = val64;
      break;
    default:
      LOG(FATAL) << "Invalid sample type " << (void*)sample_type;
    }
  }
  return num_values_read * sizeof(uint64);
}

size_t WritePerfSampleToData(const struct perf_sample& sample,
                             const uint64 sample_fields,
                             uint64* array) {
  size_t num_values_written = 0;
  for (int index = 0; (sample_fields >> index) > 0; ++index) {
    uint64 sample_type = (1 << index);
    union {
      uint32 val32[sizeof(uint64) / sizeof(uint32)];
      uint64 val64;
    };
    if (!(sample_type & sample_fields))
      continue;

    switch (sample_type) {
    case PERF_SAMPLE_IP:
      val64 = sample.ip;
      break;
    case PERF_SAMPLE_TID:
      val32[0] = sample.pid;
      val32[1] = sample.tid;
      break;
    case PERF_SAMPLE_TIME:
      val64 = sample.time;
      break;
    case PERF_SAMPLE_ADDR:
      val64 = sample.addr;
      break;
    case PERF_SAMPLE_ID:
      val64 = sample.id;
      break;
    case PERF_SAMPLE_STREAM_ID:
      val64 = sample.stream_id;
      break;
    case PERF_SAMPLE_CPU:
      val64 = sample.cpu;
      break;
    case PERF_SAMPLE_PERIOD:
      val64 = sample.period;
      break;
    default:
      LOG(FATAL) << "Invalid sample type " << (void*)sample_type;
    }
    *array++ = val64;
    ++num_values_written;
  }
  return num_values_written * sizeof(uint64);
}

// Copies only the event-specific data from one event_t to another.  The sample
// info is not copied.
void CopyPerfEventSpecificInfo(const event_t& source, event_t* destination) {
  // The sample info offset is equal to the amount of event-specific data that
  // precedes it.
  memcpy(destination, &source, GetPerfSampleDataOffset(source));
}

// Extracts from a perf event |event| info about the perf sample that contains
// the event.  Stores info in |sample|.
bool ReadPerfSampleInfo(const event_t& event,
                        uint64 sample_type,
                        struct perf_sample* sample) {
  CHECK(sample);

  uint64 sample_format = GetSampleFieldsForEventType(event.header.type,
                                                     sample_type);
  uint64 offset = GetPerfSampleDataOffset(event);
  memset(sample, 0, sizeof(*sample));
  size_t size_read = ReadPerfSampleFromData(
      reinterpret_cast<const uint64*>(&event) + offset / sizeof(uint64),
      sample_format,
      sample);

  size_t expected_size = event.header.size - offset;
  return (size_read == expected_size);
}

// Writes |sample| info back to a perf event |event|.
bool WritePerfSampleInfo(const perf_sample& sample,
                         uint64 sample_type,
                         event_t* event) {
  CHECK(event);

  uint64 sample_format = GetSampleFieldsForEventType(event->header.type,
                                                     sample_type);
  uint64 offset = GetPerfSampleDataOffset(*event);

  size_t expected_size = event->header.size - offset;
  memset(reinterpret_cast<uint8*>(event) + offset, 0, expected_size);
  size_t size_written = WritePerfSampleToData(
      sample,
      sample_format,
      reinterpret_cast<uint64*>(event) + offset / sizeof(uint64));

  return (size_written == expected_size);
}

// Returns true if |e1| has an earlier timestamp than |e2|.  The args are const
// pointers instead of references because of the way this function is used when
// calling std::stable_sort.
bool CompareParsedEventTimes(const ParsedEvent* e1, const ParsedEvent* e2) {
  return (e1->sample_info.time < e2->sample_info.time);
}

}  // namespace

PerfParser::~PerfParser() {
  ResetAddressMappers();
}

bool PerfParser::ParseRawEvents() {
  ResetAddressMappers();
  parsed_events_.resize(events_.size());
  for (size_t i = 0; i < events_.size(); ++i) {
    const event_t& raw_event = events_[i];
    ParsedEvent& parsed_event = parsed_events_[i];
    CopyPerfEventSpecificInfo(raw_event, &parsed_event.raw_event);
    if (!ReadPerfSampleInfo(raw_event,
                            attrs_[0].attr.sample_type,
                            &parsed_event.sample_info)) {
      return false;
    }
  }
  SortParsedEvents();
  ProcessEvents();
  return true;
}

bool PerfParser::GenerateRawEvents() {
  events_.resize(parsed_events_.size());
  for (size_t i = 0; i < events_.size(); ++i) {
    const ParsedEvent& parsed_event = parsed_events_[i];
    event_t& raw_event = events_[i];
    CopyPerfEventSpecificInfo(parsed_event.raw_event, &raw_event);
    if (!WritePerfSampleInfo(parsed_event.sample_info,
                             attrs_[0].attr.sample_type,
                             &raw_event)) {
      return false;
    }
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
        DLOG(INFO) << "FORK: " << event.fork.ppid << " -> " << event.fork.pid;
        ++stats_.num_fork_events;
        CHECK(MapForkEvent(event.fork)) << "Unable to map FORK event!";
        break;
      case PERF_RECORD_EXIT:
        // EXIT events have the same structure as FORK events.
        DLOG(INFO) << "EXIT: " << event.fork.pid << " <- " << event.fork.ppid;
        ++stats_.num_exit_events;
        CHECK(MapExitEvent(event.fork)) << "Unable to map EXIT event!";
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
  if (kernel_mapper_.GetMappedAddress(event->ip, &mapped_addr)) {
    mapped = true;
    mapper = &kernel_mapper_;
  } else {
    uint32 pid = event->pid;
    while (process_mappers_.find(pid) != process_mappers_.end()) {
      mapper = process_mappers_[pid];
      if (mapper->GetMappedAddress(event->ip, &mapped_addr)) {
        mapped = true;
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
  AddressMapper* mapper = &kernel_mapper_;
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
    event->len = len;
    event->pgoff = pgoff;
  }
  return true;
}

bool PerfParser::MapForkEvent(const struct fork_event& event) {
  uint32 pid = event.pid;
  if (pid == event.ppid) {
    LOG(ERROR) << "Forked process should not have the same pid as the parent.";
    return false;
  }
  if (process_mappers_.find(pid) != process_mappers_.end()) {
    LOG(ERROR) << "Found an existing process mapper with the new process's ID.";
    return false;
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

bool PerfParser::MapExitEvent(const struct fork_event& event) {
  uint32 pid = event.pid;
  // An exit event may not correspond to a previously processed fork event.
  // There can be redundant process events (from looking at "perf report -D"),
  // or the exiting process could have been created before "perf record" began.
  // So it is not necessarily a problem if there is no existing process mapping.

  // However, if the child-to-parent mapping exists (there was a fork event), so
  // must the process mapper.
  if (child_to_parent_pid_map_.find(pid) != child_to_parent_pid_map_.end() &&
      process_mappers_.find(pid) == process_mappers_.end()) {
    LOG(ERROR) << "Could not find process mapper for exiting process.";
    return false;
  }

  if (process_mappers_.find(pid) != process_mappers_.end()) {
    delete process_mappers_[pid];
    process_mappers_.erase(pid);
  }

  if (child_to_parent_pid_map_.find(pid) != child_to_parent_pid_map_.end())
    child_to_parent_pid_map_.erase(pid);

  return true;
}

void PerfParser::ResetAddressMappers() {
  std::map<uint32, AddressMapper*>::iterator iter;
  for (iter = process_mappers_.begin(); iter != process_mappers_.end(); ++iter)
    delete iter->second;
  process_mappers_.clear();
  child_to_parent_pid_map_.clear();
}
