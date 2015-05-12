// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "germ/germ_init.h"

#include <grp.h>
#include <signal.h>
#include <sys/types.h>
#include <sysexits.h>
#include <unistd.h>

#include <string>
#include <vector>

#include <base/bind.h>
#include <base/bind_helpers.h>
#include <base/location.h>
#include <base/logging.h>
#include <base/message_loop/message_loop.h>
#include <chromeos/daemons/daemon.h>

#include "germ/init_process_reaper.h"
#include "germ/proto_bindings/soma_container_spec.pb.h"

namespace germ {

GermInit::GermInit(const soma::ContainerSpec& spec)
    : init_process_reaper_(base::Bind(&GermInit::Quit, base::Unretained(this))),
      spec_(spec) {}
GermInit::~GermInit() {}

int GermInit::OnInit() {
  init_process_reaper_.RegisterWithDaemon(this);

  int return_code = chromeos::Daemon::OnInit();
  if (return_code != 0) {
    LOG(ERROR) << "Error initializing chromeos::Daemon";
    return return_code;
  }

  // It is important that we start all processes in a single task, since
  // otherwise |init_process_reaper_| might cause us to exit after only some of
  // the processes have exited. This is because InitProcessReaper's behavior is:
  // after reaping a child, if we have no more children, then exit. Thus, we
  // need to ensure that it never reaps a process while we're still in the
  // middle of starting them.
  CHECK(base::MessageLoop::current()->task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&GermInit::StartProcesses, base::Unretained(this))));
  return EX_OK;
}

void GermInit::StartProcesses() {
  size_t i = 0;
  for (const auto& executable : spec_.executables()) {
    const pid_t pid = fork();
    PCHECK(pid != -1) << "fork() failed: " << spec_.name() << "executable "
                      << i;

    if (pid == 0) {
      sigset_t mask;
      PCHECK(sigemptyset(&mask) == 0);
      PCHECK(sigprocmask(SIG_SETMASK, &mask, nullptr) == 0);

      launcher_.ExecveInMinijail(executable);
      LOG(FATAL) << "execve() failed: " << spec_.name() << " executable " << i;
    }

    ++i;
  }
}

}  // namespace germ
