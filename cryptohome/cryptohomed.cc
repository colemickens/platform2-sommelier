// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "service.h"

#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <sys/ucontext.h>

#include <base/at_exit.h>
#include <base/command_line.h>
#include <base/logging.h>
#include <chaps/pkcs11/cryptoki.h>
#include <chromeos/syslog_logging.h>
#include <dbus/dbus.h>
#include <glib.h>

#include "platform.h"

// TODO(wad) This is a placeholder DBus service which allows
//           chrome-login (and anything else running as chronos)
//           to request to mount, unmount, or check if a mapper
//           device is mounted. This is very temporary but should
//           serve as a baseline for moving all the shell scripts
//           into C++.
//           We will need a "CheckKey" interface as well to simplify
//           offline authentication checks.

namespace {

void HandlePipeSignal(int signal, siginfo_t* info, void* context_ptr) {
  uint64_t source = 0;
#if defined(__x86_64__)
  ucontext_t* context = reinterpret_cast<ucontext_t*>(context_ptr);
  source = context->uc_mcontext.gregs[REG_RIP];
#endif
  LOG(FATAL) << "SIGPIPE received from 0x" << std::hex << source;
}

}

namespace switches {
// Keeps std* open for debugging
static const char *kNoCloseOnDaemonize = "noclose";
static const char *kNoLegacyMount = "nolegacymount";
}  // namespace switches

int main(int argc, char **argv) {
  ::g_type_init();
  base::AtExitManager exit_manager;
  CommandLine::Init(argc, argv);

  chromeos::InitLog(chromeos::kLogToSyslog | chromeos::kLogToStderr);

  // Add a handler to do something informative on SIGPIPE.
  // TODO(dkrahn): Remove this code once crbug.com/367278 is fixed.
  struct sigaction pipe_action;
  memset(&pipe_action, 0, sizeof(struct sigaction));
  pipe_action.sa_flags = SA_SIGINFO;
  pipe_action.sa_sigaction = &HandlePipeSignal;
  CHECK(!sigaction(SIGPIPE, &pipe_action, NULL))
      << "Failed to register SIGPIPE handler.";

  // Allow the commands to be configurable.
  CommandLine *cl = CommandLine::ForCurrentProcess();
  int noclose = cl->HasSwitch(switches::kNoCloseOnDaemonize);
  bool nolegacymount = cl->HasSwitch(switches::kNoLegacyMount);
  PLOG_IF(FATAL, daemon(0, noclose) == -1) << "Failed to daemonize";

  // Setup threading. This needs to be called before other calls into glib and
  // before multiple threads are created that access dbus.
  dbus_threads_init_default();

  // Initialize OpenSSL.
  OpenSSL_add_all_algorithms();

  cryptohome::Platform platform;
  cryptohome::Service service;

  service.set_legacy_mount(!nolegacymount);

  if (!service.Initialize()) {
    LOG(FATAL) << "Service initialization failed";
    return 1;
  }

  if (!service.Register(chromeos::dbus::GetSystemBusConnection())) {
    LOG(FATAL) << "DBUS service registration failed";
    return 1;
  }

  if (!service.Run()) {
    LOG(FATAL) << "Service run failed.";
    return 1;
  }

  // If PKCS #11 was initialized, this will tear it down.
  C_Finalize(NULL);
  return 0;
}
