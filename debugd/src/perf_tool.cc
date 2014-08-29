// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "debugd/src/perf_tool.h"

#include <base/strings/string_split.h>
#include <base/strings/string_util.h>

#include <algorithm>

#include "debugd/src/cpu_info_parser.h"
#include "debugd/src/process_with_output.h"

using base::StringPrintf;

namespace {

// Location of quipper on ChromeOS.
const char kQuipperLocation[] = "/usr/bin/quipper";

// This is the key in /proc/cpuinfo whose value is the model name of the CPU.
const char kCPUModelNameKey[] = "model name";

// This is registered trademark symbol that appears in model name strings.
const char kRegisteredTrademarkSymbol[] = "(R)";

// Processor model name substrings for which we have perf commands.
const char* kCPUOddsFiles[] = {
  "unknown",
  "core",
  "celeron-2955u",
  "arm",
};

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
  return StringToLowerASCII(result);
}

// Goes through the list of kCPUOddsFiles, and if the any of those strings is a
// substring of the |cpu_model_name|, returns that string. If no matches are
// found, returns the first string of |kCPUOddsFiles| ("unknown").
void GetOddsFilenameForCPU(const std::string& cpu_model_name,
                           std::string* odds_filename) {
  std::string adjusted_model_name = ModelNameToFileName(cpu_model_name);
  for (size_t i = 0; i < arraysize(kCPUOddsFiles); ++i) {
    if (adjusted_model_name.find(kCPUOddsFiles[i]) != std::string::npos) {
      *odds_filename = kCPUOddsFiles[i];
      return;
    }
  }
  *odds_filename = kCPUOddsFiles[0];
}

}  // namespace

namespace debugd {

PerfTool::PerfTool() {
  std::string cpu_model_name;
  debugd::CPUInfoParser cpu_info_parser;
  cpu_info_parser.GetKey(kCPUModelNameKey, &cpu_model_name);
  std::string odds_filename;
  GetOddsFilenameForCPU(cpu_model_name, &odds_filename);
  std::string odds_file_path =
      std::string(kCPUOddsFilePrefix) + odds_filename + kCPUOddsFileSuffix;
  random_selector_.SetOddsFromFile(odds_file_path);
}

std::vector<uint8_t> PerfTool::GetRichPerfData(const uint32_t& duration_secs,
                                               DBus::Error* error) {
  std::string perf_command_line;
  random_selector_.GetNext(&perf_command_line);
  std::string output_string;
  GetPerfDataHelper(duration_secs, perf_command_line, error, &output_string);
  return std::vector<uint8_t>(output_string.begin(), output_string.end());
}

void PerfTool::GetPerfDataHelper(const uint32_t& duration_secs,
                                 const std::string& perf_command_line,
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
  process.AddArg(perf_command_line);
  // Run the process to completion. If the process might take a while, you may
  // have to make this asynchronous using .Start().
  int status = process.Run();
  if (status != 0)
    *data_string = StringPrintf("<process exited with status: %d", status);
  process.GetOutput(data_string);
}

}  // namespace debugd
