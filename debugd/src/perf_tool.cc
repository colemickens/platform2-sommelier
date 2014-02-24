// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "perf_tool.h"

#include <fstream>

#include <base/strings/string_split.h>

#include "cpu_info_parser.h"
#include "process_with_output.h"

namespace {

// Location of quipper on ChromeOS.
const char kQuipperLocation[] = "/usr/bin/quipper";

// Base perf command line to be used.
const char kPerfRecord[] = "/usr/bin/perf record -a";

// This is the key in /proc/cpuinfo whose value is the model name of the CPU.
const char kCPUModelNameKey[] = "model name";

// Processor model name substrings for which we have perf commands.
const char* kCPUOddsFiles[] = {
  "unknown",
  "core",
  "arm",
};

// Prefix path to attach to the CPU odds file.
const char kCPUOddsFilePrefix[] = "/etc/perf_commands/";

// Suffix to attach to the CPU odds file.
const char kCPUOddsFileSuffix[] = ".txt";

// Goes through the list of kCPUOddsFiles, and if the any of those strings is a
// substring of the |cpu_model_name|, returns that string. If no matches are
// found, returns the first string of |kCPUOddsFiles| ("unknown").
void GetOddsFilenameForCPU(const std::string& cpu_model_name,
                           std::string* odds_filename) {
  std::string lowered_cpu_model_name = StringToLowerASCII(cpu_model_name);
  for (size_t i = 0; i < arraysize(kCPUOddsFiles); ++i) {
    if (lowered_cpu_model_name.find(kCPUOddsFiles[i]) != std::string::npos) {
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
  std::string odds_file_path = std::string(kCPUOddsFilePrefix) +
      odds_filename +
      kCPUOddsFileSuffix;
  random_selector_.SetOddsFromFile(odds_file_path);
}

PerfTool::~PerfTool() { }

// Tool methods have the same signature as the generated DBus adaptors. Most
// pertinently, this means they take their DBus::Error argument as a non-const
// reference (hence the NOLINT). Tool methods are generally written in
// can't-fail style, since their output is usually going to be displayed to the
// user; instead of returning a DBus exception, we tend to return a string
// indicating what went wrong.
std::vector<uint8> PerfTool::GetPerfData(const uint32_t& duration_secs,
                                         DBus::Error& error) { // NOLINT
  std::string output_string;
  GetPerfDataHelper(duration_secs, kPerfRecord, error, &output_string);
  std::vector<uint8> output_vector(output_string.begin(),
                                   output_string.end());
  return output_vector;
}

std::vector<uint8> PerfTool::GetRichPerfData(const uint32_t& duration_secs,
                                             DBus::Error& error) { // NOLINT
  std::string perf_command_line;
  random_selector_.GetNext(&perf_command_line);
  std::string output_string;
  GetPerfDataHelper(duration_secs, perf_command_line, error, &output_string);
  return std::vector<uint8>(output_string.begin(), output_string.end());
}

void PerfTool::GetPerfDataHelper(const uint32_t& duration_secs,
                                 const std::string& perf_command_line,
                                 DBus::Error& error,
                                 std::string* data_string) { // NOLINT
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

};  // namespace debugd

