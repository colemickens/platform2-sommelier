// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "init/file_attrs_cleaner.h"

#include <sysexits.h>

#include <string>
#include <vector>

#include <base/command_line.h>
#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/logging.h>
#include <brillo/flag_helper.h>
#include <brillo/syslog_logging.h>
#include <metrics/metrics_library.h>

using file_attrs_cleaner::ScanDir;

int main(int argc, char* argv[]) {
  DEFINE_string(skip_dir, "", "Subdirectory name to skip.");
  DEFINE_bool(enable_metrics, false, "Report URL xattr metrics.");
  brillo::FlagHelper::Init(argc, argv, "Chromium OS File Attrs Cleaner");
  brillo::InitLog(brillo::kLogToSyslog | brillo::kLogToStderrIfTty);
  MetricsLibrary metrics;

  if (argc <= 1) {
    LOG(ERROR) << "Need at least one directory to scan.";
    return EX_USAGE;
  }

  std::vector<std::string> skip_recurse;
  if (!FLAGS_skip_dir.empty())
    skip_recurse.push_back(FLAGS_skip_dir);

  auto args = base::CommandLine::ForCurrentProcess()->GetArgs();
  bool ret = true;
  int url_xattrs_count = 0;
  for (const auto& arg : args) {
    if (base::DirectoryExists(base::FilePath(arg))) {
      ret &= ScanDir(arg, skip_recurse, &url_xattrs_count);
    } else {
      LOG(ERROR) << "Directory '" << arg << "' does not exist.";
      ret = false;
    }
  }

  if (FLAGS_enable_metrics) {
    constexpr int min = 1;
    constexpr int max = 1000;
    constexpr int nbuckets = 10;
    if (!metrics.SendToUMA("ChromeOS.UrlXattrsCount", url_xattrs_count, min,
                           max, nbuckets)) {
      LOG(ERROR) << "Failed to send |url_xattrs_count| to UMA.";
    }
  }

  return ret ? EX_OK : 1;
}
