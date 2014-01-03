// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This class is most definitely NOT re-entrant.

#include "login_manager/generator_job.h"

#include <errno.h>
#include <stdlib.h>
#include <unistd.h>

#include <string>
#include <vector>

#include <base/basictypes.h>
#include <base/file_path.h>
#include <base/logging.h>
#include <base/string_util.h>

#include "login_manager/system_utils.h"

namespace login_manager {

GeneratorJob::GeneratorJob(const std::vector<std::string>& arguments,
                           uid_t desired_uid,
                           SystemUtils* utils)
      : arguments_(arguments),
        system_(utils),
        subprocess_(desired_uid, system_) {
}

GeneratorJob::~GeneratorJob() {}

bool GeneratorJob::RunInBackground() {
  char const** argv = new char const*[arguments_.size() + 1];
  size_t index = CopyArgsToArgv(arguments_, argv);
  // Need to append NULL at the end.
  argv[index] = NULL;

  return subprocess_.ForkAndExec(argv);
}

void GeneratorJob::KillEverything(int signal, const std::string& message) {
  if (subprocess_.pid() < 0)
    return;
  subprocess_.KillEverything(signal);
}

void GeneratorJob::Kill(int signal, const std::string& message) {
  if (subprocess_.pid() < 0)
    return;
  subprocess_.Kill(signal);
}

const std::string GeneratorJob::GetName() const {
  FilePath exec_file(arguments_[0]);
  return exec_file.BaseName().value();
}

size_t GeneratorJob::CopyArgsToArgv(const std::vector<std::string>& arguments,
                                    char const** argv) const {
  for (size_t i = 0; i < arguments.size(); ++i) {
    size_t needed_space = arguments[i].length() + 1;
    char* space = new char[needed_space];
    strncpy(space, arguments[i].c_str(), needed_space);
    argv[i] = space;
  }
  return arguments.size();
}

}  // namespace login_manager
