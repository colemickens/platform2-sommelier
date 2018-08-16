// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crash-reporter/anomaly_collector.h"

#include <pwd.h>
#include <unistd.h>

#include <string>

#include <brillo/flag_helper.h>
#include <brillo/syslog_logging.h>

namespace {

// Set up all the state dirs.
void Initialize() {
  if (0) {
    // TODO(semenzato): put this back in once we decide it's safe
    // to make /var/spool/crash rwxrwxrwx root, or use a different
    // owner and setuid for the crash reporter as well.
    struct passwd* user;

    // Get low privilege uid, gid.
    user = getpwnam("chronos");  // NOLINT(runtime/threadsafe_fn)
    CHECK_NE(nullptr, user);

    // Drop privileges.
    CHECK_EQ(0, setuid(user->pw_uid));
  }
}

}  // namespace

int main(int argc, char* argv[]) {
  DEFINE_bool(filter, false, "Input is stdin and output is stdout");
  DEFINE_bool(test, false, "Run self-tests");

  brillo::FlagHelper::Init(argc, argv, "Crash Helper: Anomaly Collector");
  brillo::OpenLog("anomaly_collector", true);
  brillo::InitLog(brillo::kLogToSyslog | brillo::kLogToStderrIfTty);

  if (!FLAGS_filter && !FLAGS_test)
    Initialize();

  return AnomalyLexer(FLAGS_filter, FLAGS_test);
}
