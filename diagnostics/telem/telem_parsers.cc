// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "diagnostics/telem/telem_parsers.h"

#include <base/macros.h>
#include <base/optional.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/string_split.h>
#include <base/strings/string_tokenizer.h>
#include <base/values.h>
#include <re2/re2.h>
#include <sstream>
#include <string>

namespace diagnostics {

static void ParseProcMeminfo(const FileDumps& file_dumps,
                             base::Optional<base::Value>* mem_total,
                             base::Optional<base::Value>* mem_free) {
  if (file_dumps.size() != 1) {
    LOG(ERROR) << "Unable to find meminfo file.";
    return;
  }

  const FileDump& file_dump = file_dumps[0];

  // Parse the meminfo response for kMemTotal and kMemFree.
  base::StringPairs keyVals;
  base::SplitStringIntoKeyValuePairs(file_dump.contents, ':', '\n', &keyVals);

  for (int i = 0; i < keyVals.size(); i++) {
    if (keyVals[i].first == "MemTotal") {
      // Convert from kB to MB and cache the result.
      int memtotal_mb;
      base::StringTokenizer t(keyVals[i].second, " ");
      if (t.GetNext() && base::StringToInt(t.token(), &memtotal_mb) &&
          t.GetNext() && t.token() == "kB") {
        memtotal_mb /= 1024;
        *mem_total = base::Optional<base::Value>(base::Value(memtotal_mb));
      } else {
        LOG(ERROR) << "Incorrectly formatted MemTotal.";
      }
    } else if (keyVals[i].first == "MemFree") {
      // Convert from kB to MB and cache the result.
      int memfree_mb;
      base::StringTokenizer t(keyVals[i].second, " ");
      if (t.GetNext() && base::StringToInt(t.token(), &memfree_mb) &&
          t.GetNext() && t.token() == "kB") {
        memfree_mb /= 1024;
        *mem_free = base::Optional<base::Value>(base::Value(memfree_mb));
      } else {
        LOG(ERROR) << "Incorrectly formatted MemFree.";
      }
    }
  }
}

void ParseDataFromProcMeminfo(const FileDumps& file_dumps, CacheWriter* cache) {
  base::Optional<base::Value> mem_total;
  base::Optional<base::Value> mem_free;

  ParseProcMeminfo(file_dumps, &mem_total, &mem_free);

  cache->SetParsedData(TelemetryItemEnum::kMemTotalMebibytes, mem_total);
  cache->SetParsedData(TelemetryItemEnum::kMemFreeMebibytes, mem_free);
}

static void ParseProcLoadavg(const FileDumps& file_dumps,
                             base::Optional<base::Value>* num_runnable,
                             base::Optional<base::Value>* num_existing) {
  // Make sure we received exactly one file.
  if (file_dumps.size() != 1) {
    LOG(ERROR) << "Unable to find loadavg file.";
    return;
  }

  // We expect the loadavg response to have the following format:
  // %f %f %f %d/%d %d. At the moment, we're only interested in
  // the %d/%d: it will parse into kNumRunnableEntities/kNumExistingEntities.
  // We'll first make sure the entire response matches the expected format,
  // then we'll extract the %d/%d.
  int running_entities;
  int existing_entities;
  if (!RE2::FullMatch(file_dumps[0].contents,
                      "\\d+\\.\\d+\\s\\d+\\.\\d+\\s\\d+"
                      "\\.\\d+\\s(\\d+)/(\\d+)\\s\\d+\\n",
                      &running_entities, &existing_entities)) {
    LOG(ERROR) << "Incorrectly formatted loadavg.";
    return;
  }
  *num_runnable = base::Optional<base::Value>(base::Value(running_entities));
  *num_existing = base::Optional<base::Value>(base::Value(existing_entities));
}

void ParseDataFromProcLoadavg(const FileDumps& file_dumps, CacheWriter* cache) {
  base::Optional<base::Value> num_runnable;
  base::Optional<base::Value> num_existing;

  ParseProcLoadavg(file_dumps, &num_runnable, &num_existing);

  cache->SetParsedData(TelemetryItemEnum::kNumRunnableEntities, num_runnable);
  cache->SetParsedData(TelemetryItemEnum::kNumExistingEntities, num_existing);
}

static void ParseProcStat(
    const FileDumps& file_dumps,
    base::Optional<base::Value>* total_idle_time_user_hz,
    base::Optional<base::Value>* idle_time_per_cpu_user_hz) {
  // Make sure we received exactly one file in the response.
  if (file_dumps.size() != 1) {
    LOG(ERROR) << "Unable to find stat file.";
    return;
  }

  // Grab the idle time for all CPUs combined, as well as the idle time
  // for each logical CPU. We'll store each of the times as a string in
  // case the system has been on for long enough to overflow an int with
  // its idle time.
  std::string idle_time_combined;
  std::stringstream response_sstream(file_dumps[0].contents);
  std::string current_line;
  // The first line should be: cpu %d %d %d %d ..., where the last number
  // is the idle time.
  if (!std::getline(response_sstream, current_line) ||
      !RE2::PartialMatch(current_line, "cpu\\s+\\d+ \\d+ \\d+ (\\d+)",
                         &idle_time_combined)) {
    LOG(ERROR) << "Incorrectly formatted stat.";
    return;
  }

  // The next N lines should be: cpu%d %d %d %d %d ..., where N is the number
  // of logical CPUs.
  std::string idle_time_current_cpu;
  base::ListValue logical_cpu_idle_time;
  while (std::getline(response_sstream, current_line) &&
         RE2::PartialMatch(current_line, "cpu\\d+ \\d+ \\d+ \\d+ (\\d+)",
                           &idle_time_current_cpu)) {
    logical_cpu_idle_time.AppendString(idle_time_current_cpu);
  }

  *total_idle_time_user_hz =
      base::Optional<base::Value>(base::Value(idle_time_combined));
  *idle_time_per_cpu_user_hz =
      base::Optional<base::Value>(logical_cpu_idle_time);
}

void ParseDataFromProcStat(const FileDumps& file_dumps, CacheWriter* cache) {
  base::Optional<base::Value> total_idle_time_user_hz;
  base::Optional<base::Value> idle_time_per_cpu_user_hz;

  ParseProcStat(file_dumps, &total_idle_time_user_hz,
                &idle_time_per_cpu_user_hz);

  cache->SetParsedData(TelemetryItemEnum::kTotalIdleTimeUserHz,
                       total_idle_time_user_hz);
  cache->SetParsedData(TelemetryItemEnum::kIdleTimePerCPUUserHz,
                       idle_time_per_cpu_user_hz);
}

}  // namespace diagnostics
