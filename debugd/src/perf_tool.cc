// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "debugd/src/perf_tool.h"

#include <unistd.h>

#include <base/bind.h>
#include <base/strings/stringprintf.h>

#include "debugd/src/process_with_output.h"

using base::StringPrintf;

namespace debugd {

namespace {

const char kUnsupportedPerfToolErrorName[] =
    "org.chromium.debugd.error.UnsupportedPerfTool";
const char kProcessErrorName[] = "org.chromium.debugd.error.RunProcess";

// Location of quipper on ChromeOS.
const char kQuipperLocation[] = "/usr/bin/quipper";

enum PerfSubcommand {
  PERF_COMMAND_RECORD,
  PERF_COMMAND_STAT,
  PERF_COMMAND_MEM,
  PERF_COMMAND_UNSUPPORTED,
};

// Returns one of the above enums given an vector of perf arguments, starting
// with "perf" itself in |args[0]|.
PerfSubcommand GetPerfSubcommandType(const std::vector<std::string>& args) {
  if (args[0] == "perf" && args.size() > 1) {
    if (args[1] == "record")
      return PERF_COMMAND_RECORD;
    if (args[1] == "stat")
      return PERF_COMMAND_STAT;
    if (args[1] == "mem")
      return PERF_COMMAND_MEM;
  }

  return PERF_COMMAND_UNSUPPORTED;
}

void AddQuipperArguments(brillo::Process* process,
                         const uint32_t duration_secs,
                         const std::vector<std::string>& perf_args) {
  process->AddArg(kQuipperLocation);
  process->AddArg(StringPrintf("%u", duration_secs));
  for (const auto& arg : perf_args) {
    process->AddArg(arg);
  }
}

// For use with brillo::Process::SetPreExecCallback(), this runs after
// the fork() in the child process, but before exec(). Call fork() again
// and exit the parent (child of main process). The grandchild will get
// orphaned, meaning init will wait() for it up so we don't have to.
bool Orphan() {
  if (fork() == 0) {  // (grand-)child
    return true;
  }
  // parent
  ::_Exit(EXIT_SUCCESS);
}


}  // namespace

PerfTool::PerfTool() {}

int PerfTool::GetPerfOutput(const uint32_t& duration_secs,
                            const std::vector<std::string>& perf_args,
                            std::vector<uint8_t>* perf_data,
                            std::vector<uint8_t>* perf_stat,
                            DBus::Error* error) {
  PerfSubcommand subcommand = GetPerfSubcommandType(perf_args);
  if (subcommand == PERF_COMMAND_UNSUPPORTED) {
    error->set(kUnsupportedPerfToolErrorName,
               "perf_args must begin with {\"perf\", \"record\"}, "
               " {\"perf\", \"stat\"}, or {\"perf\", \"mem\"}");
    return -1;
  }

  std::string output_string;
  int result =
      GetPerfOutputHelper(duration_secs, perf_args, error, &output_string);

  switch (subcommand) {
  case PERF_COMMAND_RECORD:
  case PERF_COMMAND_MEM:
    perf_data->assign(output_string.begin(), output_string.end());
    break;
  case PERF_COMMAND_STAT:
    perf_stat->assign(output_string.begin(), output_string.end());
    break;
  default:
    // Discard the output.
    break;
  }

  return result;
}

void PerfTool::GetPerfOutputFd(const uint32_t& duration_secs,
                               const std::vector<std::string>& perf_args,
                               const DBus::FileDescriptor& stdout_fd,
                               DBus::Error* error) {
  PerfSubcommand subcommand = GetPerfSubcommandType(perf_args);
  if (subcommand == PERF_COMMAND_UNSUPPORTED) {
    error->set(kUnsupportedPerfToolErrorName,
               "perf_args must begin with {\"perf\", \"record\"}, "
               " {\"perf\", \"stat\"}, or {\"perf\", \"mem\"}");
    return;
  }

  SandboxedProcess process;
  process.SandboxAs("root", "root");
  if (!process.Init()) {
    error->set(kProcessErrorName, "Process initialization failure.");
    return;
  }

  AddQuipperArguments(&process, duration_secs, perf_args);

  process.SetPreExecCallback(base::Bind(Orphan));
  process.BindFd(stdout_fd.get(), 1);

  if (process.Run() != 0) {
    error->set(kProcessErrorName, "Process start failure.");
  }
}

int PerfTool::GetPerfOutputHelper(const uint32_t& duration_secs,
                                  const std::vector<std::string>& perf_args,
                                  DBus::Error* error,
                                  std::string* data_string) {
  // This whole method is synchronous, so we create a subprocess, let it run to
  // completion, then gather up its output to return it.
  ProcessWithOutput process;
  process.SandboxAs("root", "root");
  if (!process.Init())
    *data_string = "<process init failed>";

  AddQuipperArguments(&process, duration_secs, perf_args);

  // Run the process to completion. If the process might take a while, you may
  // have to make this asynchronous using .Start().
  int status = process.Run();
  if (status != 0)
    *data_string = StringPrintf("<process exited with status: %d", status);
  process.GetOutput(data_string);

  return status;
}

}  // namespace debugd
