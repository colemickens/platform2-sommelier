// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include <base/at_exit.h>
#include <base/logging.h>
#include <base/message_loop/message_loop.h>
#include <base/run_loop.h>

#include "vm_tools/syslog/collector.h"

int main(int argc, char** argv) {
  base::AtExitManager at_exit;

  if (argc > 1) {
    LOG(ERROR) << "Unexpected command line arguments";
    return 1;
  }

  base::MessageLoopForIO message_loop;

  std::unique_ptr<vm_tools::syslog::Collector> collector =
      vm_tools::syslog::Collector::Create();
  CHECK(collector);

  base::RunLoop().Run();

  return 0;
}
