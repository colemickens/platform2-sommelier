// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/common/util.h"

#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#include <string>

#include "base/basictypes.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/format_macros.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "power_manager/common/power_constants.h"

namespace power_manager {
namespace util {

void Launch(const std::string& command) {
  LOG(INFO) << "Launching \"" << command << "\"";
  pid_t pid = fork();
  if (pid == 0) {
    // Detach from parent so that powerd doesn't need to wait around for us
    setsid();
    exit(fork() == 0 ? system(command.c_str()) : 0);
  } else if (pid > 0) {
    waitpid(pid, NULL, 0);
  }
}

int Run(const std::string& command) {
  LOG(INFO) << "Running \"" << command << "\"";
  int return_value = system(command.c_str());
  if (return_value == -1) {
    LOG(ERROR) << "fork() failed";
  } else if (return_value) {
    return_value = WEXITSTATUS(return_value);
    LOG(ERROR) << "Command failed with " << return_value;
  }
  return return_value;
}

double ClampPercent(double percent) {
  return std::max(0.0, std::min(100.0, percent));
}

std::string TimeDeltaToString(base::TimeDelta delta) {
  std::string output;
  if (delta < base::TimeDelta())
    output += "-";

  int64 total_seconds = llabs(delta.InSeconds());

  const int64 hours = total_seconds / 3600;
  if (hours)
    output += base::StringPrintf("%" PRId64 "h", hours);

  const int64 minutes = (total_seconds % 3600) / 60;
  if (minutes)
    output += base::StringPrintf("%" PRId64 "m", minutes);

  const int64 seconds = total_seconds % 60;
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

}  // namespace util
}  // namespace power_manager
