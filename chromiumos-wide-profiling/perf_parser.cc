// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "perf_parser.h"

#include "utils.h"

bool PerfParser::ParseRawEvents(const std::vector<PerfFileAttr>& attrs,
                                const std::vector<event_t>& raw_events) {
  events_.resize(raw_events.size());
  for (size_t i = 0; i < raw_events.size(); ++i) {
    const event_t& raw_event = raw_events[i];
    ParsedEvent& parsed_event = events_[i];
    CopyPerfEventSpecificInfo(raw_event, &parsed_event.raw_event);
    if (!ReadPerfSampleInfo(raw_event,
                            attrs[0].attr.sample_type,
                            &parsed_event.sample_info)) {
      return false;
    }
  }
  return true;
}

bool PerfParser::GenerateRawEvents(const std::vector<PerfFileAttr>& attrs,
                                   const std::vector<ParsedEvent>& events) {
  raw_events_.resize(events.size());
  for (size_t i = 0; i < events.size(); ++i) {
    const ParsedEvent& parsed_event = events[i];
    event_t& raw_event = raw_events_[i];
    CopyPerfEventSpecificInfo(parsed_event.raw_event, &raw_event);
    if (!WritePerfSampleInfo(parsed_event.sample_info,
                             attrs[0].attr.sample_type,
                             &raw_event)) {
      return false;
    }
  }
  return true;
}
