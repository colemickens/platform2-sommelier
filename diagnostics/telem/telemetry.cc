// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>

#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/logging.h>
#include <base/time/time.h>

#include "diagnostics/telem/telem_parsers.h"
#include "diagnostics/telem/telemetry.h"
#include "diagnostics/telem/telemetry_group_enum.h"

namespace diagnostics {

namespace {

// Mapping between groups and items. Each item belongs to exactly one
// group.
const std::map<TelemetryGroupEnum, std::vector<TelemetryItemEnum>> group_map = {
    {TelemetryGroupEnum::kDisk,
     {TelemetryItemEnum::kMemTotalMebibytes,
      TelemetryItemEnum::kMemFreeMebibytes}}};

// Makes a dump of the specified file. Returns whether the dumping succeeded.
void AppendFileDump(const base::FilePath& root_dir,
                    const base::FilePath& relative_file_path,
                    FileDumps* file_dumps) {
  DCHECK(!relative_file_path.IsAbsolute());
  base::FilePath file_path = root_dir.Append(relative_file_path);

  FileDump file_dump;
  file_dump.real_path = base::MakeAbsoluteFilePath(file_path).value();
  if (!file_dump.real_path.empty()) {
    if (base::ReadFileToString(file_path, &file_dump.contents)) {
      file_dump.path = file_path.value();

      VLOG(2) << "Read " << file_dump.contents.size() << " bytes from "
              << file_dump.path << " with real path " << file_dump.real_path;
      if (file_dumps) {
        file_dumps->emplace_back(std::move(file_dump));
      }
    } else {
      PLOG(ERROR) << "Failed to read from " << file_path.value();
    }
  } else {
    PLOG(ERROR) << "Failed to obtain real path for " << file_path.value();
  }
}

void ReadAndParseFile(const base::FilePath& root_dir,
                      const std::string& relative_path,
                      void (*parser)(const FileDumps&, CacheWriter*),
                      CacheWriter* cache) {
  FileDumps file_dumps;
  AppendFileDump(root_dir, base::FilePath(relative_path), &file_dumps);
  parser(file_dumps, cache);
}

}  // namespace

Telemetry::Telemetry() = default;

Telemetry::Telemetry(const base::FilePath& root_dir) : root_dir_(root_dir) {}

Telemetry::~Telemetry() = default;

base::Optional<base::Value> Telemetry::GetItem(TelemetryItemEnum item,
                                               base::TimeDelta acceptable_age) {
  // First, check to see if the desired telemetry information is
  // present and valid in the cache. If so, just return it.
  if (!cache_.IsValid(item, acceptable_age)) {
    // When no valid cached data is present, take steps to obtain the
    // appropriate telemetry data. This may result in more data being fetched
    // and cached than just the desired item.
    switch (item) {
      case TelemetryItemEnum::kMemTotalMebibytes:  // FALLTHROUGH
      case TelemetryItemEnum::kMemFreeMebibytes:
        ReadAndParseFile(root_dir_, "proc/meminfo", ParseDataFromProcMeminfo,
                         &cache_);
        break;
      case TelemetryItemEnum::kNumRunnableEntities:  // FALLTHROUGH
      case TelemetryItemEnum::kNumExistingEntities:
        ReadAndParseFile(root_dir_, "proc/loadavg", ParseDataFromProcLoadavg,
                         &cache_);
        break;
      case TelemetryItemEnum::kTotalIdleTimeUserHz:  // FALLTHROUGH
      case TelemetryItemEnum::kIdleTimePerCPUUserHz:
        ReadAndParseFile(root_dir_, "proc/stat", ParseDataFromProcStat,
                         &cache_);
        break;
      case TelemetryItemEnum::kDmiTables:  // FALLTHROUGH
      case TelemetryItemEnum::kHwmon:      // FALLTHROUGH
      case TelemetryItemEnum::kNetDev:     // FALLTHROUGH
      case TelemetryItemEnum::kNetStat:    // FALLTHROUGH
      case TelemetryItemEnum::kThermal:    // FALLTHROUGH
      case TelemetryItemEnum::kUptime:
        NOTIMPLEMENTED();
        break;
    }
  }

  return cache_.GetParsedData(item);
}

std::vector<std::pair<TelemetryItemEnum, base::Optional<base::Value>>>
Telemetry::GetGroup(TelemetryGroupEnum group, base::TimeDelta acceptable_age) {
  std::vector<std::pair<TelemetryItemEnum, base::Optional<base::Value>>>
      telem_items;

  for (TelemetryItemEnum item : group_map.at(group)) {
    telem_items.emplace_back(item, GetItem(item, acceptable_age));
  }

  return telem_items;
}

}  // namespace diagnostics
