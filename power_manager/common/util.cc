// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/common/util.h"

#include <stdint.h>

#include <algorithm>
#include <cstdlib>
#include <string>

#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/format_macros.h>
#include <base/logging.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/string_util.h>
#include <base/strings/stringprintf.h>

#include "power_manager/common/power_constants.h"

namespace power_manager {
namespace util {

double ClampPercent(double percent) {
  return std::max(0.0, std::min(100.0, percent));
}

std::string TimeDeltaToString(base::TimeDelta delta) {
  std::string output;
  if (delta < base::TimeDelta())
    output += "-";

  int64_t total_seconds = llabs(delta.InSeconds());

  const int64_t hours = total_seconds / 3600;
  if (hours)
    output += base::StringPrintf("%" PRId64 "h", hours);

  const int64_t minutes = (total_seconds % 3600) / 60;
  if (minutes)
    output += base::StringPrintf("%" PRId64 "m", minutes);

  const int64_t seconds = total_seconds % 60;
  if (seconds || !total_seconds)
    output += base::StringPrintf("%" PRId64 "s", seconds);

  return output;
}

std::vector<base::FilePath> GetPrefPaths(const base::FilePath& read_write_path,
                                         const base::FilePath& read_only_path) {
  std::vector<base::FilePath> paths;
  paths.push_back(read_write_path);
  paths.push_back(read_only_path.Append(kBoardSpecificPrefsSubdir));
  paths.push_back(read_only_path);
  return paths;
}

bool WriteFileFully(const base::FilePath& filename,
                    const char* data,
                    int size) {
  return base::WriteFile(filename, data, size) == size;
}

}  // namespace util
}  // namespace power_manager
