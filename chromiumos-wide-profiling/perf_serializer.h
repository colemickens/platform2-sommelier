// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PERF_SERIALIZER_H_
#define PERF_SERIALIZER_H_

#include <string>
#include <vector>

#include "base/basictypes.h"

#include "perf_parser.h"
#include "perf_data.pb.h"

namespace quipper {

class PerfSerializer : public PerfParser {
 public:
  PerfSerializer();
  ~PerfSerializer();

  // Converts raw perf file to protobuf.
  bool SerializeFromFile(const string& filename,
                         quipper::PerfDataProto* perf_data_proto);

  // Converts data inside PerfSerializer to protobuf.
  bool Serialize(quipper::PerfDataProto* perf_data_proto);

  // Converts perf data protobuf to perf data file.
  bool DeserializeToFile(const quipper::PerfDataProto& perf_data_proto,
                         const string& filename);

  // Reads in contents of protobuf to store locally.  Does not write to any
  // output files.
  bool Deserialize(const quipper::PerfDataProto& perf_data_proto);

 private:
  void SerializePerfFileAttr(
      const PerfFileAttr& perf_file_attr,
      quipper::PerfDataProto_PerfFileAttr* perf_file_attr_proto) const;
  void DeserializePerfFileAttr(
      const quipper::PerfDataProto_PerfFileAttr& perf_file_attr_proto,
      PerfFileAttr* perf_file_attr) const;

  void SerializePerfEventAttr(
      const perf_event_attr& perf_event_attr,
      quipper::PerfDataProto_PerfEventAttr* perf_event_attr_proto) const;
  void DeserializePerfEventAttr(
      const quipper::PerfDataProto_PerfEventAttr& perf_event_attr_proto,
      perf_event_attr* perf_event_attr) const;

  void SerializePerfEventType(
      const perf_trace_event_type& event_type,
      quipper::PerfDataProto_PerfEventType* event_type_proto) const;
  void DeserializePerfEventType(
      const quipper::PerfDataProto_PerfEventType& event_type_proto,
      perf_trace_event_type* event_type) const;

  void SerializeEvent(const ParsedEvent& event,
                      quipper::PerfDataProto_PerfEvent* event_proto) const;
  void DeserializeEvent(
      const quipper::PerfDataProto_PerfEvent& event_proto,
      ParsedEvent* event) const;

  void SerializeEventHeader(
      const perf_event_header& header,
      quipper::PerfDataProto_EventHeader* header_proto) const;
  void DeserializeEventHeader(
      const quipper::PerfDataProto_EventHeader& header_proto,
      perf_event_header* header) const;

  void SerializeRecordSample(const ParsedEvent& event,
                             quipper::PerfDataProto_SampleEvent* sample) const;
  void DeserializeRecordSample(
      const quipper::PerfDataProto_SampleEvent& sample,
      ParsedEvent* event) const;

  void SerializeMMapSample(const ParsedEvent& event,
                           quipper::PerfDataProto_MMapEvent* sample) const;
  void DeserializeMMapSample(
      const quipper::PerfDataProto_MMapEvent& sample,
      ParsedEvent* event) const;

  void SerializeCommSample(
      const ParsedEvent& event,
      quipper::PerfDataProto_CommEvent* sample) const;
  void DeserializeCommSample(
      const quipper::PerfDataProto_CommEvent& sample,
      ParsedEvent* event) const;

  void SerializeForkSample(const ParsedEvent& event,
                           quipper::PerfDataProto_ForkEvent* sample) const;
  void DeserializeForkSample(
      const quipper::PerfDataProto_ForkEvent& sample,
      ParsedEvent* event) const;

  void SerializeSampleInfo(
      const ParsedEvent& event,
      quipper::PerfDataProto_SampleInfo* sample_info) const;
  void DeserializeSampleInfo(
      const quipper::PerfDataProto_SampleInfo& info,
      ParsedEvent* event) const;

  void SerializeBuildIDs(
      const std::vector<build_id_event*>& from,
      ::google::protobuf::RepeatedPtrField<PerfDataProto_PerfBuildID>* to)
      const;
  void DeserializeBuildIDs(
      const
      ::google::protobuf::RepeatedPtrField<PerfDataProto_PerfBuildID>& from,
      std::vector<build_id_event*>* to) const;

  void SerializeBuildIDEvent(build_id_event* const& from,
                             PerfDataProto_PerfBuildID* to) const;
  void DeserializeBuildIDEvent(const PerfDataProto_PerfBuildID& from,
                               build_id_event** to) const;

  void SerializeSingleStringMetadata(
      const PerfStringMetadata& metadata,
      PerfDataProto_PerfStringMetadata* proto_metadata) const;
  void DeserializeSingleStringMetadata(
      const PerfDataProto_PerfStringMetadata& proto_metadata,
      PerfStringMetadata* metadata) const;

  void SerializeSingleUint32Metadata(
      const PerfUint32Metadata& metadata,
      PerfDataProto_PerfUint32Metadata* proto_metadata) const;
  void DeserializeSingleUint32Metadata(
      const PerfDataProto_PerfUint32Metadata& proto_metadata,
      PerfUint32Metadata* metadata) const;

  void SerializeSingleUint64Metadata(
      const PerfUint64Metadata& metadata,
      PerfDataProto_PerfUint64Metadata* proto_metadata) const;
  void DeserializeSingleUint64Metadata(
      const PerfDataProto_PerfUint64Metadata& proto_metadata,
      PerfUint64Metadata* metadata) const;

  // Populates |parsed_events_| with pointers event_t and perf_sample structs in
  // each corresponding |events_| struct.
  void SetRawEventsAndSampleInfos(size_t num_events);

#define SERIALIZEVECTORFUNCTION(name, vec_type, proto_type, function) \
bool name(const std::vector<vec_type>& from, \
          ::google::protobuf::RepeatedPtrField<proto_type>* to) const { \
  to->Reserve(from.size()); \
  for (size_t i = 0; i < from.size(); i++) { \
    proto_type* to_element = to->Add(); \
    if (to_element == NULL) \
      return false; \
    function(from.at(i), to->Mutable(i)); \
  } \
  return true; \
}

#define DESERIALIZEVECTORFUNCTION(name, vec_type, proto_type, function) \
bool name(const ::google::protobuf::RepeatedPtrField<proto_type>& from, \
          std::vector<vec_type>* to) const { \
  to->resize(from.size()); \
  for (int i = 0; i < from.size(); i++) \
    function(from.Get(i), &to->at(i)); \
  return true; \
}

  SERIALIZEVECTORFUNCTION(SerializePerfFileAttrs, PerfFileAttr,
                          quipper::PerfDataProto_PerfFileAttr,
                          SerializePerfFileAttr)
  DESERIALIZEVECTORFUNCTION(DeserializePerfFileAttrs, PerfFileAttr,
                            quipper::PerfDataProto_PerfFileAttr,
                            DeserializePerfFileAttr)

  SERIALIZEVECTORFUNCTION(SerializePerfEventTypes, perf_trace_event_type,
                          quipper::PerfDataProto_PerfEventType,
                          SerializePerfEventType)
  DESERIALIZEVECTORFUNCTION(DeserializePerfEventTypes, perf_trace_event_type,
                            quipper::PerfDataProto_PerfEventType,
                            DeserializePerfEventType)

  SERIALIZEVECTORFUNCTION(SerializeEvents, ParsedEvent,
                          quipper::PerfDataProto_PerfEvent,
                          SerializeEvent)
  DESERIALIZEVECTORFUNCTION(DeserializeEvents, ParsedEvent,
                            quipper::PerfDataProto_PerfEvent,
                            DeserializeEvent)

  SERIALIZEVECTORFUNCTION(SerializeBuildIDEvents, build_id_event*,
                          quipper::PerfDataProto_PerfBuildID,
                          SerializeBuildIDEvent)
  DESERIALIZEVECTORFUNCTION(DeserializeBuildIDEvents, build_id_event*,
                            quipper::PerfDataProto_PerfBuildID,
                            DeserializeBuildIDEvent)

  SERIALIZEVECTORFUNCTION(SerializeStringMetadata, PerfStringMetadata,
                          quipper::PerfDataProto_PerfStringMetadata,
                          SerializeSingleStringMetadata)
  DESERIALIZEVECTORFUNCTION(DeserializeStringMetadata, PerfStringMetadata,
                            quipper::PerfDataProto_PerfStringMetadata,
                            DeserializeSingleStringMetadata)

  SERIALIZEVECTORFUNCTION(SerializeUint32Metadata, PerfUint32Metadata,
                          quipper::PerfDataProto_PerfUint32Metadata,
                          SerializeSingleUint32Metadata)
  DESERIALIZEVECTORFUNCTION(DeserializeUint32Metadata, PerfUint32Metadata,
                            quipper::PerfDataProto_PerfUint32Metadata,
                            DeserializeSingleUint32Metadata)

  SERIALIZEVECTORFUNCTION(SerializeUint64Metadata, PerfUint64Metadata,
                          quipper::PerfDataProto_PerfUint64Metadata,
                          SerializeSingleUint64Metadata)
  DESERIALIZEVECTORFUNCTION(DeserializeUint64Metadata, PerfUint64Metadata,
                            quipper::PerfDataProto_PerfUint64Metadata,
                            DeserializeSingleUint64Metadata)

  DISALLOW_COPY_AND_ASSIGN(PerfSerializer);
};

}  // namespace quipper

#endif  // PERF_SERIALIZER_H_
