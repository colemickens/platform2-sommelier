// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>

#include "base/logging.h"

#include "perf_serializer.h"
#include "perf_reader.h"
#include "utils.h"

PerfSerializer::PerfSerializer() {
}

PerfSerializer::~PerfSerializer() {
}

void PerfSerializer::SerializeEventHeader(
    const perf_event_header* header,
    PerfDataProto_EventHeader* header_proto) const {
  header_proto->set_type(header->type);
  header_proto->set_misc(header->misc);
  header_proto->set_size(header->size);
}

void PerfSerializer::DeserializeEventHeader(
    perf_event_header* header,
    const PerfDataProto_EventHeader* header_proto) const {
  header->type = header_proto->type();
  header->misc = header_proto->misc();
  header->size = header_proto->size();
}

void PerfSerializer::SerializeRecordSample(
    const event_t* event,
    PerfDataProto_SampleEvent* sample) const {
  union {
    u64 val64;
    u32 val32[2];
  } u;
  const u64* array = event->sample.array;
  if (type_ & PERF_SAMPLE_IP) {
    sample->set_ip(event->ip.ip);
    array++;
  }
  if (type_ & PERF_SAMPLE_TID) {
    u.val64 = *array;
    sample->set_pid(u.val32[0]);
    sample->set_tid(u.val32[1]);
    array++;
  }
  if (type_ & PERF_SAMPLE_TIME) {
    sample->set_time(*array);
    array++;
  }
  if (type_ & PERF_SAMPLE_ADDR) {
    sample->set_addr(*array);
    array++;
  }
  if (type_ & PERF_SAMPLE_ID) {
    sample->set_id(*array);
    array++;
  }
  if (type_ & PERF_SAMPLE_STREAM_ID) {
    sample->set_stream_id(*array);
    array++;
  }
  if (type_ & PERF_SAMPLE_CPU) {
    u.val64 = *array;
    sample->set_cpu(u.val32[0]);
    // TODO (sque): The upper uint32 of PERF_SAMPLE_CPU is listed as "res" in
    // perf_event.h in the kernel.  However, it doesn't appear to be used in the
    // perf code.  For now, make sure it is unused.  If that changes, both this
    // code and the corresponding block in DeserializeRecordSample() should be
    // updated accordingly.
    CHECK(u.val32[1] == 0);
    array++;
  }
  if (type_ & PERF_SAMPLE_PERIOD) {
    sample->set_period(*array);
    array++;
  }
}

void PerfSerializer::DeserializeRecordSample(
    event_t* event,
    const PerfDataProto_SampleEvent* sample) const {
  union {
    u64 val64;
    u32 val32[2];
  } u;
  u64* array = event->sample.array;
  if (sample->has_ip()) {
    event->ip.ip = sample->ip();
    array++;
  }
  if (sample->has_tid()) {
    u.val32[0] = sample->pid();
    u.val32[1] = sample->tid();
    *array = u.val64;
    array++;
  }
  if (sample->has_time()) {
    *array = sample->time();
    array++;
  }
  if (sample->has_addr()) {
    *array = sample->addr();
    array++;
  }
  if (sample->has_id()) {
    *array = sample->id();
    array++;
  }
  if (sample->has_stream_id()) {
    *array = sample->stream_id();
    array++;
  }
  if (sample->has_cpu()) {
    u.val32[0] = sample->cpu();
    u.val32[1] = 0;
    *array = u.val64;
    array++;
  }
  if (sample->has_period()) {
    *array = sample->period();
    array++;
  }
}

void PerfSerializer::SerializeCommSample(
      const event_t* event,
      PerfDataProto_CommEvent* sample) const {
  sample->set_pid(event->comm.pid);
  sample->set_tid(event->comm.tid);
  sample->set_comm(event->comm.comm);
  sample->set_comm_md5_prefix(Md5Prefix(event->comm.comm));

  const u64* array =
      reinterpret_cast<const u64*>(reinterpret_cast<uint64>(event) +
                                   sizeof(event->comm));

  // Comm event data contains additional time field.
  if (type_ & PERF_SAMPLE_TIME) {
    sample->set_sample_time(*array);
    array++;
  }
}

void PerfSerializer::DeserializeCommSample(
    event_t* event,
    const PerfDataProto_CommEvent* sample) const {
  event->comm.pid = sample->pid();
  event->comm.tid = sample->tid();
  snprintf(event->comm.comm,
           sizeof(event->comm.comm),
           "%s",
           sample->comm().c_str());

  u64* array =
      reinterpret_cast<u64*>(reinterpret_cast<uint64>(event) +
                             sizeof(event->comm));
  if (sample->has_sample_time()) {
    *array = sample->sample_time();
    array++;
  }
}

void PerfSerializer::SerializeMMapSample(
    const event_t* event,
    PerfDataProto_MMapEvent* sample) const {
  sample->set_pid(event->mmap.pid);
  sample->set_tid(event->mmap.tid);
  sample->set_start(event->mmap.start);
  sample->set_len(event->mmap.len);
  sample->set_pgoff(event->mmap.pgoff);
  sample->set_filename(event->mmap.filename);
  sample->set_filename_md5_prefix(Md5Prefix(event->mmap.filename));
}

void PerfSerializer::DeserializeMMapSample(
    event_t* event,
    const PerfDataProto_MMapEvent* sample) const {
  event->mmap.pid = sample->pid();
  event->mmap.tid = sample->tid();
  event->mmap.start = sample->start();
  event->mmap.len = sample->len();
  event->mmap.pgoff = sample->pgoff();
  snprintf(event->mmap.filename,
           PATH_MAX,
           "%s",
           sample->filename().c_str());
}

void PerfSerializer::SerializeForkSample(
    const event_t* event,
    PerfDataProto_ForkEvent* sample) const {
  sample->set_pid(event->fork.pid);
  sample->set_ppid(event->fork.ppid);
  sample->set_tid(event->fork.tid);
  sample->set_ptid(event->fork.ppid);
  sample->set_time(event->fork.time);

  union {
    u64 val64;
    u32 val32[2];
  } u;
  const u64* array =
      reinterpret_cast<const u64*>(reinterpret_cast<uint64>(event) +
                                   sizeof(event->fork));

  // Fork event data contains additional pid/tid, time, id, and cpu fields.
  // TODO(sque): since this follows the same format as SerializeRecordSample(),
  // put it in a common function.  Do the same for DeserializeForkSample().
  if (type_ & PERF_SAMPLE_TID) {
    u.val64 = *array;
    sample->set_sample_pid(u.val32[0]);
    sample->set_sample_tid(u.val32[1]);
    array++;
  }
  if (type_ & PERF_SAMPLE_TIME) {
    sample->set_sample_time(*array);
    array++;
  }
  if (type_ & PERF_SAMPLE_ADDR) {
    array++;
  }
  if (type_ & PERF_SAMPLE_ID) {
    sample->set_sample_id(*array);
    array++;
  }
  if (type_ & PERF_SAMPLE_STREAM_ID) {
    array++;
  }
  if (type_ & PERF_SAMPLE_CPU) {
    u.val64 = *array;
    sample->set_sample_cpu(u.val32[0]);
    array++;
  }
}

void PerfSerializer::DeserializeForkSample(
    event_t* event,
    const PerfDataProto_ForkEvent* sample) const {
  event->fork.pid = sample->pid();
  event->fork.ppid = sample->ppid();
  event->fork.tid = sample->tid();
  event->fork.ptid = sample->ptid();
  event->fork.time = sample->time();

  union {
    u64 val64;
    u32 val32[2];
  } u;
  u64* array =
      reinterpret_cast<u64*>(reinterpret_cast<uint64>(event) +
                             sizeof(event->fork));
  if (sample->has_sample_tid()) {
    u.val32[0] = sample->sample_pid();
    u.val32[1] = sample->sample_tid();
    *array = u.val64;
    array++;
  }
  if (sample->has_sample_time()) {
    *array = sample->sample_time();
    array++;
  }
  if (type_ & PERF_SAMPLE_ADDR) {
    array++;
  }
  if (sample->has_sample_id()) {
    *array = sample->sample_id();
    array++;
  }
  if (type_ & PERF_SAMPLE_STREAM_ID) {
    array++;
  }
  if (sample->has_sample_cpu()) {
    u.val32[0] = sample->sample_cpu();
    u.val32[1] = 0;
    *array = u.val64;
    array++;
  }
}

void PerfSerializer::DeserializeEvent(
    event_t* event,
    const PerfDataProto_PerfEvent* event_proto) const {
  DeserializeEventHeader(&event->header, &event_proto->header());
  switch (event_proto->header().type()) {
    case PERF_RECORD_SAMPLE: {
      DeserializeRecordSample(event, &event_proto->sample_event());
      break;
    }
    case PERF_RECORD_MMAP: {
      DeserializeMMapSample(event, &event_proto->mmap_event());
      break;
    }
    case PERF_RECORD_COMM: {
      DeserializeCommSample(event, &event_proto->comm_event());
      break;
    }
    case PERF_RECORD_EXIT:
    case PERF_RECORD_FORK: {
      DeserializeForkSample(event, &event_proto->fork_event());
      break;
    }
    case PERF_RECORD_LOST:
    case PERF_RECORD_THROTTLE:
    case PERF_RECORD_UNTHROTTLE:
    case PERF_RECORD_READ:
    case PERF_RECORD_MAX:
    default:
      break;
  }
}

void PerfSerializer::SerializeEvent(
    const event_t* event,
    PerfDataProto_PerfEvent* event_proto) const {
  SerializeEventHeader(&event->header, event_proto->mutable_header());
  switch (event->header.type) {
    case PERF_RECORD_SAMPLE: {
      PerfDataProto_SampleEvent* sample_proto =
          event_proto->mutable_sample_event();
      SerializeRecordSample(event, sample_proto);
      break;
    }
    case PERF_RECORD_MMAP: {
      PerfDataProto_MMapEvent* mmap_proto =
          event_proto->mutable_mmap_event();
      SerializeMMapSample(event, mmap_proto);
      break;
    }
    case PERF_RECORD_COMM: {
      PerfDataProto_CommEvent* comm_proto =
          event_proto->mutable_comm_event();
      SerializeCommSample(event, comm_proto);
      break;
    }
    case PERF_RECORD_EXIT:
    case PERF_RECORD_FORK: {
      PerfDataProto_ForkEvent* fork_proto =
          event_proto->mutable_fork_event();
      SerializeForkSample(event, fork_proto);
      break;
    }
    case PERF_RECORD_LOST:
    case PERF_RECORD_THROTTLE:
    case PERF_RECORD_UNTHROTTLE:
    case PERF_RECORD_READ:
    case PERF_RECORD_MAX:
    default:
      break;
  }
}

void PerfSerializer::DeserializePerfEventAttr(
    perf_event_attr* perf_event_attr,
    const PerfDataProto_PerfEventAttr* perf_event_attr_proto) {
  memset(perf_event_attr, 0, sizeof(*perf_event_attr));
#define S(x) perf_event_attr->x = perf_event_attr_proto->x()
  S(type);
  S(size);
  S(config);
  if (perf_event_attr->freq)
    S(sample_freq);
  else
    S(sample_period);
  S(sample_type);
  S(read_format);
  S(disabled);
  S(inherit);
  S(pinned);
  S(exclusive);
  S(exclude_user);
  S(exclude_kernel);
  S(exclude_hv);
  S(exclude_idle);
  S(mmap);
  S(comm);
  S(freq);
  S(inherit_stat);
  S(enable_on_exec);
  S(task);
  S(watermark);
  S(precise_ip);
  S(mmap_data);
  S(sample_id_all);
  S(exclude_host);
  S(exclude_guest);
  if (perf_event_attr->watermark)
    S(wakeup_watermark);
  else
    S(wakeup_events);
  S(bp_type);
  S(bp_len);
  S(branch_sample_type);
#undef S
  if (!type_set_) {
    type_set_ = true;
    type_ = perf_event_attr->sample_type;
  }
}

void PerfSerializer::SerializePerfEventAttr(
    const perf_event_attr* perf_event_attr,
    PerfDataProto_PerfEventAttr* perf_event_attr_proto) {
#define S(x) perf_event_attr_proto->set_##x(perf_event_attr->x)
  S(type);
  S(size);
  S(config);
  if (perf_event_attr_proto->freq())
    S(sample_freq);
  else
    S(sample_period);
  S(sample_type);
  S(read_format);
  S(disabled);
  S(inherit);
  S(pinned);
  S(exclusive);
  S(exclude_user);
  S(exclude_kernel);
  S(exclude_hv);
  S(exclude_idle);
  S(mmap);
  S(comm);
  S(freq);
  S(inherit_stat);
  S(enable_on_exec);
  S(task);
  S(watermark);
  S(precise_ip);
  S(mmap_data);
  S(sample_id_all);
  S(exclude_host);
  S(exclude_guest);
  if (perf_event_attr_proto->watermark())
    S(wakeup_watermark);
  else
    S(wakeup_events);
  S(bp_type);
  S(bp_len);
  S(branch_sample_type);
#undef S
  if (!type_set_) {
    type_set_ = true;
    type_ = perf_event_attr->sample_type;
  }
}

void PerfSerializer::SerializePerfFileAttr(
    const PerfFileAttr* perf_file_attr,
    PerfDataProto_PerfFileAttr* perf_file_attr_proto) {
  SerializePerfEventAttr(&perf_file_attr->attr,
                         perf_file_attr_proto->mutable_attr());
  for (size_t i = 0; i < perf_file_attr->ids.size(); i++ )
    perf_file_attr_proto->add_ids(perf_file_attr->ids[i]);
}

void PerfSerializer::DeserializePerfFileAttr(
    PerfFileAttr* perf_file_attr,
    const PerfDataProto_PerfFileAttr* perf_file_attr_proto) {
  DeserializePerfEventAttr(&perf_file_attr->attr,
                           &perf_file_attr_proto->attr());
  for (int i = 0; i < perf_file_attr_proto->ids_size(); i++ )
    perf_file_attr->ids.push_back(perf_file_attr_proto->ids(i));
}

bool PerfSerializer::SerializeReader(const PerfReader& perf_reader,
                                     PerfDataProto* perf_data_proto) {
  type_set_ = false;
  SerializePerfFileAttrs(
      &perf_reader.attrs(), perf_data_proto->mutable_file_attrs());
  SerializeEvents(&perf_reader.events(),
                  perf_data_proto->mutable_events());
  return true;
}

bool PerfSerializer::Serialize(const char* filename,
                               PerfDataProto* perf_data_proto) {
  PerfReader perf_reader;

  if (!perf_reader.ReadFile(filename))
    return false;

  return SerializeReader(perf_reader, perf_data_proto);
}

bool PerfSerializer::DeserializeReader(const PerfDataProto& perf_data_proto,
                                       PerfReader* perf_reader) {
  type_set_ = false;
  DeserializePerfFileAttrs(
      perf_reader->mutable_attrs(), &perf_data_proto.file_attrs());
  DeserializeEvents(perf_reader->mutable_events(),
                    &perf_data_proto.events());
  return true;
}

bool PerfSerializer::Deserialize(const char* filename,
                                 const PerfDataProto& perf_data_proto) {
  PerfReader perf_reader;
  DeserializeReader(perf_data_proto, &perf_reader);
  perf_reader.WriteFile(filename);
  return true;
}
