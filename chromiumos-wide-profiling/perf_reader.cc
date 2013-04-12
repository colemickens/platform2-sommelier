// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string.h>
#include <assert.h>
#include <cstdlib>

#include <vector>

#include "base/logging.h"

#include "perf_reader.h"
#include "quipper_string.h"
#include "utils.h"

namespace {

// The first 64 bits of the perf header, used as a perf data file ID tag.
const uint64 kPerfMagic = 0x32454c4946524550LL;

bool ReadDataFromVector(const std::vector<char>& data,
                        size_t src_offset,
                        size_t length,
                        const string& value_name,
                        void* dest) {
  if (data.size() < src_offset + length) {
    LOG(ERROR) << "Not enough bytes to read " << value_name;
    return false;
  }
  memcpy(dest, &data[src_offset], length);
  return true;
}

bool WriteDataToVector(const void* data,
                       size_t dest_offset,
                       size_t length,
                       const string& value_name,
                       std::vector<char>* dest) {
  if (dest->size() < dest_offset + length) {
    LOG(ERROR) << "No space in buffer to write " << value_name;
    return false;
  }
  memcpy(&(*dest)[dest_offset], data, length);
  return true;
}

}  // namespace

bool PerfReader::ReadFile(const string& filename) {
  std::vector<char> data;
  if (!ReadFileToData(filename, &data))
    return false;
  return ReadFileData(data);
}

bool PerfReader::ReadFileData(const std::vector<char>& data) {
  if (data.empty())
    return false;
  if (!ReadHeader(data))
    return false;

  // Check if it is normal perf data.
  if (header_.size == sizeof(header_)) {
    LOG(INFO) << "Perf data is in normal format.";
    return (ReadAttrs(data) && ReadEventTypes(data) && ReadData(data));
  }

  // Otherwise it is piped data.
  if (piped_header_.size != sizeof(piped_header_)) {
    LOG(ERROR) << "Expecting piped data format, but header size "
               << piped_header_.size << " does not match expected size "
               << sizeof(piped_header_);
    return false;
  }

  return ReadPipedData(data);
}

bool PerfReader::WriteFile(const string& filename) {
  if (!RegenerateHeader())
    return false;;

  // Compute the total perf file data to be written;
  size_t total_size = 0;
  total_size += out_header_.size;
  total_size += out_header_.attrs.size;
  total_size += out_header_.event_types.size;
  total_size += out_header_.data.size;
  // Add the ID info, whose size is not explicitly included in the header.
  for (size_t i = 0; i < attrs_.size(); ++i)
    total_size += attrs_[i].ids.size() * sizeof(attrs_[i].ids[0]);

  // Write all data into a vector.
  std::vector<char> data;
  data.resize(total_size);
  if (!WriteHeader(&data) ||
      !WriteAttrs(&data) ||
      !WriteEventTypes(&data) ||
      !WriteData(&data)) {
    return false;
  }
  return WriteDataToFile(data, filename);
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

bool PerfReader::ReadHeader(const std::vector<char>& data) {
  if (!ReadDataFromVector(data, 0, sizeof(header_), "header data", &header_))
    return false;
  if (header_.magic != kPerfMagic) {
    LOG(ERROR) << "Read wrong magic. Expected: " << kPerfMagic
               << " Got: " << header_.magic;
    return false;
  }

  // Header can be a piped header.
  if (header_.size != sizeof(header_))
    return true;

  if (header_.attr_size != sizeof(perf_file_attr)) {
    LOG(ERROR) << "header_.attr_size: " << header_.attr_size << " Expected: "
               << sizeof(perf_file_attr);
    return false;
  }
  LOG(INFO) << "event_types.size: " << header_.event_types.size;
  LOG(INFO) << "event_types.offset: " << header_.event_types.offset;

  return true;
}

bool PerfReader::ReadAttrs(const std::vector<char>& data) {
  size_t num_attrs = header_.attrs.size / header_.attr_size;
  assert(sizeof(struct perf_file_attr) == header_.attr_size);
  attrs_.resize(num_attrs);
  for (size_t i = 0; i < num_attrs; i++) {
    size_t offset = header_.attrs.offset + i * header_.attr_size;
    // Read each attr.
    PerfFileAttr& current_attr = attrs_[i];
    struct perf_event_attr& attr = current_attr.attr;
    if (!ReadDataFromVector(data, offset, sizeof(attr), "attribute", &attr))
      return false;
    offset += sizeof(current_attr.attr);

    // Currently, all perf event types have the same sample format (same fields
    // present).  The following CHECK will catch any data that shows otherwise.
    CHECK_EQ(attrs_[i].attr.sample_type, attrs_[0].attr.sample_type)
        << "Event type #" << i << " sample format does not match format of "
        << "other event type samples.";

    struct perf_file_section ids;
    if (!ReadDataFromVector(data, offset, sizeof(ids), "ID section info", &ids))
      return false;
    offset += sizeof(ids);

    current_attr.ids.resize(ids.size / sizeof(current_attr.ids[0]));
    for (size_t j = 0; j < ids.size / sizeof(current_attr.ids[0]); j++) {
      uint64& id = current_attr.ids[j];
      offset = ids.offset + j * sizeof(id);
      if (!ReadDataFromVector(data, offset, sizeof(id), "ID", &id))
        return false;
    }
  }
  if (num_attrs > 0)
    sample_type_ = attrs_[0].attr.sample_type;

  return true;
}

bool PerfReader::ReadEventTypes(const std::vector<char>& data) {
  size_t num_event_types = header_.event_types.size /
      sizeof(struct perf_trace_event_type);
  assert(sizeof(struct perf_trace_event_type) * num_event_types ==
         header_.event_types.size);
  event_types_.resize(num_event_types);
  for (size_t i = 0; i < num_event_types; ++i) {
    // Read each event type.
    struct perf_trace_event_type& type = event_types_[i];
    size_t offset = header_.event_types.offset + i * sizeof(type);
    if (!ReadDataFromVector(data, offset, sizeof(type), "event type", &type))
      return false;
  }

  return true;
}

bool PerfReader::ReadData(const std::vector<char>& data) {
  u64 data_remaining_bytes = header_.data.size;
  size_t offset = header_.data.offset;
  while (data_remaining_bytes != 0) {
    if (data.size() < offset) {
      LOG(ERROR) << "Not enough data to read a perf event.";
      return false;
    }

    const event_t* event_data = reinterpret_cast<const event_t*>(&data[offset]);
    if (!ReadPerfEventBlock(*event_data))
      return false;
    DLOG(INFO) << "Data remaining: " << data_remaining_bytes;
    data_remaining_bytes -= event_data->header.size;
    offset += event_data->header.size;
  }

  LOG(INFO) << "Number of events stored: "<< events_.size();
  return true;
}

bool PerfReader::ReadPipedData(const std::vector<char>& data) {
  size_t offset = piped_header_.size;
  bool result = true;
  while (offset < data.size() && result) {
    const union piped_data_block {
      struct perf_event_header header;
      struct attr_event attr_event;
      struct event_type_event event_type_event;
      event_t event;
    } *block;
    block = reinterpret_cast<const union piped_data_block*>(&data[offset]);
    offset += block->header.size;
    if (block->header.type < PERF_RECORD_MAX) {
      result = ReadPerfEventBlock(block->event);
      continue;
    }
    switch (block->header.type) {
    case PERF_RECORD_HEADER_ATTR:
      result = ReadAttrEventBlock(block->attr_event);
      break;
    case PERF_RECORD_HEADER_EVENT_TYPE:
      result = ReadEventTypeEventBlock(block->event_type_event);
      break;
    default:
      LOG(ERROR) << "Event type " << block->header.type
                 << " is not yet supported!";
      return false;
    }
  }
  return result;
}

bool PerfReader::WriteHeader(std::vector<char>* data) const {
  size_t header_size = sizeof(out_header_);
  return WriteDataToVector(&out_header_, 0, header_size, "file header", data);
}

bool PerfReader::WriteAttrs(std::vector<char>* data) const {
  u64 ids_size = 0;
  for (size_t i = 0; i < attrs_.size(); i++) {
    const PerfFileAttr& attr = attrs_[i];

    struct perf_file_section ids;
    ids.offset = out_header_.size + ids_size;
    ids.size = attr.ids.size() * sizeof(attr.ids[0]);

    for (size_t j = 0; j < attr.ids.size(); j++) {
      const uint64& id = attr.ids[j];
      size_t offset = ids.offset + j * sizeof(id);
      if (!WriteDataToVector(&id, offset, sizeof(id), "ID info", data))
        return false;
    }

    size_t offset = out_header_.attrs.offset + out_header_.attr_size * i;
    if (!WriteDataToVector(&attr.attr,
                           offset,
                           sizeof(attr.attr),
                           "attr info",
                           data)) {
      return false;
    }

    offset += sizeof(attr.attr);
    if (!WriteDataToVector(&ids, offset, sizeof(ids), "ID section info", data))
      return false;

    ids_size += ids.size;
  }

  return true;
}

bool PerfReader::WriteData(std::vector<char>* data) const {
  size_t offset = out_header_.data.offset;
  for (std::vector<event_t>::const_iterator it = events_.begin();
      it < events_.end();
      ++it) {
    if (!WriteDataToVector(&(*it), offset, it->header.size, "event data", data))
      return false;
    offset += it->header.size;
  }
  return true;
}

bool PerfReader::WriteEventTypes(std::vector<char>* data) const {
  for (size_t i = 0; i < event_types_.size(); ++i) {
    size_t offset = out_header_.event_types.offset +
                    sizeof(struct perf_trace_event_type) * i;
    const struct perf_trace_event_type& event_type = event_types_[i];
    if (!WriteDataToVector(&event_type,
                           offset,
                           sizeof(event_type),
                           "event type info",
                           data)) {
      return false;
    }
    offset += sizeof(event_type);
  }
  return true;
}

bool PerfReader::ReadAttrEventBlock(const struct attr_event& attr_event) {
  PerfFileAttr attr;
  attr.attr = attr_event.attr;
  size_t num_ids = (attr_event.header.size - sizeof(attr_event.header) -
                    sizeof(attr_event.attr)) / sizeof(attr_event.id[0]);
  for (size_t i = 0; i < num_ids; ++i)
    attr.ids.push_back(attr_event.id[i]);
  attrs_.push_back(attr);
  return true;
}

bool PerfReader::ReadEventTypeEventBlock(
    const struct event_type_event& event_type_event) {
  event_types_.push_back(event_type_event.event_type);
  return true;
}

bool PerfReader::ReadPerfEventBlock(const event_t& event) {
  const perf_event_header& pe_header = event.header;

  DLOG(INFO) << "Data type: " << pe_header.type;
  DLOG(INFO) << "Data size: " << pe_header.size;
  DLOG(INFO) << "Seek size: " << pe_header.size - sizeof(pe_header);

  if (pe_header.size > sizeof(event_t)) {
    LOG(INFO) << "Data size: " << pe_header.size << " sizeof(event_t): "
              << sizeof(event_t);
    return false;
  }

  events_.push_back(event);

  if (pe_header.type == PERF_RECORD_COMM) {
    DLOG(INFO) << "len: " << pe_header.size;
    DLOG(INFO) << "sizeof header: " << sizeof(pe_header);
    DLOG(INFO) << "sizeof comm_event: " << sizeof(comm_event);
  }
  return true;
}
