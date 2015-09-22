// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromiumos-wide-profiling/sample_info_reader.h"

#include <string.h>

#include "base/logging.h"
#include "chromiumos-wide-profiling/buffer_reader.h"
#include "chromiumos-wide-profiling/buffer_writer.h"
#include "chromiumos-wide-profiling/kernel/perf_internals.h"

namespace quipper {

namespace {

bool IsSupportedEventType(uint32_t type) {
  switch (type) {
  case PERF_RECORD_SAMPLE:
  case PERF_RECORD_MMAP:
  case PERF_RECORD_MMAP2:
  case PERF_RECORD_FORK:
  case PERF_RECORD_EXIT:
  case PERF_RECORD_COMM:
  case PERF_RECORD_LOST:
  case PERF_RECORD_THROTTLE:
  case PERF_RECORD_UNTHROTTLE:
    return true;
  case PERF_RECORD_READ:
  case PERF_RECORD_MAX:
    return false;
  default:
    LOG(FATAL) << "Unknown event type " << type;
    return false;
  }
}

// Read read info from perf data.  Corresponds to sample format type
// PERF_SAMPLE_READ.
void ReadReadInfo(DataReader* reader,
                  uint64_t read_format,
                  struct perf_sample* sample) {
  if (read_format & PERF_FORMAT_TOTAL_TIME_ENABLED)
    reader->ReadUint64(&sample->read.time_enabled);
  if (read_format & PERF_FORMAT_TOTAL_TIME_RUNNING)
    reader->ReadUint64(&sample->read.time_running);
  if (read_format & PERF_FORMAT_ID)
    reader->ReadUint64(&sample->read.id);
}

// Read call chain info from perf data.  Corresponds to sample format type
// PERF_SAMPLE_CALLCHAIN.
void ReadCallchain(DataReader* reader, struct perf_sample* sample) {
  // Make sure there is no existing allocated memory in |sample->callchain|.
  CHECK_EQ(static_cast<void*>(NULL), sample->callchain);

  // The callgraph data consists of a uint64_t value |nr| followed by |nr|
  // addresses.
  uint64_t callchain_size = 0;
  reader->ReadUint64(&callchain_size);

  struct ip_callchain* callchain =
      reinterpret_cast<struct ip_callchain*>(new uint64_t[callchain_size + 1]);
  callchain->nr = callchain_size;

  for (size_t i = 0; i < callchain_size; ++i)
    reader->ReadUint64(&callchain->ips[i]);

  sample->callchain = callchain;
}

// Read raw info from perf data.  Corresponds to sample format type
// PERF_SAMPLE_RAW.
void ReadRawData(DataReader* reader, struct perf_sample* sample) {
  // Save the original read offset.
  size_t reader_offset = reader->Tell();

  reader->ReadUint32(&sample->raw_size);

  // Allocate space for and read the raw data bytes.
  sample->raw_data = new uint8_t[sample->raw_size];
  reader->ReadData(sample->raw_size, sample->raw_data);

  // Determine the bytes that were read, and align to the next 64 bits.
  reader_offset += AlignSize(sizeof(sample->raw_size) + sample->raw_size,
                             sizeof(uint64_t));
  reader->SeekSet(reader_offset);
}

// Read call chain info from perf data.  Corresponds to sample format type
// PERF_SAMPLE_CALLCHAIN.
void ReadBranchStack(DataReader* reader, struct perf_sample* sample) {
  // Make sure there is no existing allocated memory in
  // |sample->branch_stack|.
  CHECK_EQ(static_cast<void*>(NULL), sample->branch_stack);

  // The branch stack data consists of a uint64_t value |nr| followed by |nr|
  // branch_entry structs.
  uint64_t branch_stack_size = 0;
  reader->ReadUint64(&branch_stack_size);

  struct branch_stack* branch_stack =
      reinterpret_cast<struct branch_stack*>(
          new uint8_t[sizeof(uint64_t) +
                      branch_stack_size * sizeof(struct branch_entry)]);
  branch_stack->nr = branch_stack_size;
  for (size_t i = 0; i < branch_stack_size; ++i) {
    reader->ReadUint64(&branch_stack->entries[i].from);
    reader->ReadUint64(&branch_stack->entries[i].to);
    reader->ReadData(sizeof(branch_stack->entries[i].flags),
                     &branch_stack->entries[i].flags);
    if (reader->is_cross_endian()) {
      // TODO(sque): swap bytes of flags.
      LOG(ERROR) << "Byte swapping of branch stack flags is not yet supported.";
    }
  }
  sample->branch_stack = branch_stack;
}

size_t ReadPerfSampleFromData(const perf_event_type event_type,
                              const void* data,
                              const size_t data_size,
                              const uint64_t sample_fields,
                              const uint64_t read_format,
                              bool swap_bytes,
                              struct perf_sample* sample) {
  BufferReader reader(data, data_size);
  reader.set_is_cross_endian(swap_bytes);

  // See structure for PERF_RECORD_SAMPLE in kernel/perf_event.h
  // and compare sample_id when sample_id_all is set.

  // NB: For sample_id, sample_fields has already been masked to the set
  // of fields in that struct by GetSampleFieldsForEventType. That set
  // of fields is mostly in the same order as PERF_RECORD_SAMPLE, with
  // the exception of PERF_SAMPLE_IDENTIFIER.

  // PERF_SAMPLE_IDENTIFIER is in a different location depending on
  // if this is a SAMPLE event or the sample_id of another event.
  if (event_type == PERF_RECORD_SAMPLE) {
    // { u64                   id;       } && PERF_SAMPLE_IDENTIFIER
    if (sample_fields & PERF_SAMPLE_IDENTIFIER) {
      reader.ReadUint64(&sample->id);
    }
  }

  // { u64                   ip;       } && PERF_SAMPLE_IP
  if (sample_fields & PERF_SAMPLE_IP) {
    reader.ReadUint64(&sample->ip);
  }

  // { u32                   pid, tid; } && PERF_SAMPLE_TID
  if (sample_fields & PERF_SAMPLE_TID) {
    reader.ReadUint32(&sample->pid);
    reader.ReadUint32(&sample->tid);
  }

  // { u64                   time;     } && PERF_SAMPLE_TIME
  if (sample_fields & PERF_SAMPLE_TIME) {
    reader.ReadUint64(&sample->time);
  }

  // { u64                   addr;     } && PERF_SAMPLE_ADDR
  if (sample_fields & PERF_SAMPLE_ADDR) {
    reader.ReadUint64(&sample->addr);
  }

  // { u64                   id;       } && PERF_SAMPLE_ID
  if (sample_fields & PERF_SAMPLE_ID) {
    reader.ReadUint64(&sample->id);
  }

  // { u64                   stream_id;} && PERF_SAMPLE_STREAM_ID
  if (sample_fields & PERF_SAMPLE_STREAM_ID) {
    reader.ReadUint64(&sample->stream_id);
  }

  // { u32                   cpu, res; } && PERF_SAMPLE_CPU
  if (sample_fields & PERF_SAMPLE_CPU) {
    reader.ReadUint32(&sample->cpu);

    // The PERF_SAMPLE_CPU format bit specifies 64-bits of data, but the actual
    // CPU number is really only 32 bits. There is an extra 32-bit word of
    // reserved padding, as the whole field is aligned to 64 bits.

    // reader.ReadUint32(&sample->res);  // reserved
    u32 reserved;
    reader.ReadUint32(&reserved);
  }

  // This is the location of PERF_SAMPLE_IDENTIFIER in struct sample_id.
  if (event_type != PERF_RECORD_SAMPLE) {
    // { u64                   id;       } && PERF_SAMPLE_IDENTIFIER
    if (sample_fields & PERF_SAMPLE_IDENTIFIER) {
      reader.ReadUint64(&sample->id);
    }
  }

  //
  // The remaining fields are only in PERF_RECORD_SAMPLE
  //

  // { u64                   period;   } && PERF_SAMPLE_PERIOD
  if (sample_fields & PERF_SAMPLE_PERIOD) {
    reader.ReadUint64(&sample->period);
  }

  // { struct read_format    values;   } && PERF_SAMPLE_READ
  if (sample_fields & PERF_SAMPLE_READ) {
    // TODO(cwp-team): support grouped read info.
    if (read_format & PERF_FORMAT_GROUP)
      return reader.Tell();
    ReadReadInfo(&reader, read_format, sample);
  }

  // { u64                   nr,
  //   u64                   ips[nr];  } && PERF_SAMPLE_CALLCHAIN
  if (sample_fields & PERF_SAMPLE_CALLCHAIN) {
    ReadCallchain(&reader, sample);
  }

  // { u32                   size;
  //   char                  data[size];}&& PERF_SAMPLE_RAW
  if (sample_fields & PERF_SAMPLE_RAW) {
    ReadRawData(&reader, sample);
  }

  // { u64                   nr;
  //   { u64 from, to, flags } lbr[nr];} && PERF_SAMPLE_BRANCH_STACK
  if (sample_fields & PERF_SAMPLE_BRANCH_STACK) {
    ReadBranchStack(&reader, sample);
  }

  static const u64 kUnimplementedSampleFields =
      PERF_SAMPLE_REGS_USER  |
      PERF_SAMPLE_STACK_USER |
      PERF_SAMPLE_WEIGHT     |
      PERF_SAMPLE_DATA_SRC   |
      PERF_SAMPLE_TRANSACTION;

  if (sample_fields & kUnimplementedSampleFields) {
    LOG(WARNING) << "Unimplemented sample fields 0x"
                 << std::hex << (sample_fields & kUnimplementedSampleFields);
  }

  if (sample_fields & ~(PERF_SAMPLE_MAX-1)) {
    LOG(WARNING) << "Unrecognized sample fields 0x"
                 << std::hex << (sample_fields & ~(PERF_SAMPLE_MAX-1));
  }

  return reader.Tell();
}

size_t WritePerfSampleToData(const perf_event_type event_type,
                             const struct perf_sample& sample,
                             const uint64_t sample_fields,
                             const uint64_t read_format,
                             uint64_t* array) {
  const uint64_t* initial_array_ptr = array;

  union {
    uint32_t val32[sizeof(uint64_t) / sizeof(uint32_t)];
    uint64_t val64;
  };

  // See notes at the top of ReadPerfSampleFromData regarding the structure
  // of PERF_RECORD_SAMPLE, sample_id, and PERF_SAMPLE_IDENTIFIER, as they
  // all apply here as well.

  // PERF_SAMPLE_IDENTIFIER is in a different location depending on
  // if this is a SAMPLE event or the sample_id of another event.
  if (event_type == PERF_RECORD_SAMPLE) {
    // { u64                   id;       } && PERF_SAMPLE_IDENTIFIER
    if (sample_fields & PERF_SAMPLE_IDENTIFIER) {
      *array++ = sample.id;
    }
  }

  // { u64                   ip;       } && PERF_SAMPLE_IP
  if (sample_fields & PERF_SAMPLE_IP) {
    *array++ = sample.ip;
  }

  // { u32                   pid, tid; } && PERF_SAMPLE_TID
  if (sample_fields & PERF_SAMPLE_TID) {
    val32[0] = sample.pid;
    val32[1] = sample.tid;
    *array++ = val64;
  }

  // { u64                   time;     } && PERF_SAMPLE_TIME
  if (sample_fields & PERF_SAMPLE_TIME) {
    *array++ = sample.time;
  }

  // { u64                   addr;     } && PERF_SAMPLE_ADDR
  if (sample_fields & PERF_SAMPLE_ADDR) {
    *array++ = sample.addr;
  }

  // { u64                   id;       } && PERF_SAMPLE_ID
  if (sample_fields & PERF_SAMPLE_ID) {
    *array++ = sample.id;
  }

  // { u64                   stream_id;} && PERF_SAMPLE_STREAM_ID
  if (sample_fields & PERF_SAMPLE_STREAM_ID) {
    *array++ = sample.stream_id;
  }

  // { u32                   cpu, res; } && PERF_SAMPLE_CPU
  if (sample_fields & PERF_SAMPLE_CPU) {
    val32[0] = sample.cpu;
    // val32[1] = sample.res;  // reserved
    val32[1] = 0;
    *array++ = val64;
  }

  // This is the location of PERF_SAMPLE_IDENTIFIER in struct sample_id.
  if (event_type != PERF_RECORD_SAMPLE) {
    // { u64                   id;       } && PERF_SAMPLE_IDENTIFIER
    if (sample_fields & PERF_SAMPLE_IDENTIFIER) {
      *array++ = sample.id;
    }
  }

  //
  // The remaining fields are only in PERF_RECORD_SAMPLE
  //

  // { u64                   period;   } && PERF_SAMPLE_PERIOD
  if (sample_fields & PERF_SAMPLE_PERIOD) {
    *array++ = sample.period;
  }

  // { struct read_format    values;   } && PERF_SAMPLE_READ
  if (sample_fields & PERF_SAMPLE_READ) {
    // TODO(cwp-team): support grouped read info.
    if (read_format & PERF_FORMAT_GROUP)
      return 0;
    if (read_format & PERF_FORMAT_TOTAL_TIME_ENABLED)
      *array++ = sample.read.time_enabled;
    if (read_format & PERF_FORMAT_TOTAL_TIME_RUNNING)
      *array++ = sample.read.time_running;
    if (read_format & PERF_FORMAT_ID)
      *array++ = sample.read.id;
  }

  // { u64                   nr,
  //   u64                   ips[nr];  } && PERF_SAMPLE_CALLCHAIN
  if (sample_fields & PERF_SAMPLE_CALLCHAIN) {
    if (!sample.callchain) {
      LOG(ERROR) << "Expecting callchain data, but none was found.";
    } else {
      *array++ = sample.callchain->nr;
      for (size_t i = 0; i < sample.callchain->nr; ++i)
        *array++ = sample.callchain->ips[i];
    }
  }

  // { u32                   size;
  //   char                  data[size];}&& PERF_SAMPLE_RAW
  if (sample_fields & PERF_SAMPLE_RAW) {
    uint32_t* ptr = reinterpret_cast<uint32_t*>(array);
    *ptr++ = sample.raw_size;
    memcpy(ptr, sample.raw_data, sample.raw_size);

    // Update the data read pointer after aligning to the next 64 bytes.
    int num_bytes = AlignSize(sizeof(sample.raw_size) + sample.raw_size,
                              sizeof(uint64_t));
    array += num_bytes / sizeof(uint64_t);
  }

  // { u64                   nr;
  //   { u64 from, to, flags } lbr[nr];} && PERF_SAMPLE_BRANCH_STACK
  if (sample_fields & PERF_SAMPLE_BRANCH_STACK) {
    if (!sample.branch_stack) {
      LOG(ERROR) << "Expecting branch stack data, but none was found.";
    } else {
      *array++ = sample.branch_stack->nr;
      for (size_t i = 0; i < sample.branch_stack->nr; ++i) {
        *array++ = sample.branch_stack->entries[i].from;
        *array++ = sample.branch_stack->entries[i].to;
        memcpy(array++, &sample.branch_stack->entries[i].flags,
               sizeof(uint64_t));
      }
    }
  }

  return (array - initial_array_ptr) * sizeof(uint64_t);
}

}  // namespace

bool SampleInfoReader::ReadPerfSampleInfo(const event_t& event,
                                          struct perf_sample* sample) const {
  CHECK(sample);

  if (!IsSupportedEventType(event.header.type)) {
    LOG(ERROR) << "Unsupported event type " << event.header.type;
    return false;
  }

  uint64_t offset = GetPerfSampleDataOffset(event);
  size_t sample_info_size = event.header.size - offset;
  size_t size_read = ReadPerfSampleFromData(
      static_cast<perf_event_type>(event.header.type),
      reinterpret_cast<const uint8_t*>(&event) + offset,
      sample_info_size,
      GetSampleFieldsForEventType(event.header.type, sample_type_),
      read_format_,
      read_cross_endian_,
      sample);

  if (size_read != sample_info_size) {
    LOG(ERROR) << "Read " << size_read << " bytes, expected "
               << sample_info_size << " bytes.";
  }

  return (size_read == sample_info_size);
}

bool SampleInfoReader::WritePerfSampleInfo(const perf_sample& sample,
                                           event_t* event) const {
  CHECK(event);

  if (!IsSupportedEventType(event->header.type)) {
    LOG(ERROR) << "Unsupported event type " << event->header.type;
    return false;
  }

  uint64_t offset = GetPerfSampleDataOffset(*event);
  size_t expected_size = event->header.size - offset;
  memset(reinterpret_cast<uint8_t*>(event) + offset, 0, expected_size);
  size_t size_written = WritePerfSampleToData(
      static_cast<perf_event_type>(event->header.type),
      sample,
      GetSampleFieldsForEventType(event->header.type, sample_type_),
      read_format_,
      reinterpret_cast<uint64_t*>(event) + offset / sizeof(uint64_t));
  if (size_written != expected_size) {
    LOG(ERROR) << "Wrote " << size_written << " bytes, expected "
               << expected_size << " bytes.";
  }

  return (size_written == expected_size);
}

// static
uint64_t SampleInfoReader::GetSampleFieldsForEventType(uint32_t event_type,
                                                       uint64_t sample_type) {
  uint64_t mask = UINT64_MAX;
  switch (event_type) {
  case PERF_RECORD_MMAP:
  case PERF_RECORD_LOST:
  case PERF_RECORD_COMM:
  case PERF_RECORD_EXIT:
  case PERF_RECORD_THROTTLE:
  case PERF_RECORD_UNTHROTTLE:
  case PERF_RECORD_FORK:
  case PERF_RECORD_READ:
  case PERF_RECORD_MMAP2:
    // See perf_event.h "struct" sample_id and sample_id_all.
    mask = PERF_SAMPLE_TID | PERF_SAMPLE_TIME | PERF_SAMPLE_ID |
           PERF_SAMPLE_STREAM_ID | PERF_SAMPLE_CPU | PERF_SAMPLE_IDENTIFIER;
    break;
  case PERF_RECORD_SAMPLE:
    break;
  default:
    LOG(FATAL) << "Unknown event type " << event_type;
  }
  return sample_type & mask;
}

// static
uint64_t SampleInfoReader::GetPerfSampleDataOffset(const event_t& event) {
  uint64_t offset = UINT64_MAX;
  switch (event.header.type) {
  case PERF_RECORD_SAMPLE:
    offset = offsetof(event_t, sample.array);
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
    offset = sizeof(event.lost);
    break;
  case PERF_RECORD_THROTTLE:
  case PERF_RECORD_UNTHROTTLE:
    offset = sizeof(event.throttle);
    break;
  case PERF_RECORD_READ:
    offset = sizeof(event.read);
    break;
  case PERF_RECORD_MMAP2:
    offset = sizeof(event.mmap2) - sizeof(event.mmap2.filename) +
             GetUint64AlignedStringLength(event.mmap2.filename);
    break;
  default:
    LOG(FATAL) << "Unknown event type " << event.header.type;
    break;
  }
  // Make sure the offset was valid
  CHECK_NE(offset, UINT64_MAX);
  CHECK_EQ(offset % sizeof(uint64_t), 0U);
  return offset;
}

}  // namespace quipper
