// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "debugd/src/scheduler_configuration_tool.h"

#include "debugd/src/error_utils.h"
#include "debugd/src/process_with_output.h"
#include "debugd/src/sandboxed_process.h"

#include <string>

#include <base/files/file_util.h>
#include <base/logging.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/string_split.h>
#include <base/strings/stringprintf.h>
#include <brillo/errors/error_codes.h>
#include <build/build_config.h>
#include <build/buildflag.h>
#include <chromeos/dbus/service_constants.h>

namespace debugd {

namespace {

constexpr char kErrorPath[] =
    "org.chromium.debugd.SchedulerConfigurationPolicyError";

constexpr bool IsX86_64() {
#if defined(__x86_64__)
  return true;
#else
  return false;
#endif
}

// Executes a helper process with the expectation that any message printed to
// stderr indicates a failure that should be passed back over D-Bus.
// Returns false if any errors launching the process occur. Returns true
// otherwise, and sets |exit_status| if it isn't null.
bool RunHelper(const std::string& command,
               const ProcessWithOutput::ArgList& arguments,
               int* exit_status,
               brillo::ErrorPtr* error) {
  std::string helper_path;
  if (!SandboxedProcess::GetHelperPath(command, &helper_path)) {
    DEBUGD_ADD_ERROR(error, kErrorPath, "Path too long");
    return false;
  }

  // Note: This runs the helper as root and without a sandbox only because the
  // helper immediately drops privileges and enforces its own sandbox. debugd
  // should not be used to launch unsandboxed executables.
  std::string stderr;
  int result = ProcessWithOutput::RunProcess(
      helper_path, arguments, true /*requires_root*/,
      true /* disable_sandbox */, nullptr, nullptr, &stderr, error);

  if (!stderr.empty()) {
    DEBUGD_ADD_ERROR(error, kErrorPath, stderr.c_str());
    return false;
  }

  if (exit_status)
    *exit_status = result;
  return true;
}

}  // namespace

bool SchedulerConfigurationTool::SetPolicy(const std::string& policy,
                                           brillo::ErrorPtr* error) {
  if (!IsX86_64()) {
    DEBUGD_ADD_ERROR(error, kErrorPath, "Invalid architecture");
    return false;
  }

  int exit_status;
  bool result = RunHelper("scheduler_configuration_helper",
                          ProcessWithOutput::ArgList{"--policy=" + policy},
                          &exit_status, error);

  bool status = result && (exit_status == 0);
  if (!status) {
    DEBUGD_ADD_ERROR(error, kErrorPath,
                     "scheduler_configuration_helper failed");
  }

  return status;
}

}  // namespace debugd
