// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PERF_SERIALIZER_H_
#define PERF_SERIALIZER_H_

#include <string>

#include "base/basictypes.h"

#include "perf_reader.h"
#include "perf_data.pb.h"

class PerfSerializer {
 public:
  PerfSerializer();
  ~PerfSerializer();

  bool SerializeReader(const PerfReader& perf_reader,
                       PerfDataProto* perf_data_proto);
  bool Serialize(const char* filename,
                 PerfDataProto* perf_data_proto);
  bool DeserializeReader(const PerfDataProto& perf_data_proto,
                         PerfReader* perf_reader);
  bool Deserialize(const char* filename,
                   const PerfDataProto& perf_data_proto);

 private:
  void SerializeEvent(const event_t* event,
                      PerfDataProto_PerfEvent* event_proto) const;
  void DeserializeEvent(event_t* event,
                        const PerfDataProto_PerfEvent* event_proto) const;

  void SerializeEventHeader(const perf_event_header* header,
                            PerfDataProto_EventHeader* header_proto) const;
  void DeserializeEventHeader(
      perf_event_header* header,
      const PerfDataProto_EventHeader* header_proto) const;

  void SerializeRecordSample(const event_t* event,
                             PerfDataProto_SampleEvent* sample) const;
  void DeserializeRecordSample(
      event_t* event,
      const PerfDataProto_SampleEvent* sample) const;

  void SerializeMMapSample(const event_t* event,
                           PerfDataProto_MMapEvent* sample) const;
  void DeserializeMMapSample(event_t* event,
                             const PerfDataProto_MMapEvent* sample) const;

  void SerializeCommSample(
      const event_t* event,
      PerfDataProto_CommEvent* sample) const;

  void DeserializeCommSample(
      event_t* event,
      const PerfDataProto_CommEvent* sample) const;

  void SerializePerfEventAttr(
      const perf_event_attr* perf_event_attr,
      PerfDataProto_PerfEventAttr* perf_event_attr_proto);
  void DeserializePerfEventAttr(
      perf_event_attr* perf_event_attr,
      const PerfDataProto_PerfEventAttr* perf_event_attr_proto);

  void SerializePerfFileAttr(
      const PerfFileAttr* perf_file_attr,
      PerfDataProto_PerfFileAttr* perf_file_attr_proto);
  void DeserializePerfFileAttr(
      PerfFileAttr* perf_file_attr,
      const PerfDataProto_PerfFileAttr* perf_file_attr_proto);

#define SERIALIZEVECTORFUNCTION(name, vec_type, proto_type, function) \
bool name(const std::vector<vec_type>* from, \
          ::google::protobuf::RepeatedPtrField<proto_type>* to) \
{ \
  to->Reserve(from->size()); \
  for (size_t i = 0; i < from->size(); i++) \
  { \
    proto_type* to_element = to->Add(); \
    if (to_element == NULL) \
      return false; \
    function(&from->at(i), to->Mutable(i)); \
  } \
  return true; \
}

#define DESERIALIZEVECTORFUNCTION(name, vec_type, proto_type, function) \
bool name(std::vector<vec_type>* to, \
          const ::google::protobuf::RepeatedPtrField<proto_type>* from) \
{ \
  to->resize(from->size()); \
  for (int i = 0; i < from->size(); i++) \
  { \
    function(&to->at(i), &from->Get(i)); \
  } \
  return true; \
}

  DESERIALIZEVECTORFUNCTION(DeserializePerfFileAttrs, PerfFileAttr,
                            PerfDataProto_PerfFileAttr,
                            DeserializePerfFileAttr)
  SERIALIZEVECTORFUNCTION(SerializePerfFileAttrs, PerfFileAttr,
                          PerfDataProto_PerfFileAttr,
                          SerializePerfFileAttr)

  DESERIALIZEVECTORFUNCTION(DeserializeEvents, event_t,
                            PerfDataProto_PerfEvent,
                            DeserializeEvent)
  SERIALIZEVECTORFUNCTION(SerializeEvents, event_t,
                          PerfDataProto_PerfEvent,
                          SerializeEvent)
  bool type_set_;
  u64 type_;

  DISALLOW_COPY_AND_ASSIGN(PerfSerializer);
};

#endif  // PERF_SERIALIZER_H_
