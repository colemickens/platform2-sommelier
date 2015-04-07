// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "germ/germ_host.h"

#include <string>
#include <vector>

#include <base/logging.h>

namespace germ {

int GermHost::Launch(LaunchRequest* request, LaunchResponse* response) {
  std::vector<std::string> argv;
  for (const auto& cmdline_token : request->spec().command_line()) {
    argv.push_back(cmdline_token);
  }
  pid_t pid = -1;
  bool success = launcher_.RunDaemonized(request->name(), argv, &pid);
  if (!success) {
    LOG(ERROR) << "RunDaemonized(" << request->name() << ") failed";
    response->set_pid(-1);
    return -1;
  }
  response->set_pid(pid);
  return 0;
}

}  // namespace germ
