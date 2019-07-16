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

namespace {

template <typename T, bool (*StringToType)(const base::StringPiece&, T*)>
bool ReadIntegerFile(const base::FilePath& path, T* value_out) {
  DCHECK(value_out);

  std::string str;
  if (!base::ReadFileToString(path, &str)) {
    PLOG(ERROR) << "Unable to read from " << path.value();
    return false;
  }

  base::TrimWhitespaceASCII(str, base::TRIM_TRAILING, &str);
  if (!StringToType(str, value_out)) {
    LOG(ERROR) << "Unable to parse \"" << str << "\" from " << path.value();
    return false;
  }
  return true;
}

}  // namespace

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

bool WriteFileFully(const base::FilePath& filename,
                    const char* data,
                    int size) {
  return base::WriteFile(filename, data, size) == size;
}

bool WriteInt64File(const base::FilePath& path, int64_t value) {
  std::string buf = base::Int64ToString(value);
  if (!WriteFileFully(path, buf.data(), buf.size())) {
    PLOG(ERROR) << "Unable to write \"" << buf << "\" to " << path.value();
    return false;
  }
  return true;
}

auto ReadInt64File = ReadIntegerFile<int64_t, base::StringToInt64>;
auto ReadUint64File = ReadIntegerFile<uint64_t, base::StringToUint64>;
auto ReadHexUint32File = ReadIntegerFile<uint32_t, base::HexStringToUInt>;

std::string JoinPaths(const std::vector<base::FilePath>& paths,
                      const std::string& separator) {
  std::string str;
  for (const auto& path : paths)
    str += (str.empty() ? std::string() : separator) + path.value();
  return str;
}

}  // namespace util
}  // namespace power_manager
