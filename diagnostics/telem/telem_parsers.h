// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DIAGNOSTICS_TELEM_TELEM_PARSERS_H_
#define DIAGNOSTICS_TELEM_TELEM_PARSERS_H_

#include "diagnostics/telem/telem_cache.h"
#include "diagnostics/telem/telemetry.h"

namespace diagnostics {

void ParseDataFromProcLoadavg(const FileDumps& file_dumps, CacheWriter* cache);
void ParseDataFromProcStat(const FileDumps& file_dumps, CacheWriter* cache);
void ParseDataFromProcMeminfo(const FileDumps& file_dumps, CacheWriter* cache);

}  // namespace diagnostics

#endif  // DIAGNOSTICS_TELEM_TELEM_PARSERS_H_
