// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/common/util.h"

#include <glib.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#include <string>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"

namespace {

// Path to program used to run code as root.
const char kSetuidHelperPath[] = "/usr/bin/powerd_setuid_helper";

}  // namespace

namespace power_manager {
namespace util {

bool OOBECompleted() {
  return access("/home/chronos/.oobe_completed", F_OK) == 0;
}

void Launch(const char* command) {
  LOG(INFO) << "Launching \"" << command << "\"";
  pid_t pid = fork();
  if (pid == 0) {
    // Detach from parent so that powerd doesn't need to wait around for us
    setsid();
    exit(fork() == 0 ? system(command) : 0);
  } else if (pid > 0) {
    waitpid(pid, NULL, 0);
  }
}

int Run(const char* command) {
  LOG(INFO) << "Running \"" << command << "\"";
  int return_value = system(command);
  if (return_value == -1) {
    LOG(ERROR) << "fork() failed";
  } else if (return_value) {
    return_value = WEXITSTATUS(return_value);
    LOG(ERROR) << "Command failed with " << return_value;
  }
  return return_value;
}

int RunSetuidHelper(const std::string& action,
                    const std::string& additional_args,
                    bool wait_for_completion) {
  std::string command = kSetuidHelperPath + std::string(" --action=" + action);
  if (!additional_args.empty())
    command += " " + additional_args;
  if (wait_for_completion) {
    return Run(command.c_str());
  } else {
    Launch(command.c_str());
    return 0;
  }
}

void CreateStatusFile(const base::FilePath& file) {
  if (!file_util::WriteFile(file, NULL, 0) == -1)
    LOG(ERROR) << "Unable to create " << file.value();
  else
    LOG(INFO) << "Created " << file.value();
}

void RemoveStatusFile(const base::FilePath& file) {
  if (file_util::PathExists(file)) {
    if (!file_util::Delete(file, false))
      LOG(ERROR) << "Unable to remove " << file.value();
    else
      LOG(INFO) << "Removed " << file.value();
  }
}

bool GetUintFromFile(const char* filename, unsigned int* value) {
  std::string buf;

  base::FilePath path(filename);
  if (!file_util::ReadFileToString(path, &buf)) {
    LOG(ERROR) << "Unable to read " << filename;
    return false;
  }
  TrimWhitespaceASCII(buf, TRIM_TRAILING, &buf);
  if (base::StringToUint(buf, value))
      return true;
  LOG(ERROR) << "Garbage found in " << filename << "( " << buf << " )";
  return false;
}

void RemoveTimeout(guint* timeout_id) {
  if (*timeout_id) {
    g_source_remove(*timeout_id);
    *timeout_id = 0;
  }
}

}  // namespace util
}  // namespace power_manager
