// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMIUMOS_WIDE_PROFILING_PERF_SERIALIZER_H_
#define CHROMIUMOS_WIDE_PROFILING_PERF_SERIALIZER_H_

#include <memory>
#include <vector>

#include "base/macros.h"

#include "chromiumos-wide-profiling/compat/proto.h"
#include "chromiumos-wide-profiling/compat/string.h"
#include "chromiumos-wide-profiling/sample_info_reader.h"
#include "chromiumos-wide-profiling/utils.h"

struct perf_event_attr;

namespace quipper {

struct ParsedEvent;
struct PerfFileAttr;
struct PerfNodeTopologyMetadata;
struct PerfCPUTopologyMetadata;
struct PerfEventStats;
struct PerfParserOptions;
struct PerfUint32Metadata;
struct PerfUint64Metadata;

class PerfParser;
class PerfReader;
class PerfSerializer;

// Functor to serialize a vector of perf structs to a Proto repeated field,
// using a method serializes one such item.
// Overriding RefT allows serializing a vector of pointers to struct with
// a serialize member fuction that takes const T* (instead of T*const&).
template <typename Proto, typename T, typename RefT = const T&>
struct VectorSerializer {
  bool operator()(const std::vector<T>& from,
                  RepeatedPtrField<Proto>* to) const {
    to->Reserve(from.size());
    for (size_t i = 0; i != from.size(); ++i) {
      Proto* to_element = to->Add();
      if (to_element == NULL) {
        return false;
      }
      if (!(p->*serialize)(from[i], to_element)) {
        return false;
      }
    }
    return true;
  }
  const PerfSerializer* p;
  bool (PerfSerializer::*serialize)(RefT, Proto*) const;
};

// Functor to deserialize a Proto repeated field to a vector of perf structs,
// using a method that deserializes one Proto.
template <typename Proto, typename T>
struct VectorDeserializer {
  bool operator()(const RepeatedPtrField<Proto>& from,
                  std::vector<T>* to) const {
    to->resize(from.size());
    for (int i = 0; i != from.size(); ++i) {
      if (!(p->*deserialize)(from.Get(i), &(*to)[i])) {
        return false;
      }
    }
    return true;
  }
  const PerfSerializer* p;
  bool (PerfSerializer::*deserialize)(const Proto&, T*) const;
};

class PerfSerializer {
 public:
  PerfSerializer();
  ~PerfSerializer();

  // Converts raw perf file to protobuf.
  bool SerializeFromFile(const string& filename,
                         PerfDataProto* perf_data_proto);

  // Converts raw perf file to protobuf, specifying options to pass to
  // PerfParser.
  bool SerializeFromFileWithOptions(const string& filename,
                                    const PerfParserOptions& options,
                                    PerfDataProto* perf_data_proto);

  // Converts data inside PerfSerializer to protobuf. |parser| is optional, can
  // be nullptr.
  bool Serialize(const PerfReader& reader,
                 const PerfParser* parser,
                 PerfDataProto* proto);

  // Converts perf data protobuf to perf data file.
  bool DeserializeToFile(const PerfDataProto& perf_data_proto,
                         const string& filename);

  // Reads in contents of protobuf to store locally. Does not write to any
  // output files. |parser| is optional, can be nullptr.
  bool Deserialize(const PerfDataProto& proto,
                   PerfReader* reader,
                   PerfParser* parser);

  void set_serialize_sorted_events(bool sorted) {
    serialize_sorted_events_ = sorted;
  }

 private:
  bool SerializePerfFileAttr(
      const PerfFileAttr& perf_file_attr,
      PerfDataProto_PerfFileAttr* perf_file_attr_proto) const;
  bool DeserializePerfFileAttr(
      const PerfDataProto_PerfFileAttr& perf_file_attr_proto,
      PerfFileAttr* perf_file_attr) const;

  bool SerializePerfEventAttr(
      const perf_event_attr& perf_event_attr,
      PerfDataProto_PerfEventAttr* perf_event_attr_proto) const;
  bool DeserializePerfEventAttr(
      const PerfDataProto_PerfEventAttr& perf_event_attr_proto,
      perf_event_attr* perf_event_attr) const;

  bool SerializePerfEventType(
      const PerfFileAttr& event_attr,
      PerfDataProto_PerfEventType* event_type_proto) const;
  bool DeserializePerfEventType(
      const PerfDataProto_PerfEventType& event_type_proto,
      PerfFileAttr* event_attr) const;

  bool SerializeEvent(const malloced_unique_ptr<event_t>& event,
                      PerfDataProto_PerfEvent* event_proto) const;
  bool SerializeParsedEvent(const ParsedEvent& event,
                            PerfDataProto_PerfEvent* event_proto) const;
  bool SerializeParsedEventPointer(const ParsedEvent* event_ptr,
                                   PerfDataProto_PerfEvent* event_proto) const;
  bool SerializeRawEvent(const event_t& event,
                         PerfDataProto_PerfEvent* event_proto) const;

  bool DeserializeEvent(const PerfDataProto_PerfEvent& event_proto,
                        malloced_unique_ptr<event_t>* event) const;

  bool SerializeEventHeader(const perf_event_header& header,
                            PerfDataProto_EventHeader* header_proto) const;
  bool DeserializeEventHeader(const PerfDataProto_EventHeader& header_proto,
                              perf_event_header* header) const;

  bool SerializeRecordSample(const event_t& event,
                             PerfDataProto_SampleEvent* sample) const;
  bool DeserializeRecordSample(const PerfDataProto_SampleEvent& sample,
                               event_t* event) const;

  bool SerializeMMapSample(const event_t& event,
                           PerfDataProto_MMapEvent* sample) const;
  bool DeserializeMMapSample(const PerfDataProto_MMapEvent& sample,
                             event_t* event) const;

  bool SerializeMMap2Sample(const event_t& event,
                            PerfDataProto_MMapEvent* sample) const;
  bool DeserializeMMap2Sample(const PerfDataProto_MMapEvent& sample,
                              event_t* event) const;

  bool SerializeCommSample(const event_t& event,
                           PerfDataProto_CommEvent* sample) const;
  bool DeserializeCommSample(const PerfDataProto_CommEvent& sample,
                             event_t* event) const;

  // These handle both fork and exit events, which use the same protobuf
  // message definition.
  bool SerializeForkExitSample(const event_t& event,
                               PerfDataProto_ForkEvent* sample) const;
  bool DeserializeForkExitSample(const PerfDataProto_ForkEvent& sample,
                                 event_t* event) const;

  bool SerializeLostSample(const event_t& event,
                           PerfDataProto_LostEvent* sample) const;
  bool DeserializeLostSample(const PerfDataProto_LostEvent& sample,
                             event_t* event) const;

  bool SerializeThrottleSample(const event_t& event,
                               PerfDataProto_ThrottleEvent* sample) const;
  bool DeserializeThrottleSample(const PerfDataProto_ThrottleEvent& sample,
                                 event_t* event) const;

  bool SerializeReadSample(const event_t& event,
                           PerfDataProto_ReadEvent* sample) const;
  bool DeserializeReadSample(const PerfDataProto_ReadEvent& sample,
                             event_t* event) const;

  bool SerializeSampleInfo(const event_t& event,
                           PerfDataProto_SampleInfo* sample_info) const;
  bool DeserializeSampleInfo(const PerfDataProto_SampleInfo& info,
                             event_t* event) const;

  bool SerializeTracingMetadata(const std::vector<char>& from,
                                PerfDataProto* to) const;
  bool DeserializeTracingMetadata(const PerfDataProto& from,
                                  std::vector<char>* to) const;

  bool SerializeBuildIDs(
      const std::vector<malloced_unique_ptr<build_id_event>>& from,
      RepeatedPtrField<PerfDataProto_PerfBuildID>* to)
      const;
  bool DeserializeBuildIDs(
      const RepeatedPtrField<PerfDataProto_PerfBuildID>& from,
      std::vector<malloced_unique_ptr<build_id_event>>* to) const;

  bool SerializeMetadata(const PerfReader& from, PerfDataProto* to) const;
  bool DeserializeMetadata(const PerfDataProto& from, PerfReader* to);

  bool SerializeBuildIDEvent(const malloced_unique_ptr<build_id_event>& from,
                             PerfDataProto_PerfBuildID* to) const;
  bool DeserializeBuildIDEvent(const PerfDataProto_PerfBuildID& from,
                               malloced_unique_ptr<build_id_event>* to) const;

  bool SerializeSingleUint32Metadata(
      const PerfUint32Metadata& metadata,
      PerfDataProto_PerfUint32Metadata* proto_metadata) const;
  bool DeserializeSingleUint32Metadata(
      const PerfDataProto_PerfUint32Metadata& proto_metadata,
      PerfUint32Metadata* metadata) const;

  bool SerializeSingleUint64Metadata(
      const PerfUint64Metadata& metadata,
      PerfDataProto_PerfUint64Metadata* proto_metadata) const;
  bool DeserializeSingleUint64Metadata(
      const PerfDataProto_PerfUint64Metadata& proto_metadata,
      PerfUint64Metadata* metadata) const;

  bool SerializeCPUTopologyMetadata(
      const PerfCPUTopologyMetadata& metadata,
      PerfDataProto_PerfCPUTopologyMetadata* proto_metadata) const;
  bool DeserializeCPUTopologyMetadata(
      const PerfDataProto_PerfCPUTopologyMetadata& proto_metadata,
      PerfCPUTopologyMetadata* metadata) const;

  bool SerializeNodeTopologyMetadata(
      const PerfNodeTopologyMetadata& metadata,
      PerfDataProto_PerfNodeTopologyMetadata* proto_metadata) const;
  bool DeserializeNodeTopologyMetadata(
      const PerfDataProto_PerfNodeTopologyMetadata& proto_metadata,
      PerfNodeTopologyMetadata* metadata) const;

  static void SerializeParserStats(const PerfEventStats& stats,
                                   PerfDataProto* perf_data_proto);
  static void DeserializeParserStats(const PerfDataProto& perf_data_proto,
                                     PerfEventStats* stats);

  const VectorSerializer<PerfDataProto_PerfFileAttr, PerfFileAttr>
      SerializePerfFileAttrs = {this, &PerfSerializer::SerializePerfFileAttr};
  const VectorDeserializer<PerfDataProto_PerfFileAttr, PerfFileAttr>
      DeserializePerfFileAttrs = {
        this, &PerfSerializer::DeserializePerfFileAttr};

  const VectorSerializer<PerfDataProto_PerfEventType, PerfFileAttr>
      SerializePerfEventTypes = {this, &PerfSerializer::SerializePerfEventType};
  const VectorDeserializer<PerfDataProto_PerfEventType, PerfFileAttr>
      DeserializePerfEventTypes = {
        this, &PerfSerializer::DeserializePerfEventType};

  const VectorSerializer<PerfDataProto_PerfEvent, ParsedEvent>
      SerializeParsedEvents = {this, &PerfSerializer::SerializeParsedEvent};
  const VectorSerializer<PerfDataProto_PerfEvent,
                         ParsedEvent*, const ParsedEvent*>
      SerializeParsedEventPointers = {
        this, &PerfSerializer::SerializeParsedEventPointer};

  const VectorSerializer<PerfDataProto_PerfEvent, malloced_unique_ptr<event_t>>
      SerializeEvents = {this, &PerfSerializer::SerializeEvent};
  const VectorDeserializer<PerfDataProto_PerfEvent,
                           malloced_unique_ptr<event_t>>
      DeserializeEvents = {this, &PerfSerializer::DeserializeEvent};

  const VectorSerializer<PerfDataProto_PerfBuildID,
                         malloced_unique_ptr<build_id_event>>
      SerializeBuildIDEvents = {this, &PerfSerializer::SerializeBuildIDEvent};
  const VectorDeserializer<PerfDataProto_PerfBuildID,
                           malloced_unique_ptr<build_id_event>>
      DeserializeBuildIDEvents = {
        this, &PerfSerializer::DeserializeBuildIDEvent};

  const VectorSerializer<PerfDataProto_PerfUint32Metadata, PerfUint32Metadata>
      SerializeUint32Metadata = {
        this, &PerfSerializer::SerializeSingleUint32Metadata};
  const VectorDeserializer<PerfDataProto_PerfUint32Metadata, PerfUint32Metadata>
      DeserializeUint32Metadata = {
        this, &PerfSerializer::DeserializeSingleUint32Metadata};

  const VectorSerializer<PerfDataProto_PerfUint64Metadata, PerfUint64Metadata>
      SerializeUint64Metadata = {
        this, &PerfSerializer::SerializeSingleUint64Metadata};
  const VectorDeserializer<PerfDataProto_PerfUint64Metadata, PerfUint64Metadata>
      DeserializeUint64Metadata = {
        this, &PerfSerializer::DeserializeSingleUint64Metadata};

  const VectorSerializer<PerfDataProto_PerfNodeTopologyMetadata,
                         PerfNodeTopologyMetadata>
      SerializeNUMATopologyMetadata = {
        this, &PerfSerializer::SerializeNodeTopologyMetadata};
  const VectorDeserializer<PerfDataProto_PerfNodeTopologyMetadata,
                           PerfNodeTopologyMetadata>
      DeserializeNUMATopologyMetadata = {
        this, &PerfSerializer::DeserializeNodeTopologyMetadata};

  // Instantiate a new PerfSampleReader with the given parameters. If an old one
  // exists, it is discarded.
  void CreateSampleInfoReader(const struct perf_event_attr& event_attr,
                              bool is_cross_endian);

  // Set this flag to serialize perf events in chronological order, rather than
  // the order in which they appear in the raw data.
  bool serialize_sorted_events_;

  // Use this to serialize or deserialize sample info fields in events.
  std::unique_ptr<SampleInfoReader> sample_info_reader_;

  DISALLOW_COPY_AND_ASSIGN(PerfSerializer);
};

}  // namespace quipper

#endif  // CHROMIUMOS_WIDE_PROFILING_PERF_SERIALIZER_H_
