// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/android_container_manager_impl.h"

#include <string>

#include <base/files/file_util.h>
#include <brillo/process.h>

namespace login_manager {

namespace {

constexpr char kShutdownMetricsFileName[] = "/var/lib/metrics/Arc.ShutdownTime";

}  // anonymous namespace

AndroidContainerManagerImpl::AndroidContainerManagerImpl(
    SystemUtils* system_utils,
    const base::FilePath& containers_directory,
    const std::string& name)
    : ContainerManagerImpl(system_utils, containers_directory, name) {}

bool AndroidContainerManagerImpl::RequestTermination() {
  shutdown_start_time_ = base::TimeTicks::Now();
  brillo::ProcessImpl shutdown_process;
  shutdown_process.AddArg("/usr/sbin/android-sh");
  shutdown_process.AddStringOption("-c", "setprop sys.powerctl shutdown");

  if (shutdown_process.Run() != 0) {
    LOG(ERROR) << "setprop sys.powerctl shutdown failed";
    return false;
  }

  return true;
}

void AndroidContainerManagerImpl::OnContainerStopped(bool clean) {
  if (!clean)
    return;

  std::string shutdown_msec = std::to_string(
      (base::TimeTicks::Now() - shutdown_start_time_).InMilliseconds());

  if (base::WriteFile(base::FilePath(kShutdownMetricsFileName),
                      shutdown_msec.c_str(), shutdown_msec.size()) !=
      static_cast<int>(shutdown_msec.size())) {
    PLOG(ERROR) << "Failed to write to shutdown metrics file";
  }
}

}  // namespace login_manager
