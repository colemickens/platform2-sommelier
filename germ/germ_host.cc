// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "germ/germ_host.h"

#include <string>
#include <vector>

namespace germ {

int GermHost::Launch(LaunchRequest* request, LaunchResponse* response) {
  std::vector<std::string> argv;
  for (const auto& cmdline_token : request->command_line()) {
    argv.push_back(cmdline_token);
  }
  int status = launcher_.RunService(request->name(), argv);
  response->set_status(status);
  return 0;
}

}  // namespace germ
