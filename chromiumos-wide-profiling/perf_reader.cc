// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <byteswap.h>

#include <cstdlib>
#include <cstring>
#include <vector>

#include "base/logging.h"

#include "perf_reader.h"
#include "quipper_string.h"
#include "utils.h"

namespace quipper {

namespace {

// The first 64 bits of the perf header, used as a perf data file ID tag.
const uint64 kPerfMagic = 0x32454c4946524550LL;

// A mask that is applied to metadata_mask_ in order to get a mask for
// only the metadata supported by quipper.
// Currently, we support hostname, osrelease, version, arch, cpudesck,
// and branchstack.
// The mask is computed as (1 << HEADER_HOSTNAME) |
// (1 << HEADER_OSRELEASE) | ... | (1 << HEADER_BRANCH_STACK)
const uint32 kSupportedMetadataMask = 0x8178;

// Eight bits in a byte.
size_t BytesToBits(size_t num_bytes) {
  return num_bytes * 8;
}

template <class T>
void ByteSwap(T* input) {
  switch(sizeof(T)) {
  case sizeof(uint8):
    LOG(WARNING) << "Attempting to byte swap on a single byte.";
    break;
  case sizeof(uint16):
    *input = bswap_16(*input);
    break;
  case sizeof(uint32):
    *input = bswap_32(*input);
    break;
  case sizeof(uint64):
    *input = bswap_64(*input);
    break;
  default:
    LOG(FATAL) << "Invalid size for byte swap: " << sizeof(T) << " bytes";
    break;
  }
}

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
    // IP and pid/tid fields of sample events are read as part of event_t, so
    // mask away those two fields.
    mask = ~(PERF_SAMPLE_IP | PERF_SAMPLE_TID);
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
    offset = sizeof(event.header);
    break;
  default:
    LOG(FATAL) << "Unknown event type " << event.header.type;
    break;
  }
  // Make sure the offset was valid
  CHECK_NE(offset, kuint64max);
  CHECK_EQ(offset % sizeof(uint64), 0U);
  return offset;
}

size_t ReadPerfSampleFromData(const uint64* array,
                              const uint64 sample_fields,
                              bool swap_bytes,
                              struct perf_sample* sample) {
  size_t num_values_read = 0;
  const uint64 k32BitFields = PERF_SAMPLE_TID | PERF_SAMPLE_CPU;
  bool read_callchain = false;
  bool read_branch_stack = false;

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

    if (swap_bytes) {
      if (k32BitFields & sample_type) {
        ByteSwap(&val32[0]);
        ByteSwap(&val32[1]);
      } else {
        ByteSwap(&val64);
      }
    }

    switch (sample_type) {
    case PERF_SAMPLE_IP:
      sample->ip = val64;
      break;
    case PERF_SAMPLE_TID:
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
    case PERF_SAMPLE_CALLCHAIN:
      // Call chain is a special case.  It comes after the other fields in the
      // sample info data, regardless of the order of |sample_type| bits.
      read_callchain = true;
      --num_values_read;
      --array;
      break;
    case PERF_SAMPLE_BRANCH_STACK:
      // Branch infois a special case just like call chain, and comes after the
      // other sample info data and call chain data.
      read_branch_stack = true;
      --num_values_read;
      --array;
      break;
    default:
      LOG(FATAL) << "Invalid sample type " << (void*)sample_type;
    }
  }

  if (read_callchain) {
    // Make sure there is no existing allocated memory in |sample->callchain|.
    CHECK_EQ(static_cast<void*>(NULL), sample->callchain);

    // The callgraph data consists of a uint64 value |nr| followed by |nr|
    // addresses.
    uint64 callchain_size = *array++;
    if (swap_bytes)
      ByteSwap(&callchain_size);
    struct ip_callchain* callchain =
        reinterpret_cast<struct ip_callchain*>(new uint64[callchain_size + 1]);
    callchain->nr = callchain_size;
    for (size_t i = 0; i < callchain_size; ++i) {
      callchain->ips[i] = *array++;
      if (swap_bytes)
        ByteSwap(&callchain->ips[i]);
    }
    num_values_read += callchain_size + 1;
    sample->callchain = callchain;
  }

  if (read_branch_stack) {
    // Make sure there is no existing allocated memory in
    // |sample->branch_stack|.
    CHECK_EQ(static_cast<void*>(NULL), sample->branch_stack);

    // The branch stack data consists of a uint64 value |nr| followed by |nr|
    // branch_entry structs.
    uint64 branch_stack_size = *array++;
    if (swap_bytes)
      ByteSwap(&branch_stack_size);
    struct branch_stack* branch_stack =
        reinterpret_cast<struct branch_stack*>(
            new uint8[sizeof(uint64) +
                      branch_stack_size * sizeof(struct branch_entry)]);
    branch_stack->nr = branch_stack_size;
    for (size_t i = 0; i < branch_stack_size; ++i) {
      memcpy(&branch_stack->entries[i], array, sizeof(struct branch_entry));
      array += sizeof(struct branch_entry) / sizeof(*array);
      if (swap_bytes) {
        ByteSwap(&branch_stack->entries[i].from);
        ByteSwap(&branch_stack->entries[i].to);
      }
    }
    num_values_read +=
        branch_stack_size * sizeof(struct branch_entry) / sizeof(uint64) + 1;
    sample->branch_stack = branch_stack;
  }

  return num_values_read * sizeof(uint64);
}

size_t WritePerfSampleToData(const struct perf_sample& sample,
                             const uint64 sample_fields,
                             uint64* array) {
  size_t num_values_written = 0;
  bool write_callchain = false;
  bool write_branch_stack = false;

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
    case PERF_SAMPLE_CALLCHAIN:
      write_callchain = true;
      continue;
    case PERF_SAMPLE_BRANCH_STACK:
      write_branch_stack = true;
      continue;
    default:
      LOG(FATAL) << "Invalid sample type " << (void*)sample_type;
    }
    *array++ = val64;
    ++num_values_written;
  }

  if (write_callchain) {
    *array++ = sample.callchain->nr;
    for (size_t i = 0; i < sample.callchain->nr; ++i)
      *array++ = sample.callchain->ips[i];
    num_values_written += sample.callchain->nr + 1;
  }

  if (write_branch_stack) {
    *array++ = sample.branch_stack->nr;
    ++num_values_written;
    for (size_t i = 0; i < sample.branch_stack->nr; ++i) {
      *array++ = sample.branch_stack->entries[i].from;
      *array++ = sample.branch_stack->entries[i].to;
      *array++ =
          *reinterpret_cast<uint64*>(&sample.branch_stack->entries[i].flags);
      num_values_written += 3;
    }
  }

  return num_values_written * sizeof(uint64);
}

// Extracts from a perf event |event| info about the perf sample that contains
// the event.  Stores info in |sample|.
bool ReadPerfSampleInfo(const event_t& event,
                        uint64 sample_type,
                        bool swap_bytes,
                        struct perf_sample* sample) {
  CHECK(sample);

  uint64 sample_format = GetSampleFieldsForEventType(event.header.type,
                                                     sample_type);
  uint64 offset = GetPerfSampleDataOffset(event);
  memset(sample, 0, sizeof(*sample));
  size_t size_read = ReadPerfSampleFromData(
      reinterpret_cast<const uint64*>(&event) + offset / sizeof(uint64),
      sample_format,
      swap_bytes,
      sample);

  if (event.header.type == PERF_RECORD_SAMPLE) {
    sample->pid = event.ip.pid;
    sample->tid = event.ip.tid;
    if (swap_bytes) {
      ByteSwap(&sample->pid);
      ByteSwap(&sample->tid);
    }
  }

  size_t expected_size = event.header.size - offset;
  if (size_read != expected_size) {
    LOG(ERROR) << "Read " << size_read << " bytes, expected "
               << expected_size << " bytes.";
  }

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
  if (size_written != expected_size) {
    LOG(ERROR) << "Wrote " << size_written << " bytes, expected "
               << expected_size << " bytes.";
  }

  return (size_written == expected_size);
}

}  // namespace

PerfReader::~PerfReader() {
  // Free allocated memory.
  for (size_t i = 0; i < events_.size(); ++i) {
    if (events_[i].sample_info.callchain)
      delete [] events_[i].sample_info.callchain;
    if (events_[i].sample_info.branch_stack)
      delete [] events_[i].sample_info.branch_stack;
  }
}

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
    metadata_mask_ = header_.adds_features[0];
    return (ReadAttrs(data) && ReadEventTypes(data) && ReadData(data)
            && ReadMetadata(data));
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

  // Add the sizes of the various metadata.
  for (size_t i = 0; i < string_metadata_.size(); ++i)
    total_size += string_metadata_[i].size;

  // Additional info about metadata.  See WriteMetadata for explanation.
  total_size += (string_metadata_.size() + 1) * 2 * sizeof(u64);

  // Write all data into a vector.
  std::vector<char> data;
  data.resize(total_size);
  if (!WriteHeader(&data) ||
      !WriteAttrs(&data) ||
      !WriteEventTypes(&data) ||
      !WriteData(&data) ||
      !WriteMetadata(&data)) {
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
    out_header_.data.size += events_[i].event.header.size;
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

  // Construct the header feature bits.
  memset(&out_header_.adds_features, 0, sizeof(out_header_.adds_features));
  // The following code makes the assumption that all feature bits are in the
  // first word of |adds_features|.  If the perf data format changes and the
  // assumption is no longer valid, this CHECK will fail, at which point the
  // below code needs to be updated.  For now, sticking to that assumption keeps
  // the code simple.
  // This assumption is also used when reading metadata, so that code
  // will also have to be updated if this CHECK starts to fail.
  CHECK_LE(static_cast<size_t>(HEADER_LAST_FEATURE),
           BytesToBits(sizeof(out_header_.adds_features[0])));
  if (sample_type_ & PERF_SAMPLE_BRANCH_STACK)
    out_header_.adds_features[0] |= (1 << HEADER_BRANCH_STACK);
  out_header_.adds_features[0] |= metadata_mask_ & kSupportedMetadataMask;

  return true;
}

bool PerfReader::ReadHeader(const std::vector<char>& data) {
  if (!ReadDataFromVector(data, 0, sizeof(header_), "header data", &header_))
    return false;
  if (header_.magic != kPerfMagic && header_.magic != bswap_64(kPerfMagic)) {
    LOG(ERROR) << "Read wrong magic. Expected: 0x" << std::hex << kPerfMagic
               << " or 0x" << std::hex << bswap_64(kPerfMagic)
               << " Got: 0x" << std::hex << header_.magic;
    return false;
  }
  is_cross_endian_ = (header_.magic != kPerfMagic);
  if (is_cross_endian_)
    ByteSwap(&header_.size);

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
  CHECK_EQ(sizeof(struct perf_file_attr), header_.attr_size);
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
      uint64 id = 0;
      offset = ids.offset + j * sizeof(id);
      if (!ReadDataFromVector(data, offset, sizeof(id), "ID", &id))
        return false;
      current_attr.ids[j] = id;
    }
  }
  if (num_attrs > 0)
    sample_type_ = attrs_[0].attr.sample_type;

  return true;
}

bool PerfReader::ReadEventTypes(const std::vector<char>& data) {
  size_t num_event_types = header_.event_types.size /
      sizeof(struct perf_trace_event_type);
  CHECK_EQ(sizeof(struct perf_trace_event_type) * num_event_types,
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
    data_remaining_bytes -= event_data->header.size;
    offset += event_data->header.size;
  }

  LOG(INFO) << "Number of events stored: "<< events_.size();
  return true;
}

bool PerfReader::ReadMetadata(const std::vector<char>& data) {
  size_t offset = header_.data.offset + header_.data.size;

  for (u32 type = HEADER_FIRST_FEATURE; type != HEADER_LAST_FEATURE; ++type) {
    if ((metadata_mask_ & (1 << type)) == 0)
      continue;

    if (data.size() < offset) {
      LOG(ERROR) << "Not enough data to read offset and size of metadata.";
      return false;
    }

    u64 metadata_offset = *(reinterpret_cast<const u64*>(&data[offset]));
    offset += sizeof(metadata_offset) / sizeof(data[offset]);
    u64 metadata_size = *(reinterpret_cast<const u64*>(&data[offset]));
    offset += sizeof(metadata_size) / sizeof(data[offset]);

    if (data.size() < metadata_offset + metadata_size) {
      LOG(ERROR) << "Not enough data to read metadata.";
      return false;
    }

    switch (type) {
      case HEADER_HOSTNAME:
      case HEADER_OSRELEASE:
      case HEADER_VERSION:
      case HEADER_ARCH:
      case HEADER_CPUDESC:
        if (!ReadStringMetadata(data, type, metadata_offset, metadata_size))
          return false;
        break;
      case HEADER_BRANCH_STACK:
        continue;
      default: LOG(INFO) << "Unsupported metadata type: " << type;
        break;
    }
  }

  return true;
}

bool PerfReader::ReadStringMetadata(const std::vector<char>& data, u32 type,
                                    size_t offset, size_t size) {
  PerfStringMetadata str_data;
  str_data.type = type;
  str_data.size = size;

  str_data.len = *(reinterpret_cast<const u32*>(&data[offset]));
  offset += sizeof(str_data.len) / sizeof(data[offset]);
  str_data.data = string(&data[offset]);

  string_metadata_.push_back(str_data);
  return true;
}

bool PerfReader::ReadPipedData(const std::vector<char>& data) {
  size_t offset = piped_header_.size;
  bool result = true;
  while (offset < data.size() && result) {
    union piped_data_block {
      // Generic format of a piped perf data block, with header + more data.
      struct {
        struct perf_event_header header;
        char more_data[];
      };
      // Specific types of data.
      struct attr_event attr_event;
      struct event_type_event event_type_event;
      struct event_desc_event event_desc_event;
      event_t event;
    };
    const union piped_data_block *block_data
        = reinterpret_cast<const union piped_data_block*>(&data[offset]);
    union piped_data_block block;

    // Copy the header and swap bytes if necessary.
    memcpy(&block.header, block_data, sizeof(block.header));
    if (is_cross_endian_) {
      ByteSwap(&block.header.type);
      ByteSwap(&block.header.misc);
      ByteSwap(&block.header.size);
    }
    // Copy the rest of the data.
    memcpy(&block.more_data,
           block_data->more_data,
           block.header.size - sizeof(block.header));

    offset += block.header.size;
    if (block.header.type < PERF_RECORD_MAX) {
      result = ReadPerfEventBlock(block.event);
      continue;
    }
    switch (block.header.type) {
    case PERF_RECORD_HEADER_ATTR:
      result = ReadAttrEventBlock(block.attr_event);
      break;
    case PERF_RECORD_HEADER_EVENT_TYPE:
      result = ReadEventTypeEventBlock(block.event_type_event);
      break;
    case PERF_RECORD_HEADER_EVENT_DESC:
      result = ReadEventDescEventBlock(block.event_desc_event);
      break;
    default:
      LOG(WARNING) << "Event type " << block.header.type
                   << " is not yet supported!";
      break;
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
      const uint64 id = attr.ids[j];
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
  for (size_t i = 0; i < events_.size(); ++i) {
    // First write to a local event object.
    event_t event = events_[i].event;
    uint32 event_size = event.header.size;
    const struct perf_sample& sample_info = events_[i].sample_info;
    if (!WritePerfSampleInfo(sample_info, sample_type_, &event))
      return false;
    // Then write that local event object to the data buffer.
    if (!WriteDataToVector(&event, offset, event_size, "event data", data))
      return false;
    offset += event.header.size;
  }
  return true;
}

bool PerfReader::WriteMetadata(std::vector<char>* data) const {
  //TODO: Remove once we support metadata for piped data
  if (string_metadata_.size() == 0)
    return true;
  size_t header_offset = out_header_.data.offset + out_header_.data.size;
  // Before writing the metadata, there is one header for each piece
  // of metadata, and one extra showing the end of the file.
  // Each header contains two 64-bit numbers (offset and size).
  u64 metadata_offset =
      header_offset + (string_metadata_.size() + 1) * 2 * sizeof(u64);

  // Zero out the memory used by the headers
  memset(&(*data)[header_offset], 0,
         (metadata_offset - header_offset) * sizeof((*data)[header_offset]));

  for (u32 type = HEADER_FIRST_FEATURE; type != HEADER_LAST_FEATURE; ++type) {
    if ((out_header_.adds_features[0] & (1 << type)) == 0)
      continue;

    // metadata must be initialized by the method that writes the
    // metadata to the data vector (for example, WriteStringMetadata).
    const PerfMetadata* metadata = NULL;

    // Write actual metadata to address metadata_offset
    switch (type) {
      case HEADER_HOSTNAME:
      case HEADER_OSRELEASE:
      case HEADER_VERSION:
      case HEADER_ARCH:
      case HEADER_CPUDESC:
        if (!WriteStringMetadata(type, metadata_offset, &metadata, data))
          return false;
        break;
      case HEADER_BRANCH_STACK:
        continue;
      default: LOG(ERROR) << "Unsupported metadata type: " << type;
        return false;
    }

    // Write metadata offset and size to address header_offset.
    if (!WriteDataToVector(&metadata_offset, header_offset,
                           sizeof(metadata_offset), "metadata offset", data))
      return false;
    header_offset += sizeof(metadata_offset) / sizeof((*data)[header_offset]);

    if (!WriteDataToVector(&metadata->size, header_offset,
                           sizeof(metadata->size), "metadata size", data))
      return false;
    header_offset += sizeof(metadata_offset) / sizeof((*data)[header_offset]);

    // Set up metadata_offset for next metadata event.
    // metadata->size is given in units of char.
    metadata_offset +=
        metadata->size * sizeof((*data)[header_offset]) / sizeof(char);
  }

  // Write the last entry - a pointer to the end of the file
  if (!WriteDataToVector(&metadata_offset, header_offset,
                         sizeof(metadata_offset), "metadata offset", data))
    return false;

  return true;
}

bool PerfReader::WriteStringMetadata(u32 type, size_t offset,
                                     const PerfMetadata** metadata_handle,
                                     std::vector<char>* data) const {
  for (size_t i = 0; i < string_metadata_.size(); ++i) {
    const PerfStringMetadata& str_data = string_metadata_[i];
    if (str_data.type == type) {
      *metadata_handle = &str_data;

      if (!WriteDataToVector(&str_data.len, offset, sizeof(str_data.len),
                             "length of string metadata", data))
        return false;
      offset += sizeof(str_data.len) / sizeof((*data)[offset]);

      memset(&(*data)[offset], 0, str_data.len * sizeof((*data)[offset]));

      const char* data_to_write = str_data.data.c_str();
      size_t length_using_strlen = strlen(data_to_write);
      CHECK_LE(length_using_strlen, str_data.len);

      if (!WriteDataToVector(data_to_write, offset,
                             length_using_strlen * sizeof(char),
                             "string metadata", data))
        return false;

      return true;
    }
  }
  LOG(ERROR) << "String metadata of type " << type << " not present";
  return false;
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
  // Reverse bit order for big endian machine data.
  if (is_cross_endian_) {
    ByteSwap(&attr.attr.type);
    ByteSwap(&attr.attr.size);
    ByteSwap(&attr.attr.config);
    ByteSwap(&attr.attr.sample_period);
    ByteSwap(&attr.attr.sample_type);
    ByteSwap(&attr.attr.read_format);
    ByteSwap(&attr.attr.wakeup_events);
    ByteSwap(&attr.attr.bp_type);
    ByteSwap(&attr.attr.bp_addr);
    ByteSwap(&attr.attr.bp_len);
    ByteSwap(&attr.attr.branch_sample_type);
  }

  size_t num_ids = (attr_event.header.size - sizeof(attr_event.header) -
                    sizeof(attr_event.attr)) / sizeof(attr_event.id[0]);
  for (size_t i = 0; i < num_ids; ++i) {
    attr.ids.push_back(attr_event.id[i]);
    if (is_cross_endian_)
      ByteSwap(&attr.ids[i]);
  }
  attrs_.push_back(attr);
  // Assign sample type if it hasn't been assigned, otherwise make sure all
  // subsequent attributes have the same sample type bits set.
  if (sample_type_ == 0)
    sample_type_ = attr.attr.sample_type;
  else
    CHECK_EQ(sample_type_, attr.attr.sample_type);
  return true;
}

bool PerfReader::ReadEventTypeEventBlock(
    const struct event_type_event& event_type_event) {
  event_types_.push_back(event_type_event.event_type);
  if (is_cross_endian_)
    ByteSwap(&event_types_.back().event_id);
  return true;
}

bool PerfReader::ReadEventDescEventBlock(
    const struct event_desc_event& desc_event) {
  uint32_t num_events = desc_event.num_events;
  uint32_t event_header_size = desc_event.event_header_size;
  if (is_cross_endian_) {
    ByteSwap(&num_events);
    ByteSwap(&event_header_size);
  }
  size_t offset = 0;
  for (size_t i = 0; i < num_events; ++i) {
    PerfFileAttr attr;
    memcpy(&attr.attr,
           &desc_event.more_data[offset],
           event_header_size);
    offset += event_header_size;
    union {
      const struct {
        uint32 num_ids;
        uint32 name_string_length;
        char name[];
      } *more_data;
      const void* ptr;
    };
    ptr = &desc_event.more_data[offset];
    struct perf_trace_event_type event_type;
    event_type.event_id = i;
    strcpy(event_type.name, more_data->name);
    event_types_.push_back(event_type);
  }
  return true;
}

bool PerfReader::ReadPerfEventBlock(const event_t& event) {
  const perf_event_header& pe_header = event.header;

  if (pe_header.size > sizeof(event_t)) {
    LOG(INFO) << "Data size: " << pe_header.size << " sizeof(event_t): "
              << sizeof(event_t);
    return false;
  }

  PerfEventAndSampleInfo event_and_sample;
  event_and_sample.event = event;
  if (!ReadPerfSampleInfo(event,
                          sample_type_,
                          is_cross_endian_,
                          &event_and_sample.sample_info)) {
    return false;
  }

  if (is_cross_endian_) {
    event_t& event = event_and_sample.event;
    switch (event.header.type) {
    case PERF_RECORD_SAMPLE:
      ByteSwap(&event.ip.ip);
      ByteSwap(&event.ip.pid);
      ByteSwap(&event.ip.tid);
      break;
    case PERF_RECORD_MMAP:
      ByteSwap(&event.mmap.pid);
      ByteSwap(&event.mmap.tid);
      ByteSwap(&event.mmap.start);
      ByteSwap(&event.mmap.len);
      ByteSwap(&event.mmap.pgoff);
      break;
    case PERF_RECORD_FORK:
    case PERF_RECORD_EXIT:
      ByteSwap(&event.fork.pid);
      ByteSwap(&event.fork.tid);
      ByteSwap(&event.fork.ppid);
      ByteSwap(&event.fork.ptid);
      break;
    case PERF_RECORD_COMM:
      ByteSwap(&event.comm.pid);
      ByteSwap(&event.comm.tid);
      break;
    case PERF_RECORD_LOST:
    case PERF_RECORD_THROTTLE:
    case PERF_RECORD_UNTHROTTLE:
    case PERF_RECORD_READ:
    case PERF_RECORD_MAX:
      break;
    default:
      LOG(FATAL) << "Unknown event type: " << event.header.type;
    }
  }

  events_.push_back(event_and_sample);

  return true;
}

}  // namespace quipper
