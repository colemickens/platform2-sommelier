// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/termina_manager_impl.h"

#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <sys/mount.h>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <base/files/file_util.h>
#include <brillo/process.h>

#include "login_manager/system_utils.h"

using base::FilePath;

namespace login_manager {

namespace {

const char kVmLauncherPath[] = "/usr/bin/vm_launcher";
const char kVmToolAllVms[] = "all";
const char kVmToolContainerOpt[] = "--container";
const char kVmToolRwContainerOpt[] = "--rwcontainer";
const char kVmToolForceOpt[] = "--force";
const char kVmToolGetName[] = "getname";
const char kVmToolStart[] = "start";
const char kVmToolStop[] = "stop";

bool VmEnabled() {
  return base::PathExists(base::FilePath(kVmLauncherPath));
}

std::string VmNameFromPid(int pid) {
  brillo::ProcessImpl vmtool;
  vmtool.AddArg(std::string(kVmLauncherPath));
  vmtool.AddArg(std::string(kVmToolGetName));
  vmtool.AddArg(std::to_string(pid));
  vmtool.RedirectUsingPipe(STDOUT_FILENO, false /* is_input */);
  vmtool.Start();
  int stdout_pipe = vmtool.GetPipe(STDOUT_FILENO);
  char stdout_buff[256];
  ssize_t ret = read(stdout_pipe, &stdout_buff, sizeof(stdout_buff) - 1);
  if (ret < 0)
    return std::string();
  stdout_buff[ret] = '\0';
  if (vmtool.Wait() < 0)
    return std::string();

  return stdout_buff;
}

void CleanUpVm(const std::string& name) {
  brillo::ProcessImpl vmtool;
  vmtool.AddArg(std::string(kVmLauncherPath));
  vmtool.AddArg(std::string(kVmToolStop));
  vmtool.AddArg(name);
  vmtool.Run();
}

}  // anonymous namespace

TerminaManagerImpl::TerminaManagerImpl(SystemUtils* system_utils)
    : system_utils_(system_utils) {
  DCHECK(system_utils_);
}

bool TerminaManagerImpl::IsManagedJob(pid_t pid) {
  return VmEnabled() && !VmNameFromPid(pid).empty();
}

void TerminaManagerImpl::HandleExit(const siginfo_t& status) {
  if (!VmEnabled())
    return;

  std::string vm_name = VmNameFromPid(status.si_pid);
  if (vm_name.empty())
    return;

  CleanUpVm(vm_name);
}

void TerminaManagerImpl::RequestJobExit(const std::string& reason) {
  if (!VmEnabled())
    return;

  CleanUpVm(kVmToolAllVms);
}

void TerminaManagerImpl::EnsureJobExit(base::TimeDelta timeout) {
  if (!VmEnabled())
    return;

  brillo::ProcessImpl vmtool;
  vmtool.AddArg(kVmLauncherPath);
  vmtool.AddArg(kVmToolStop);
  vmtool.AddArg(kVmToolForceOpt);
  vmtool.AddArg(kVmToolAllVms);
  vmtool.Run();
}

bool TerminaManagerImpl::StartVmContainer(const base::FilePath& image_path,
                                          const std::string& name,
                                          bool writable) {
  LOG(INFO) << "Starting container " << image_path.value() << " in termina VM "
            << name;

  brillo::ProcessImpl vmtool;
  vmtool.AddArg(kVmLauncherPath);
  vmtool.AddArg(kVmToolStart);
  vmtool.AddArg(base::StringPrintf(
      "%s=%s", writable ? kVmToolRwContainerOpt : kVmToolContainerOpt,
      image_path.value().c_str()));
  vmtool.AddArg(name);

  return vmtool.Run() == 0;
}

bool TerminaManagerImpl::StopVmContainer(const std::string& name) {
  LOG(INFO) << "Stopping termina VM " << name;

  brillo::ProcessImpl vmtool;
  vmtool.AddArg(kVmLauncherPath);
  vmtool.AddArg(kVmToolStop);
  vmtool.AddArg(name);

  return vmtool.Run() == 0;
}

}  // namespace login_manager
