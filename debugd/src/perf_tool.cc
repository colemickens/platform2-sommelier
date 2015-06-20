// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "debugd/src/perf_tool.h"

#include <base/strings/string_split.h>
#include <base/strings/string_util.h>
#include <sys/utsname.h>

#include <algorithm>

#include "debugd/src/cpu_info_parser.h"
#include "debugd/src/process_with_output.h"
#include "debugd/src/random_selector.h"

using base::StringPrintf;

namespace {

// Location of quipper on ChromeOS.
const char kQuipperLocation[] = "/usr/bin/quipper";

// This is the key in /proc/cpuinfo whose value is the model name of the CPU.
const char kCPUModelNameKey[] = "model name";

// This is registered trademark symbol that appears in model name strings.
const char kRegisteredTrademarkSymbol[] = "(R)";

// Processor model name substrings for which we have perf commands.

// For 64-bit x86 processors.
const char* kx86_64CPUOddsFiles[] = {
  "celeron-2955u",
  NULL,
};

// For 32-bit x86 processors.
const char* kx86_32CPUOddsFiles[] = {
  // 32-bit x86 doesn't have any special cases, so all processors use the
  // default commands. Add future special cases here.
  NULL,
};

// For ARMv7 processors.
const char* kARMv7CPUOddsFiles[] = {
  // ARMv7 doesn't have any special cases, so all processors use the default
  // commands. Add future special cases here.
  NULL,
};

// For miscellaneous processors models of a known architecture.
const char kMiscCPUModelOddsFile[] = "default";

// Odds file name for miscellaneous processor architectures.
const char kMiscCPUArchOddsFile[] = "unknown";

// Prefix path to attach to the CPU odds file.
const char kCPUOddsFilePrefix[] = "/etc/perf_commands/";

// Suffix to attach to the CPU odds file.
const char kCPUOddsFileSuffix[] = ".txt";

// Converts an CPU model name string into a format that can be used as a file
// name. The rules are:
// - Replace spaces with hyphens.
// - Strip all "(R)" symbols.
// - Convert to lower case.
std::string ModelNameToFileName(const std::string& model_name) {
  std::string result = model_name;
  std::replace(result.begin(), result.end(), ' ', '-');
  ReplaceSubstringsAfterOffset(&result, 0, kRegisteredTrademarkSymbol, "");
  return base::StringToLowerASCII(result);
}

// For the given |cpu_arch| and |cpu_model_name|, look for the CPU odds file
// that corresponds to these strings. If no matches are found for |CPU_arch|,
// return the odds file for unknown CPU types. If |cpu_arch| is valid, but no
// matches are found |cpu_model_name|, return the odds file for unknown models
// of class |cpu_arch|.
std::string GetOddsFilenameForCPU(const std::string& cpu_arch,
                                  const std::string& cpu_model_name) {
  const char** cpu_odds_file_list = NULL;
  if (cpu_arch == "i386" || cpu_arch == "i486" || cpu_arch == "i586" ||
      cpu_arch == "i686") {
    cpu_odds_file_list = kx86_32CPUOddsFiles;
  } else if (cpu_arch == "amd64" || cpu_arch == "x86_64") {
    cpu_odds_file_list = kx86_64CPUOddsFiles;
  } else if (cpu_arch == "armv7l") {
    cpu_odds_file_list = kARMv7CPUOddsFiles;
  } else {
    // If the CPU arch doesn't match any of the recognized arch families, just
    // use the CPU odds file for unknown CPU types.
    return kMiscCPUArchOddsFile;
  }

  std::string adjusted_model_name = ModelNameToFileName(cpu_model_name);
  for (size_t i = 0; cpu_odds_file_list[i]; ++i) {
    if (adjusted_model_name.find(cpu_odds_file_list[i]) != std::string::npos) {
      return cpu_arch + "/" + cpu_odds_file_list[i];
    }
  }
  // If there isn't an odds file for the particular model, use the generic odds
  // for the CPU arch.
  return cpu_arch + "/" + kMiscCPUModelOddsFile;
}

}  // namespace

namespace debugd {

PerfTool::PerfTool() : PerfTool(CPUInfoReader(), new RandomSelector) {}

PerfTool::PerfTool(const CPUInfoReader& cpu_info,
                   RandomSelector* random_selector)
    : random_selector_(random_selector) {
  std::string odds_filename =
      GetOddsFilenameForCPU(cpu_info.arch(), cpu_info.model());
  random_selector_->SetOddsFromFile(
      kCPUOddsFilePrefix + odds_filename + kCPUOddsFileSuffix);
}

std::vector<uint8_t> PerfTool::GetRichPerfData(const uint32_t& duration_secs,
                                               DBus::Error* error) {
  const std::vector<std::string>& perf_args = random_selector_->GetNext();
  if (perf_args[1] != "record")
    return std::vector<uint8_t>();

  std::string output_string;
  int result =
      GetPerfOutputHelper(duration_secs, perf_args, error, &output_string);

  if (result > 0)
    return std::vector<uint8_t>();

  return std::vector<uint8_t>(output_string.begin(), output_string.end());
}

int PerfTool::GetRandomPerfOutput(const uint32_t& duration_secs,
                                  std::vector<uint8_t>* perf_data,
                                  std::vector<uint8_t>* perf_stat,
                                  DBus::Error* error) {
  const std::vector<std::string>& perf_args = random_selector_->GetNext();
  std::string output_string;
  int result =
      GetPerfOutputHelper(duration_secs, perf_args, error, &output_string);

  if (perf_args[1] == "record")
    perf_data->assign(output_string.begin(), output_string.end());
  else if (perf_args[1] == "stat")
    perf_stat->assign(output_string.begin(), output_string.end());

  return result;
}

PerfTool::CPUInfoReader::CPUInfoReader() {
  // Get CPU model name, e.g. "Intel(R) Celeron(R) 2955U @ 1.40GHz".
  debugd::CPUInfoParser cpu_info_parser;
  cpu_info_parser.GetKey(kCPUModelNameKey, &model_);

  // Get CPU machine hardware class, e.g. "i686", "x86_64", "armv7l".
  struct utsname uname_info;
  if (!uname(&uname_info))
    arch_ = uname_info.machine;
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
  // If you're going to add switches to a command, have a look at the Process
  // interface; there's support for adding options specifically.
  process.AddArg(kQuipperLocation);
  process.AddArg(StringPrintf("%u", duration_secs));
  for (const auto& arg : perf_args) {
    process.AddArg(arg);
  }
  // Run the process to completion. If the process might take a while, you may
  // have to make this asynchronous using .Start().
  int status = process.Run();
  if (status != 0)
    *data_string = StringPrintf("<process exited with status: %d", status);
  process.GetOutput(data_string);

  return status;
}

}  // namespace debugd
