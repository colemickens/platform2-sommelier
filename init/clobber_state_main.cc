// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "init/clobber_state.h"

#include <unistd.h>

#include <memory>

#include <base/logging.h>

#include "init/crossystem_impl.h"

int main(int argc, char* argv[]) {
  logging::LoggingSettings settings;
  settings.logging_dest = logging::LOG_TO_FILE;
  settings.log_file = "/tmp/clobber-state.log";
  // All logging happens in the main thread, so there is no need to lock the log
  // file.
  settings.lock_log = logging::DONT_LOCK_LOG_FILE;
  settings.delete_old = logging::DELETE_OLD_LOG_FILE;
  logging::InitLogging(settings);

  if (getuid() != 0) {
    LOG(ERROR) << "clobber-state must be run as root";
    return 1;
  }

  std::unique_ptr<CrosSystemImpl> cros_system =
      std::make_unique<CrosSystemImpl>();
  ClobberState::Arguments args = ClobberState::ParseArgv(argc, argv);
  ClobberState clobber(args, cros_system.get());
  return clobber.Run();
}
