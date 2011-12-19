// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include <base/at_exit.h>
#include <base/basictypes.h>
#include <base/command_line.h>
#include <crypto/rsa_private_key.h>
#include <base/file_util.h>
#include <base/logging.h>

#include "login_manager/keygen_worker.h"
#include "login_manager/nss_util.h"
#include "login_manager/owner_key.h"
#include "login_manager/system_utils.h"

using std::string;
using login_manager::NssUtil;
using login_manager::OwnerKey;

namespace switches {

// Name of the flag that determines the path to log file.
static const char kLogFile[] = "log-file";
// The default path to the log file.
static const char kDefaultLogFile[] = "/var/log/session_manager";

}  // namespace switches

int main(int argc, char* argv[]) {
  base::AtExitManager exit_manager;
  CommandLine::Init(argc, argv);
  CommandLine* cl = CommandLine::ForCurrentProcess();
  string log_file = cl->GetSwitchValueASCII(switches::kLogFile);
  if (log_file.empty())
    log_file.assign(switches::kDefaultLogFile);
  logging::InitLogging(log_file.c_str(),
                       logging::LOG_TO_BOTH_FILE_AND_SYSTEM_DEBUG_LOG,
                       logging::DONT_LOCK_LOG_FILE,
                       logging::APPEND_TO_OLD_LOG_FILE,
                       logging::DISABLE_DCHECK_FOR_NON_OFFICIAL_RELEASE_BUILDS);

  if (cl->args().size() != 1) {
    LOG(FATAL) << "Usage: keygen /path/to/output_file";
  }
  return login_manager::keygen::generate(cl->args()[0]);
}
