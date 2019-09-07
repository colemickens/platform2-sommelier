// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crash-reporter/generic_failure_collector.h"

#include <base/files/file_util.h>
#include <base/logging.h>

#include "crash-reporter/util.h"

namespace {
const char kSignatureKey[] = "sig";
}  // namespace

using base::FilePath;

const char* const GenericFailureCollector::kGenericFailure = "generic-failure";
const char* const GenericFailureCollector::kSuspendFailure = "suspend-failure";

GenericFailureCollector::GenericFailureCollector()
    : GenericFailureCollector(kGenericFailure) {}

GenericFailureCollector::GenericFailureCollector(const std::string& exec_name)
    : CrashCollector("generic_failure"),
      failure_report_path_("/dev/stdin"),
      exec_name_(exec_name) {}

GenericFailureCollector::~GenericFailureCollector() {}

bool GenericFailureCollector::LoadGenericFailure(std::string* content,
                                                 std::string* signature) {
  FilePath failure_report_path(failure_report_path_.c_str());
  if (!base::ReadFileToString(failure_report_path, content)) {
    LOG(ERROR) << "Could not open " << failure_report_path.value();
    return false;
  }

  std::string::size_type end_position = content->find('\n');
  if (end_position == std::string::npos) {
    LOG(ERROR) << "unexpected generic failure format";
    return false;
  }
  *signature = content->substr(0, end_position);
  return true;
}

bool GenericFailureCollector::Collect() {
  std::string reason = "normal collection";
  bool feedback = true;
  if (util::IsDeveloperImage()) {
    reason = "always collect from developer builds";
    feedback = true;
  } else if (!is_feedback_allowed_function_()) {
    reason = "no user consent";
    feedback = false;
  }

  LOG(INFO) << "Processing generic failure: " << reason;

  if (!feedback) {
    return true;
  }

  std::string generic_failure;
  std::string failure_signature;
  if (!LoadGenericFailure(&generic_failure, &failure_signature)) {
    return true;
  }

  FilePath crash_directory;
  if (!GetCreatedCrashDirectoryByEuid(kRootUid, &crash_directory, nullptr)) {
    return true;
  }

  std::string dump_basename = FormatDumpBasename(exec_name_, time(nullptr), 0);
  FilePath log_path = GetCrashPath(crash_directory, dump_basename, "log");
  FilePath meta_path = GetCrashPath(crash_directory, dump_basename, "meta");

  AddCrashMetaData(kSignatureKey, failure_signature);

  bool result = GetLogContents(log_config_path_, exec_name_, log_path);
  if (result) {
    FinishCrash(meta_path, exec_name_, log_path.BaseName().value());
  }

  return true;
}
