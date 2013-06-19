// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "perf_serializer.h"

#include <stdio.h>
#include <sys/time.h>

#include "base/logging.h"

#include "quipper_string.h"
#include "utils.h"

namespace quipper {

PerfSerializer::PerfSerializer() {
}

PerfSerializer::~PerfSerializer() {
}

bool PerfSerializer::SerializeFromFile(const string& filename,
                                       PerfDataProto* perf_data_proto) {
  if (!ReadFile(filename))
    return false;
  return Serialize(perf_data_proto);
}

bool PerfSerializer::Serialize(PerfDataProto* perf_data_proto) {
  SerializePerfFileAttrs(attrs_, perf_data_proto->mutable_file_attrs());

  if (!ParseRawEvents())
    return false;

  SerializeEvents(parsed_events_, perf_data_proto->mutable_events());

  perf_data_proto->add_metadata_mask(metadata_mask_);

  SerializeBuildIDs(build_id_events_,
                    perf_data_proto->mutable_build_ids());
  SerializeStringMetadata(string_metadata_,
                          perf_data_proto->mutable_string_metadata());

  // Add a timestamp_sec to the protobuf.
  struct timeval timestamp_sec;
  if (!gettimeofday(&timestamp_sec, NULL))
    perf_data_proto->set_timestamp_sec(timestamp_sec.tv_sec);

  PerfDataProto_PerfEventStats* stats = perf_data_proto->mutable_stats();
  stats->set_num_sample_events(stats_.num_sample_events);
  stats->set_num_mmap_events(stats_.num_mmap_events);
  stats->set_num_fork_events(stats_.num_fork_events);
  stats->set_num_exit_events(stats_.num_exit_events);
  stats->set_did_remap(stats_.did_remap);
  stats->set_num_sample_events_mapped(stats_.num_sample_events_mapped);
  return true;
}

bool PerfSerializer::DeserializeToFile(const PerfDataProto& perf_data_proto,
                                       const string& filename) {
  Deserialize(perf_data_proto);
  return WriteFile(filename);
}

bool PerfSerializer::Deserialize(const PerfDataProto& perf_data_proto) {
  DeserializePerfFileAttrs(perf_data_proto.file_attrs(), &attrs_);

  // Make sure all event types (attrs) have the same sample type.
  for (size_t i = 0; i < attrs_.size(); ++i) {
    CHECK_EQ(attrs_[i].attr.sample_type, attrs_[0].attr.sample_type)
        << "Sample type for attribute #" << i
        << " (" << std::hex << attrs_[i].attr.sample_type << ")"
        << " does not match that of attribute 0"
        << " (" << std::hex << attrs_[0].attr.sample_type << ")";
  }
  CHECK_GT(attrs_.size(), 0U);
  sample_type_ = attrs_[0].attr.sample_type;

  SetRawEventsAndSampleInfos(perf_data_proto.events().size());
  DeserializeEvents(perf_data_proto.events(), &parsed_events_);

  metadata_mask_ = perf_data_proto.metadata_mask(0);

  DeserializeBuildIDs(perf_data_proto.build_ids(),
                      &build_id_events_);
  DeserializeStringMetadata(perf_data_proto.string_metadata(),
                            &string_metadata_);

  SortParsedEvents();
  ProcessEvents();

  memset(&stats_, 0, sizeof(stats_));
  const PerfDataProto_PerfEventStats& stats = perf_data_proto.stats();
  stats_.num_sample_events = stats.num_sample_events();
  stats_.num_mmap_events = stats.num_mmap_events();
  stats_.num_fork_events = stats.num_fork_events();
  stats_.num_exit_events = stats.num_exit_events();
  stats_.did_remap = stats.did_remap();
  stats_.num_sample_events_mapped = stats.num_sample_events_mapped();

  return true;
}

void PerfSerializer::SerializePerfFileAttr(
    const PerfFileAttr& perf_file_attr,
    PerfDataProto_PerfFileAttr* perf_file_attr_proto) const {
  SerializePerfEventAttr(perf_file_attr.attr,
                         perf_file_attr_proto->mutable_attr());
  for (size_t i = 0; i < perf_file_attr.ids.size(); i++ )
    perf_file_attr_proto->add_ids(perf_file_attr.ids[i]);
}

void PerfSerializer::DeserializePerfFileAttr(
    const PerfDataProto_PerfFileAttr& perf_file_attr_proto,
    PerfFileAttr* perf_file_attr) const {
  DeserializePerfEventAttr(perf_file_attr_proto.attr(), &perf_file_attr->attr);
  for (int i = 0; i < perf_file_attr_proto.ids_size(); i++ )
    perf_file_attr->ids.push_back(perf_file_attr_proto.ids(i));
}

void PerfSerializer::SerializePerfEventAttr(
    const perf_event_attr& perf_event_attr,
    PerfDataProto_PerfEventAttr* perf_event_attr_proto) const {
#define S(x) perf_event_attr_proto->set_##x(perf_event_attr.x)
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
}

void PerfSerializer::DeserializePerfEventAttr(
    const PerfDataProto_PerfEventAttr& perf_event_attr_proto,
    perf_event_attr* perf_event_attr) const {
  memset(perf_event_attr, 0, sizeof(*perf_event_attr));
#define S(x) perf_event_attr->x = perf_event_attr_proto.x()
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
}

void PerfSerializer::SerializeEvent(
    const ParsedEvent& event,
    PerfDataProto_PerfEvent* event_proto) const {
  SerializeEventHeader(event.raw_event->header, event_proto->mutable_header());
  switch (event.raw_event->header.type) {
    case PERF_RECORD_SAMPLE:
      SerializeRecordSample(event, event_proto->mutable_sample_event());
      break;
    case PERF_RECORD_MMAP:
      SerializeMMapSample(event, event_proto->mutable_mmap_event());
      break;
    case PERF_RECORD_COMM:
      SerializeCommSample(event, event_proto->mutable_comm_event());
      break;
    case PERF_RECORD_EXIT:
    case PERF_RECORD_FORK:
      SerializeForkSample(event, event_proto->mutable_fork_event());
      break;
    case PERF_RECORD_LOST:
    case PERF_RECORD_THROTTLE:
    case PERF_RECORD_UNTHROTTLE:
    case PERF_RECORD_READ:
    case PERF_RECORD_MAX:
    default:
      break;
  }
}

void PerfSerializer::DeserializeEvent(
    const PerfDataProto_PerfEvent& event_proto,
    ParsedEvent* event) const {
  DeserializeEventHeader(event_proto.header(), &event->raw_event->header);
  switch (event_proto.header().type()) {
    case PERF_RECORD_SAMPLE:
      DeserializeRecordSample(event_proto.sample_event(), event);
      break;
    case PERF_RECORD_MMAP:
      DeserializeMMapSample(event_proto.mmap_event(), event);
      break;
    case PERF_RECORD_COMM:
      DeserializeCommSample(event_proto.comm_event(), event);
      break;
    case PERF_RECORD_EXIT:
    case PERF_RECORD_FORK:
      DeserializeForkSample(event_proto.fork_event(), event);
      break;
    case PERF_RECORD_LOST:
    case PERF_RECORD_THROTTLE:
    case PERF_RECORD_UNTHROTTLE:
    case PERF_RECORD_READ:
    case PERF_RECORD_MAX:
    default:
      break;
  }
}

void PerfSerializer::SerializeEventHeader(
    const perf_event_header& header,
    PerfDataProto_EventHeader* header_proto) const {
  header_proto->set_type(header.type);
  header_proto->set_misc(header.misc);
  header_proto->set_size(header.size);
}

void PerfSerializer::DeserializeEventHeader(
    const PerfDataProto_EventHeader& header_proto,
    perf_event_header* header) const {
  header->type = header_proto.type();
  header->misc = header_proto.misc();
  header->size = header_proto.size();
}

void PerfSerializer::SerializeRecordSample(
    const ParsedEvent& event,
    PerfDataProto_SampleEvent* sample) const {
  const struct perf_sample& perf_sample = *event.sample_info;
  const struct ip_event& ip_event = event.raw_event->ip;

  if (sample_type_ & PERF_SAMPLE_IP)
    sample->set_ip(ip_event.ip);
  if (sample_type_ & PERF_SAMPLE_TID) {
    CHECK_EQ(ip_event.pid, perf_sample.pid);
    CHECK_EQ(ip_event.tid, perf_sample.tid);
    sample->set_pid(ip_event.pid);
    sample->set_tid(ip_event.tid);
  }
  if (sample_type_ & PERF_SAMPLE_TIME)
    sample->set_sample_time_ns(perf_sample.time);
  if (sample_type_ & PERF_SAMPLE_ADDR)
    sample->set_addr(perf_sample.addr);
  if (sample_type_ & PERF_SAMPLE_ID)
    sample->set_id(perf_sample.id);
  if (sample_type_ & PERF_SAMPLE_STREAM_ID)
    sample->set_stream_id(perf_sample.stream_id);
  if (sample_type_ & PERF_SAMPLE_CPU)
    sample->set_cpu(perf_sample.cpu);
  if (sample_type_ & PERF_SAMPLE_PERIOD)
    sample->set_period(perf_sample.period);
  if (sample_type_ & PERF_SAMPLE_CALLCHAIN) {
    for (size_t i = 0; i < perf_sample.callchain->nr; ++i)
      sample->add_callchain(perf_sample.callchain->ips[i]);
  }
  if (sample_type_ & PERF_SAMPLE_BRANCH_STACK) {
    for (size_t i = 0; i < perf_sample.branch_stack->nr; ++i) {
      sample->add_branch_stack();
      const struct branch_entry& entry = perf_sample.branch_stack->entries[i];
      sample->mutable_branch_stack(i)->set_from_ip(entry.from);
      sample->mutable_branch_stack(i)->set_to_ip(entry.to);
      sample->mutable_branch_stack(i)->set_mispredicted(entry.flags.mispred);
    }
  }
}

void PerfSerializer::DeserializeRecordSample(
    const PerfDataProto_SampleEvent& sample,
    ParsedEvent* event) const {
  struct perf_sample& perf_sample = *event->sample_info;
  struct ip_event& ip_event = event->raw_event->ip;
  if (sample.has_ip())
    ip_event.ip = sample.ip();
  if (sample.has_pid()) {
    CHECK(sample.has_tid()) << "Cannot have PID without TID.";
    ip_event.pid = sample.pid();
    ip_event.tid = sample.tid();
    perf_sample.pid = sample.pid();
    perf_sample.tid = sample.tid();
  }
  if (sample.has_sample_time_ns())
    perf_sample.time = sample.sample_time_ns();
  if (sample.has_addr())
    perf_sample.addr = sample.addr();
  if (sample.has_id())
    perf_sample.id = sample.id();
  if (sample.has_stream_id())
    perf_sample.stream_id = sample.stream_id();
  if (sample.has_cpu())
    perf_sample.cpu = sample.cpu();
  if (sample.has_period())
    perf_sample.period = sample.period();
  if (sample.callchain_size() > 0) {
    uint64 callchain_size = sample.callchain_size();
    perf_sample.callchain =
        reinterpret_cast<struct ip_callchain*>(new uint64[callchain_size + 1]);
    perf_sample.callchain->nr = callchain_size;
    for (size_t i = 0; i < callchain_size; ++i)
      perf_sample.callchain->ips[i] = sample.callchain(i);
  }
  if (sample.branch_stack_size() > 0) {
    uint64 branch_stack_size = sample.branch_stack_size();
    perf_sample.branch_stack =
        reinterpret_cast<struct branch_stack*>(
            new uint8[sizeof(uint64) +
                      branch_stack_size * sizeof(struct branch_entry)]);
    perf_sample.branch_stack->nr = branch_stack_size;
    for (size_t i = 0; i < branch_stack_size; ++i) {
      struct branch_entry& entry = perf_sample.branch_stack->entries[i];
      memset(&entry, 0, sizeof(entry));
      entry.from = sample.branch_stack(i).from_ip();
      entry.to = sample.branch_stack(i).to_ip();
      entry.flags.mispred = sample.branch_stack(i).mispredicted();
      entry.flags.predicted = !entry.flags.mispred;
    }
  }
}

void PerfSerializer::SerializeMMapSample(
    const ParsedEvent& event,
    PerfDataProto_MMapEvent* sample) const {
  const struct mmap_event& mmap = event.raw_event->mmap;
  sample->set_pid(mmap.pid);
  sample->set_tid(mmap.tid);
  sample->set_start(mmap.start);
  sample->set_len(mmap.len);
  sample->set_pgoff(mmap.pgoff);
  sample->set_filename(mmap.filename);
  sample->set_filename_md5_prefix(Md5Prefix(mmap.filename));

  SerializeSampleInfo(event, sample->mutable_sample_info());
}

void PerfSerializer::DeserializeMMapSample(
    const PerfDataProto_MMapEvent& sample,
    ParsedEvent* event) const {
  struct mmap_event& mmap = event->raw_event->mmap;
  mmap.pid = sample.pid();
  mmap.tid = sample.tid();
  mmap.start = sample.start();
  mmap.len = sample.len();
  mmap.pgoff = sample.pgoff();
  snprintf(mmap.filename, PATH_MAX, "%s", sample.filename().c_str());

  DeserializeSampleInfo(sample.sample_info(), event);
}

void PerfSerializer::SerializeCommSample(
      const ParsedEvent& event,
      PerfDataProto_CommEvent* sample) const {
  const struct comm_event& comm = event.raw_event->comm;
  sample->set_pid(comm.pid);
  sample->set_tid(comm.tid);
  sample->set_comm(comm.comm);
  sample->set_comm_md5_prefix(Md5Prefix(comm.comm));

  SerializeSampleInfo(event, sample->mutable_sample_info());
}

void PerfSerializer::DeserializeCommSample(
    const PerfDataProto_CommEvent& sample,
    ParsedEvent* event) const {
  struct comm_event& comm = event->raw_event->comm;
  comm.pid = sample.pid();
  comm.tid = sample.tid();
  snprintf(comm.comm, sizeof(comm.comm), "%s", sample.comm().c_str());

  DeserializeSampleInfo(sample.sample_info(), event);
}

void PerfSerializer::SerializeForkSample(
    const ParsedEvent& event,
    PerfDataProto_ForkEvent* sample) const {
  const struct fork_event& fork = event.raw_event->fork;
  sample->set_pid(fork.pid);
  sample->set_ppid(fork.ppid);
  sample->set_tid(fork.tid);
  sample->set_ptid(fork.ppid);
  sample->set_fork_time_ns(fork.time);

  SerializeSampleInfo(event, sample->mutable_sample_info());
}

void PerfSerializer::DeserializeForkSample(
    const PerfDataProto_ForkEvent& sample,
    ParsedEvent* event) const {
  struct fork_event& fork = event->raw_event->fork;
  fork.pid = sample.pid();
  fork.ppid = sample.ppid();
  fork.tid = sample.tid();
  fork.ptid = sample.ptid();
  fork.time = sample.fork_time_ns();

  DeserializeSampleInfo(sample.sample_info(), event);
}

void PerfSerializer::SerializeSampleInfo(
    const ParsedEvent& event,
    PerfDataProto_SampleInfo* sample_info) const {
  const struct perf_sample perf_sample = *event.sample_info;

  if (sample_type_ & PERF_SAMPLE_TID) {
    sample_info->set_pid(perf_sample.pid);
    sample_info->set_tid(perf_sample.tid);
  }
  if (sample_type_ & PERF_SAMPLE_TIME)
    sample_info->set_sample_time_ns(perf_sample.time);
  if (sample_type_ & PERF_SAMPLE_ID)
    sample_info->set_id(perf_sample.id);
  if (sample_type_ & PERF_SAMPLE_CPU)
    sample_info->set_cpu(perf_sample.cpu);
}

void PerfSerializer::DeserializeSampleInfo(
    const PerfDataProto_SampleInfo& sample_info,
    ParsedEvent* event) const {
  struct perf_sample& perf_sample = *event->sample_info;
  if (sample_info.has_tid()) {
    perf_sample.pid = sample_info.pid();
    perf_sample.tid = sample_info.tid();
  }
  if (sample_info.has_sample_time_ns())
    perf_sample.time = sample_info.sample_time_ns();
  if (sample_info.has_id())
    perf_sample.id = sample_info.id();
  if (sample_info.has_cpu())
    perf_sample.cpu = sample_info.cpu();
}

void PerfSerializer::SerializeBuildIDs(
    const PerfBuildIDMetadata& from,
    ::google::protobuf::RepeatedPtrField<PerfDataProto_PerfBuildID>* to) const {
  SerializeBuildIDEvents(from.events, to);
}

void PerfSerializer::DeserializeBuildIDs(
    const ::google::protobuf::RepeatedPtrField<PerfDataProto_PerfBuildID>& from,
    PerfBuildIDMetadata* to) const {
  // Free any existing build id events.
  for (size_t i = 0; i < to->events.size(); ++i)
    free(to->events[i]);
  to->events.clear();

  to->type = HEADER_BUILD_ID;
  DeserializeBuildIDEvents(from, &to->events);

  to->size = 0;
  for (size_t i = 0; i < to->events.size(); ++i)
    to->size += to->events[i]->header.size;
}

void PerfSerializer::SerializeBuildIDEvent(
    build_id_event* const& from,
    PerfDataProto_PerfBuildID* to) const {
  to->set_misc(from->header.misc);
  to->set_pid(from->pid);
  to->set_build_id(from->build_id, sizeof(from->build_id));
  to->set_filename(from->filename);
}

void PerfSerializer::DeserializeBuildIDEvent(
    const PerfDataProto_PerfBuildID& from,
    build_id_event** to) const {
  const string& filename = from.filename();
  size_t size = sizeof(build_id_event) + GetUint64AlignedStringLength(filename);

  build_id_event* event = (build_id_event*) malloc(size);
  memset(event, 0, size);
  *to = event;
  event->header.type = PERF_RECORD_HEADER_BUILD_ID;
  event->header.size = size;
  event->header.misc = from.misc();
  event->pid = from.pid();
  memcpy(event->build_id, from.build_id().c_str(), sizeof(event->build_id));

  CHECK_GT(snprintf(event->filename, filename.size() + 1, "%s",
                    filename.c_str()),
           0);
}

void PerfSerializer::SerializeSingleStringMetadata(
    const PerfStringMetadata& metadata,
    PerfDataProto_PerfStringMetadata* proto_metadata) const {
  proto_metadata->set_type(metadata.type);
  proto_metadata->set_data(metadata.data);
}

void PerfSerializer::DeserializeSingleStringMetadata(
    const PerfDataProto_PerfStringMetadata& proto_metadata,
    PerfStringMetadata* metadata) const {
  metadata->type = proto_metadata.type();
  metadata->data = proto_metadata.data();
  metadata->len = GetUint64AlignedStringLength(metadata->data);
  metadata->size = metadata->len + sizeof(metadata->len);
}

void PerfSerializer::SetRawEventsAndSampleInfos(size_t num_events) {
  events_.resize(num_events);
  parsed_events_.resize(num_events);
  for (size_t i = 0; i < num_events; ++i) {
    parsed_events_[i].raw_event = &events_[i].event;
    parsed_events_[i].sample_info = &events_[i].sample_info;
  }
}

}  // namespace quipper
