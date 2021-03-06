// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>

#include <base/at_exit.h>
#include <base/files/file_descriptor_watcher_posix.h>
#include <base/logging.h>
#include <base/message_loop/message_loop.h>
#include <base/run_loop.h>
#include <brillo/syslog_logging.h>

#include "vm_tools/seneschal/service.h"

int main(int argc, char** argv) {
  base::AtExitManager at_exit;
  brillo::InitLog(brillo::kLogToSyslog | brillo::kLogToStderrIfTty);

  if (argc != 1) {
    LOG(ERROR) << "Unexpected command line arguments";
    return EXIT_FAILURE;
  }

  base::MessageLoopForIO message_loop;
  base::FileDescriptorWatcher watcher(&message_loop);
  base::RunLoop run_loop;

  auto service = vm_tools::seneschal::Service::Create(run_loop.QuitClosure());

  CHECK(service);

  run_loop.Run();

  return EXIT_SUCCESS;
}
