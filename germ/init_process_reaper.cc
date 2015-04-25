// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "germ/init_process_reaper.h"

#include <sys/types.h>
#include <sys/wait.h>

#include <base/bind.h>
#include <base/callback.h>
#include <base/posix/eintr_wrapper.h>

namespace germ {

InitProcessReaper::InitProcessReaper(base::Closure quit_closure)
    : quit_closure_(quit_closure) {}
InitProcessReaper::~InitProcessReaper() {}

void InitProcessReaper::HandleReapedChild(const siginfo_t& info) {
  LOG(INFO) << "Process " << info.si_pid << " terminated with status "
            << info.si_status << " (code = " << info.si_code << ")";

  if (NoUnwaitedForChildren()) {
    LOG(INFO) << "No more children, exiting.";
    quit_closure_.Run();
  }
}

bool InitProcessReaper::NoUnwaitedForChildren() {
  siginfo_t info;
  const int rc =
      HANDLE_EINTR(waitid(P_ALL, 0, &info, WEXITED | WNOHANG | WNOWAIT));
  return rc == -1 && errno == ECHILD;
}

}  // namespace germ
