// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "oobe_config/utils.h"

#include <string>
#include <vector>

#include <base/strings/string_util.h>
#include <brillo/process.h>

using std::string;
using std::vector;

namespace oobe_config {

int RunCommand(const vector<string>& command) {
  LOG(INFO) << "Command: " << base::JoinString(command, " ");

  brillo::ProcessImpl proc;
  for (const auto& arg : command) {
    proc.AddArg(arg);
  }
  return proc.Run();
}

}  // namespace oobe_config
