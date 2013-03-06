// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string.h>
#include <assert.h>
#include <cstdlib>
#include <fstream>

#include "base/logging.h"

#include "perf_reader.h"

namespace {
const uint64 kPerfMagic = 0x32454c4946524550LL;
}  // namespace

bool PerfReader::ReadFileFromHandle(std::ifstream* in) {
  if (!in->good())
    return false;
  if (!ReadHeader(in) || !ReadAttrs(in) || !ReadEventTypes(in) || !ReadData(in))
    return false;
  return true;
}

bool PerfReader::ReadFile(const std::string& filename) {
  std::ifstream in(filename.c_str(), std::ios::binary);
  if (ReadFileFromHandle(&in) == false)
    return false;
  return true;
}

bool PerfReader::WriteFile(const std::string& filename) {
  std::ofstream out(filename.c_str(), std::ios::binary);
  return WriteFileFromHandle(&out);
}

bool PerfReader::WriteFileFromHandle(std::ofstream* out) {
  if (!out->good())
    return false;
  if (!RegenerateHeader())
    return false;
  if (!WriteHeader(out) ||
      !WriteAttrs(out) ||
      !WriteEventTypes(out) ||
      !WriteData(out)) {
    return false;
  }
  return true;
}

bool PerfReader::RegenerateHeader() {
  // This is the order of the input perf file contents:
  // 1. Header
  // 2. Attribute IDs (pointed to by attr.ids.offset)
  // 3. Attributes
  // 4. Event types
  // 5. Data

  // Compute offsets in the above order.
  memset(&out_header_, 0, sizeof(out_header_));
  out_header_.magic = kPerfMagic;
  out_header_.size = sizeof(out_header_);
  out_header_.attr_size = sizeof(attrs_[0].attr) + sizeof(perf_file_section);
  out_header_.attrs.size = out_header_.attr_size * attrs_.size();
  for (size_t i = 0; i < events_.size(); i++)
    out_header_.data.size += events_[i].header.size;
  if (!event_types_.empty()) {
    out_header_.event_types.size =
        event_types_.size() * sizeof(event_types_[0]);
  }

  u64 current_offset = 0;
  current_offset += out_header_.size;
  for (size_t i = 0; i < attrs_.size(); i++)
    current_offset += sizeof(attrs_[i].ids[0]) * attrs_[i].ids.size();
  out_header_.attrs.offset = current_offset;
  current_offset += out_header_.attrs.size;
  out_header_.event_types.offset = current_offset;
  current_offset += out_header_.event_types.size;

  out_header_.data.offset = current_offset;

  return true;
}

bool PerfReader::ReadHeader(std::ifstream* in) {
  in->seekg(0, std::ios::beg);
  in->read(reinterpret_cast<char*>(&header_), sizeof(header_));
  if (!in->good())
    return false;
  if (header_.magic != kPerfMagic) {
    LOG(ERROR) << "Read wrong magic. Expected: " << kPerfMagic
               << " Got: " << header_.magic;
    return false;
  }
  if (header_.size != sizeof(header_)) {
    LOG(ERROR) << "header_.size: " << header_.size << " Expected: "
               << sizeof(header_);
    return false;
  }
  if (header_.attr_size != sizeof(perf_file_attr)) {
    LOG(ERROR) << "header_.attr_size: " << header_.attr_size << " Expected: "
               << sizeof(perf_file_attr);
    return false;
  }
  LOG(INFO) << "event_types.size: " << header_.event_types.size;
  LOG(INFO) << "event_types.offset: " << header_.event_types.offset;

  return true;
}

bool PerfReader::ReadAttrs(std::ifstream* in) {
  size_t num_attrs = header_.attrs.size / header_.attr_size;
  assert(sizeof(struct perf_file_attr) == header_.attr_size);
  attrs_.resize(num_attrs);
  for (size_t i = 0; i < num_attrs; i++) {
    // Read each attr.
    in->seekg(header_.attrs.offset + i * header_.attr_size, std::ios::beg);
    PerfFileAttr& current_attr = attrs_[i];

    in->read(reinterpret_cast<char*>(&current_attr.attr),
             sizeof(current_attr.attr));
    if (!in->good())
      return false;

    // Currently, all perf event types have the same sample format (same fields
    // present).  The following CHECK will catch any data that shows otherwise.
    CHECK_EQ(attrs_[i].attr.sample_type, attrs_[0].attr.sample_type)
        << "Event type #" << i << " sample format does not match format of "
        << "other event type samples.";

    struct perf_file_section ids;
    in->read(reinterpret_cast<char*>(&ids), sizeof(ids));
    if (!in->good())
      return false;

    current_attr.ids.resize(ids.size / sizeof(current_attr.ids[0]));
    for (size_t j = 0; j < ids.size / sizeof(current_attr.ids[0]); j++) {
      in->seekg(ids.offset + j * sizeof(current_attr.ids[0]), std::ios::beg);
      in->read(reinterpret_cast<char*>(&current_attr.ids[j]),
               sizeof(current_attr.ids[j]));
      if (!in->good())
        return false;
    }
  }
  if (num_attrs > 0)
    sample_type_ = attrs_[0].attr.sample_type;

  return true;
}

bool PerfReader::ReadEventTypes(std::ifstream* in) {
  size_t num_event_types = header_.event_types.size /
      sizeof(struct perf_trace_event_type);
  assert(sizeof(struct perf_trace_event_type) * num_event_types ==
         header_.event_types.size);
  event_types_.resize(num_event_types);
  for (size_t i = 0; i < num_event_types; ++i) {
    // Read each event type.
    struct perf_trace_event_type& current_type = event_types_[i];
    in->seekg(header_.event_types.offset + i * sizeof(current_type),
              std::ios::beg);

    in->read(reinterpret_cast<char*>(&current_type.event_id),
             sizeof(current_type.event_id));
    if (!in->good())
      return false;
    in->read(current_type.name, sizeof(current_type.name));
    if (!in->good())
      return false;
  }

  return true;
}

bool PerfReader::ReadData(std::ifstream* in) {
  in->seekg(header_.data.offset, std::ios::beg);
  u64 data_remaining_bytes = header_.data.size;
  while (data_remaining_bytes != 0) {
    perf_event_header pe_header;
    in->read(reinterpret_cast<char*>(&pe_header), sizeof(pe_header));
    if (!in->good())
      return false;

    LOG(INFO) << "Data remaining: " << data_remaining_bytes;
    LOG(INFO) << "Data type: " << pe_header.type;
    LOG(INFO) << "Data size: " << pe_header.size;
    size_t remaining_size = pe_header.size - sizeof(pe_header);
    LOG(INFO) << "Seek size: " << remaining_size;
    data_remaining_bytes -= pe_header.size;

    if (pe_header.size > sizeof(event_t)) {
      LOG(INFO) << "Data size: " << pe_header.size << " sizeof(event_t): "
                << sizeof(event_t);
      return false;
    }

    in->seekg(-1L * (long)(sizeof(pe_header)), std::ios::cur);
    events_.resize(events_.size() + 1);

    in->read(reinterpret_cast<char*>(&events_.back()), pe_header.size);
    if (!in->good())
      return false;

    bool event_processed = ProcessEvent(events_.back());
    if (!event_processed)
      return false;

    if (pe_header.type == PERF_RECORD_COMM) {
      LOG(INFO) << "len: " << pe_header.size;
      LOG(INFO) << "sizeof header: " << sizeof(pe_header);
      LOG(INFO) << "sizeof comm_event: " << sizeof(comm_event);
    }
  }

  LOG(INFO) << "Number of events_ stored: "<< events_.size();

  return true;
}

bool PerfReader::WriteHeader(std::ofstream* out) const {
  out->seekp(0, std::ios::beg);
  out->write(reinterpret_cast<const char*>(&out_header_), sizeof(out_header_));
  return out->good();
}

bool PerfReader::WriteData(std::ofstream* out) const {
  out->seekp(out_header_.data.offset, std::ios::beg);
  for (std::vector<event_t>::const_iterator it = events_.begin();
      it < events_.end();
      ++it) {
    bool event_written = WriteEventFull(out, *it);
    if (!event_written) {
      LOG(ERROR) << "Could not write event!";
      return false;
    }
  }
  return true;
}

bool PerfReader::WriteAttrs(std::ofstream* out) const {
  u64 ids_size = 0;
  for (size_t i = 0; i < attrs_.size(); i++) {
    struct perf_file_section ids;
    ids.offset = out_header_.size + ids_size;
    ids.size = attrs_[i].ids.size() * sizeof(attrs_[i].ids[0]);

    out->seekp(ids.offset, std::ios::beg);
    for (size_t j = 0; j < attrs_[i].ids.size(); j++) {
      out->write(reinterpret_cast<const char*>(&attrs_[i].ids[j]),
                 sizeof(attrs_[i].ids[j]));
      if (!out->good())
        return false;
    }

    out->seekp(out_header_.attrs.offset + out_header_.attr_size * i,
               std::ios::beg);
    out->write(reinterpret_cast<const char*>(&attrs_[i].attr),
               sizeof(attrs_[i].attr));
    if (!out->good())
      return false;

    out->write(reinterpret_cast<const char*>(&ids), sizeof(ids));
    if (!out->good())
      return false;
    ids_size += ids.size;
  }

  return true;
}

bool PerfReader::WriteEventTypes(std::ofstream* out) const {
  for (size_t i = 0; i < event_types_.size(); ++i) {
    out->seekp(out_header_.event_types.offset +
                  sizeof(struct perf_trace_event_type) * i,
               std::ios::beg);
    out->write(reinterpret_cast<const char*>(&event_types_[i].event_id),
               sizeof(event_types_[i].event_id));
    if (!out->good())
      return false;
    out->write(event_types_[i].name, sizeof(event_types_[i].name));
    if (!out->good())
      return false;
  }

  return true;
}

bool PerfReader::WriteEventFull(std::ofstream* out,
                                const event_t& event) const {
  out->write(reinterpret_cast<const char*>(&event), event.header.size);
  return out->good();
}

bool PerfReader::ProcessEvent(const event_t& event) {
  switch (event.header.type) {
    case PERF_RECORD_SAMPLE:
      LOG(INFO) << "IP: " << reinterpret_cast<void*>(event.ip.ip);
      break;
    case PERF_RECORD_MMAP:
      LOG(INFO) << "MMAP: " << event.mmap.filename;
      break;
    case PERF_RECORD_FORK:
      LOG(INFO) << "FORK: " << event.fork.pid << " <- " << event.fork.ppid;
      break;
    case PERF_RECORD_EXIT:
      // EXIT events have the same structure as FORK events.
      LOG(INFO) << "EXIT: " << event.fork.pid << " <- " << event.fork.ppid;
      break;
    case PERF_RECORD_LOST:
    case PERF_RECORD_COMM:
    case PERF_RECORD_THROTTLE:
    case PERF_RECORD_UNTHROTTLE:
    case PERF_RECORD_READ:
    case PERF_RECORD_MAX:
      LOG(INFO) << "Read event type: " << event.header.type
                << ". Doing nothing.";
      break;
    default:
      LOG(ERROR) << "Unknown event type: " << event.header.type;
      return false;
  }
  return true;
}
