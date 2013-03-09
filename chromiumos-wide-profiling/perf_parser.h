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
  // TODO(sque): to save space, |raw_event| should be a pointer.
  event_t raw_event;                // Contains perf event info.
  struct perf_sample sample_info;   // Contains perf sample info.
};

class PerfParser : public PerfReader {
 public:
  PerfParser() {}
  bool ParseRawEvents();
  bool GenerateRawEvents();

 protected:
  std::vector<ParsedEvent> parsed_events_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PerfParser);
};

#endif  // PERF_PARSER_H_
