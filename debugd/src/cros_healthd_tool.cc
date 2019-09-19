// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "debugd/src/cros_healthd_tool.h"

#include <fcntl.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/files/scoped_file.h>
#include <base/files/scoped_temp_dir.h>
#include <base/json/json_reader.h>
#include <base/logging.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/stringprintf.h>
#include <base/strings/string_split.h>
#include <base/values.h>
#include <brillo/errors/error_codes.h>
#include <build/build_config.h>
#include <build/buildflag.h>
#include <chromeos/dbus/service_constants.h>
#include <vboot/crossystem.h>

#include "debugd/src/error_utils.h"
#include "debugd/src/sandboxed_process.h"

#include <base/process/launch.h>
#include <re2/re2.h>
using base::ListValue;

namespace debugd {

namespace {
constexpr char kErrorPath[] = "org.chromium.debugd.CrosHealthdToolError";
constexpr char kSandboxDirPath[] = "/usr/share/policy/";
constexpr char kBinary[] = "/usr/libexec/diagnostics/cros_healthd_helper";
constexpr char kRunAs[] = "healthd_ec";
constexpr char kCrosHealthdSeccompPolicy[] = "ectool_i2cread-seccomp.policy";
bool CreateNonblockingPipe(base::ScopedFD* read_fd, base::ScopedFD* write_fd) {
  int pipe_fd[2];
  int ret = pipe2(pipe_fd, O_CLOEXEC | O_NONBLOCK);
  if (ret != 0) {
    PLOG(ERROR) << "Cannot create a pipe.";
    return false;
  }
  read_fd->reset(pipe_fd[0]);
  write_fd->reset(pipe_fd[1]);
  return true;
}

}  // namespace

bool CrosHealthdTool::GetManufactureDate(
    brillo::ErrorPtr* error, brillo::dbus_utils::FileDescriptor* outfd) {
  std::unique_ptr<brillo::Process> process;

  std::unique_ptr<SandboxedProcess> sandboxed_process{new SandboxedProcess()};

  const auto ectool_seccomp_path =
      base::FilePath{kSandboxDirPath}.Append(base::StringPrintf(
          "%s", kCrosHealthdSeccompPolicy));  // ectool seccomp policy

  if (!base::PathExists(ectool_seccomp_path)) {
    DEBUGD_ADD_ERROR(error, kErrorPath,
                     "Sandbox info is missing for this architecture");
    return false;
  }

  std::vector<std::string> parsed_args;

  // Minijail set up for cros_healthd in debugd.
  // Important to note that /dev/log needs to be mounted before /tmp

  parsed_args.push_back("-G");  // Inherit all the supplementary groups
  parsed_args.push_back("-c");
  parsed_args.push_back("cap_sys_rawio=e");

  parsed_args.push_back("-b");            // Bind mount cros_ec
  parsed_args.push_back("/dev/cros_ec");  // Bind mount cros_ec
  sandboxed_process->SandboxAs(kRunAs, kRunAs);
  sandboxed_process->SetSeccompFilterPolicyFile(
      ectool_seccomp_path.MaybeAsASCII());
  DLOG(INFO) << "Sandbox for ectool i2cread is ready";
  if (!sandboxed_process->Init(parsed_args)) {
    DEBUGD_ADD_ERROR(error, kErrorPath, "Process initialization failure.");
    return false;
  }
  process = std::move(sandboxed_process);
  base::ScopedFD read_fd, write_fd;
  if (!CreateNonblockingPipe(&read_fd, &write_fd)) {
    DEBUGD_ADD_ERROR(error, kErrorPath, "Cannot create a pipe.");
    return false;
  }

  process->AddArg(kBinary);
  process->BindFd(write_fd.get(), STDOUT_FILENO);
  process->Start();
  process->Release();
  *outfd = std::move(read_fd);
  return true;
}

}  // namespace debugd
