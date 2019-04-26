// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DIAGNOSTICS_TELEM_TELEMETRY_H_
#define DIAGNOSTICS_TELEM_TELEMETRY_H_

#include <string>
#include <utility>
#include <vector>

#include <base/files/file_path.h>
#include <base/macros.h>
#include <base/optional.h>
#include <base/time/time.h>
#include <base/values.h>

#include "diagnostics/telem/telem_cache.h"
#include "diagnostics/telem/telemetry_group_enum.h"
#include "diagnostics/telem/telemetry_item_enum.h"

namespace diagnostics {

// Holds a dump of a file's contents.
struct FileDump {
  // The "asked for" pathname of the file.
  std::string path;

  // Real path to the file. Unlike |path|, this path never contains
  // symbolic links.
  std::string real_path;

  // Contents of the file.
  std::string contents;
};

typedef std::vector<FileDump> FileDumps;

// Libtelem's main interface for requesting telemetry information.
//
// Example usage:
//   Telemetry telemetry;
//   const base::Value* memtotal_mb =
//     telemetry.GetItem(TelemetryItemEnum::kMemTotalMB);
class Telemetry final {
 public:
  Telemetry();

  // For testing purposes.
  explicit Telemetry(const base::FilePath& root_dir);
  ~Telemetry();

  // Returns telemetry data corresponding to |item|, which was updated at least
  // |acceptable_age| ago. The returned value should be checked before it is
  // used - the function will return base::nullopt if the requested item could
  // not be retrieved.
  base::Optional<base::Value> GetItem(TelemetryItemEnum item,
                                      base::TimeDelta acceptable_age);

  // Returns telemetry data for each item in |group|, which was updated at least
  // |acceptable_age| ago.
  std::vector<std::pair<TelemetryItemEnum, base::Optional<base::Value>>>
  GetGroup(TelemetryGroupEnum group, base::TimeDelta acceptable_age);

 private:
  base::FilePath root_dir_{"/"};
  TelemCache cache_;

  DISALLOW_COPY_AND_ASSIGN(Telemetry);
};

}  // namespace diagnostics

#endif  // DIAGNOSTICS_TELEM_TELEMETRY_H_
