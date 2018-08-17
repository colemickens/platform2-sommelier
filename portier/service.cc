// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/at_exit.h>
#include <base/logging.h>
#include <brillo/syslog_logging.h>

#include "portier/portierd.h"

using std::string;

using portier::Portierd;

int main(int argc, char** argv) {
  brillo::InitLog(brillo::kLogToStderrIfTty);

  auto service = Portierd::Create();
  service->Run();
  return 0;
}
