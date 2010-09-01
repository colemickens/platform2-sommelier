// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gflags/gflags.h>
#include <time.h>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "cros/chromeos_cros_api.h"
#include "power_manager/powerman.h"

using std::string;

DEFINE_string(log_dir, "",
              "Directory to store logs.");

// Returns true on success.
static bool SetUpLogSymlink(const string& symlink_path,
                            const string& log_basename) {
  unlink(symlink_path.c_str());
  if (symlink(log_basename.c_str(), symlink_path.c_str()) == -1) {
    return false;
  }
  return true;
}

static string GetTimeAsString(time_t utime) {
  struct tm tm;
  CHECK(localtime_r(&utime, &tm) == &tm);
  char str[16];
  CHECK(strftime(str, sizeof(str), "%Y%m%d-%H%M%S", &tm) == 15);
  return string(str);
}


int main(int argc, char* argv[]) {
  g_type_init();
  google::ParseCommandLineFlags(&argc, &argv, true);

  CHECK(!FLAGS_log_dir.empty()) << "--log_dir is required";
  CHECK(argc == 1) << "Unexpected arguments. Try --help";
  CommandLine::Init(argc, argv);

  const string log_latest =
      StringPrintf("%s/powerm.LATEST", FLAGS_log_dir.c_str());
  const string log_basename =
      StringPrintf("powerm.%s", GetTimeAsString(::time(NULL)).c_str());
  const string log_path = FLAGS_log_dir + "/" + log_basename;
  CHECK(SetUpLogSymlink(log_latest, log_basename));
  logging::InitLogging(log_path.c_str(),
                       logging::LOG_ONLY_TO_FILE,
                       logging::DONT_LOCK_LOG_FILE,
                       logging::APPEND_TO_OLD_LOG_FILE);
  power_manager::PowerManDaemon daemon(true, true);
  daemon.Init();
  daemon.Run();
  return 0;
}
