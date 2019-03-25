// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "debugd/src/probe_tool.h"

#include "debugd/src/error_utils.h"
#include "debugd/src/sandboxed_process.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <base/files/file_util.h>
#include <base/files/scoped_temp_dir.h>
#include <base/files/file_path.h>
#include <base/logging.h>
#include <base/values.h>
#include <base/json/json_reader.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/string_split.h>
#include <base/strings/stringprintf.h>
#include <brillo/errors/error_codes.h>
#include <build/build_config.h>
#include <build/buildflag.h>
#include <chromeos/dbus/service_constants.h>
#include <vboot/crossystem.h>

using base::ListValue;

namespace debugd {

namespace {
constexpr char kErrorPath[] = "org.chromium.debugd.RunProbeFunctionError";
constexpr char kSandboxInfoDir[] = "/etc/runtime_probe/sandbox";
constexpr char kBinary[] = "/usr/bin/runtime_probe_helper";
constexpr char kRunAs[] = "runtime_probe";

}  // namespace

bool ProbeTool::EvaluateProbeFunction(brillo::ErrorPtr* error,
                                      const std::string& sandbox_info,
                                      const std::string& probe_statement,
                                      std::string* probe_result,
                                      int32_t* exit_code) {
  std::unique_ptr<brillo::Process> process;

  // Details of sandboxing for probing should be centralized in a single
  // directory. Sandboxing is mandatory when we don't allow debug features.
  if (VbGetSystemPropertyInt("cros_debug") != 1) {
    std::unique_ptr<SandboxedProcess> sandboxed_process(new SandboxedProcess());

    const auto seccomp_path = base::FilePath{kSandboxInfoDir}.Append(
        base::StringPrintf("%s-seccomp.policy", sandbox_info.c_str()));
    const auto minijail_args_path = base::FilePath{kSandboxInfoDir}.Append(
        base::StringPrintf("%s.args", sandbox_info.c_str()));

    if (!base::PathExists(seccomp_path) ||
        !base::PathExists(minijail_args_path)) {
      DEBUGD_ADD_ERROR(error, kErrorPath,
                       "Sandbox info is missing for this architecture");
      return false;
    }

    // Read and parse the arguments, use JSON to avoid quote escaping.
    std::string minijail_args_str;
    if (!base::ReadFileToString(base::FilePath(minijail_args_path),
                                &minijail_args_str)) {
      DEBUGD_ADD_ERROR(error, kErrorPath, "Failed to load minijail arguments");
      return false;
    }

    DLOG(INFO) << "minijail arguments : " << minijail_args_str;

    // Parse the JSON formatted minijail arguments.
    std::unique_ptr<ListValue> minijail_args =
        ListValue::From(base::JSONReader().ReadToValue(minijail_args_str));

    if (!minijail_args) {
      DEBUGD_ADD_ERROR(error, kErrorPath,
                       "minijail args are not stored in list");
      return false;
    }
    std::vector<std::string> parsed_args;

    // The following is the general minijail set up for runtime_probe in debugd
    // /dev/log needs to be bind mounted before any possible tmpfs mount on run
    parsed_args.push_back("-G");  // Inherit all the supplementary groups

    // Run the process inside the new VFS namespace. The root of VFS is mounted
    // on /var/empty/
    parsed_args.push_back("-P");
    parsed_args.push_back("/mnt/empty");

    parsed_args.push_back("-b");        // Bind mount rootfs
    parsed_args.push_back("/");         // Bind mount rootfs
    parsed_args.push_back("-b");        // Bind mount /proc
    parsed_args.push_back("/proc");     // Bind mount /proc
    parsed_args.push_back("-b");        // Enable logging in minijail
    parsed_args.push_back("/dev/log");  // Enable logging in minijail
    parsed_args.push_back("-t");        // Bind mount /tmp
    parsed_args.push_back("-r");        // Remount /proc read-only
    parsed_args.push_back("-d");        // Mount a new /dev with minimum nodes

    std::string tmp_str;
    for (size_t i = 0; i < minijail_args->GetSize(); ++i) {
      if (!minijail_args->GetString(i, &tmp_str)) {
        DEBUGD_ADD_ERROR(error, kErrorPath,
                         "Failed to parse minijail arguments");
        return false;
      }
      parsed_args.push_back(tmp_str);
    }

    sandboxed_process->SandboxAs(kRunAs, kRunAs);
    sandboxed_process->SetSeccompFilterPolicyFile(seccomp_path.MaybeAsASCII());
    DLOG(INFO) << "Sandbox for " << sandbox_info << " is ready";
    if (!sandboxed_process->Init(parsed_args)) {
      DEBUGD_ADD_ERROR(error, kErrorPath, "Process initialization failure.");
      return false;
    }
    process = std::move(sandboxed_process);
  } else {
    process = std::make_unique<brillo::ProcessImpl>();
    // Explicitly running it without sandboxing.
    LOG(ERROR) << "Running " << sandbox_info << " without sandbox";
  }

  process->AddArg(kBinary);
  process->AddArg(probe_statement);

  base::ScopedTempDir temp_dir;
  CHECK(temp_dir.CreateUniqueTempDir())
      << "Fail to create unique temporary directory.";
  std::string output_file = temp_dir.GetPath().Append(kRunAs).value();
  process->RedirectOutput(output_file);
  // TODO(itspeter): switch to Start() so to allow time consuming probe
  // statement and possibility of multiprocesing. (b/129508946)
  *exit_code = static_cast<int32_t>(process->Run());
  base::ReadFileToString(base::FilePath(output_file), probe_result);
  if (*exit_code) {
    DEBUGD_ADD_ERROR(
        error, kErrorPath,
        base::StringPrintf("Helper returns non-zero (%d)", *exit_code));
    return false;
  }
  return true;
}

}  // namespace debugd
