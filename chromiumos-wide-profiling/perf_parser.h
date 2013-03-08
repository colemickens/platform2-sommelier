// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PERF_PARSER_H_
#define PERF_PARSER_H_

#include <vector>
#include <string>

#include "base/basictypes.h"

#include "perf_reader.h"

struct ParsedEvent {
  event_t raw_event;                // Contains perf event info.
  struct perf_sample sample_info;   // Contains perf sample info.
};

class PerfParser {
 public:
  PerfParser() {}
  bool ParseRawEvents(const std::vector<PerfFileAttr>& attrs,
                      const std::vector<event_t>& raw_events);
  bool GenerateRawEvents(const std::vector<PerfFileAttr>& attrs,
                         const std::vector<ParsedEvent>& events);

  const std::vector<ParsedEvent>& events() {
    return events_;
  }

  const std::vector<event_t>& raw_events() {
    return raw_events_;
  }

 private:
  std::vector<ParsedEvent> events_;
  std::vector<event_t> raw_events_;

  DISALLOW_COPY_AND_ASSIGN(PerfParser);
};

#endif  // PERF_PARSER_H_
