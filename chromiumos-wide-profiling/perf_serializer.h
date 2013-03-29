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

  void SerializeForkSample(const ParsedEvent& event,
                           quipper::PerfDataProto_ForkEvent* sample) const;
  void DeserializeForkSample(
      const quipper::PerfDataProto_ForkEvent& sample,
      ParsedEvent* event) const;

  void SerializeCommSample(
      const ParsedEvent& event,
      quipper::PerfDataProto_CommEvent* sample) const;

  void DeserializeCommSample(
      const quipper::PerfDataProto_CommEvent& sample,
      ParsedEvent* event) const;

  void SerializeSampleInfo(
      const ParsedEvent& event,
      quipper::PerfDataProto_SampleInfo* sample_info) const;
  void DeserializeSampleInfo(
      const quipper::PerfDataProto_SampleInfo& info,
      ParsedEvent* event) const;

  void SerializePerfEventAttr(
      const perf_event_attr& perf_event_attr,
      quipper::PerfDataProto_PerfEventAttr* perf_event_attr_proto);
  void DeserializePerfEventAttr(
      const quipper::PerfDataProto_PerfEventAttr& perf_event_attr_proto,
      perf_event_attr* perf_event_attr);

  void SerializePerfFileAttr(
      const PerfFileAttr& perf_file_attr,
      quipper::PerfDataProto_PerfFileAttr* perf_file_attr_proto);
  void DeserializePerfFileAttr(
      const quipper::PerfDataProto_PerfFileAttr& perf_file_attr_proto,
      PerfFileAttr* perf_file_attr);

#define SERIALIZEVECTORFUNCTION(name, vec_type, proto_type, function) \
bool name(const std::vector<vec_type>& from, \
          ::google::protobuf::RepeatedPtrField<proto_type>* to) { \
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
          std::vector<vec_type>* to) { \
  to->resize(from.size()); \
  for (int i = 0; i < from.size(); i++) \
    function(from.Get(i), &to->at(i)); \
  return true; \
}

  DESERIALIZEVECTORFUNCTION(DeserializePerfFileAttrs, PerfFileAttr,
                            quipper::PerfDataProto_PerfFileAttr,
                            DeserializePerfFileAttr)
  SERIALIZEVECTORFUNCTION(SerializePerfFileAttrs, PerfFileAttr,
                          quipper::PerfDataProto_PerfFileAttr,
                          SerializePerfFileAttr)

  DESERIALIZEVECTORFUNCTION(DeserializeEvents, ParsedEvent,
                            quipper::PerfDataProto_PerfEvent,
                            DeserializeEvent)
  SERIALIZEVECTORFUNCTION(SerializeEvents, ParsedEvent,
                          quipper::PerfDataProto_PerfEvent,
                          SerializeEvent)

  DISALLOW_COPY_AND_ASSIGN(PerfSerializer);
};

}  // namespace quipper

#endif  // PERF_SERIALIZER_H_
