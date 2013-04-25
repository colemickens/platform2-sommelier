// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "perf_tool.h"

#include "process_with_output.h"

namespace {

// Location of quipper on ChromeOS.
const char kQuipperLocation[] = "/usr/bin/quipper";

// Base perf command line to be used.
const char kPerfRecord[] = "/usr/sbin/perf record -a";

// This flag enables perf to run in callgraph mode.
const char kCallGraphMode[] = " -g";

// This flag enables perf to run in branch mode.
const char kBranchMode[] = " -b";

// This flag enables perf to record cycle events.
const char kCyclesEvent[] = " -e cycles";

// This flag enables perf to record iTLB-miss events.
const char kITLBMissesEvent[] = " -e iTLB-misses";

// This flag enables perf to record dTLB-miss events.
const char kDTLBMissesEvent[] = " -e dTLB-misses";

void PopulateOdds(std::map<std::string, float>* odds) {
  std::string base_perf_command = kPerfRecord;
  (*odds)[base_perf_command] = 80;
  (*odds)[base_perf_command + kCallGraphMode] = 5;
  (*odds)[base_perf_command + kBranchMode] = 5;
  (*odds)[base_perf_command + kITLBMissesEvent] = 5;
  (*odds)[base_perf_command + kDTLBMissesEvent] = 5;
}

}  // namespace

namespace debugd {

PerfTool::PerfTool() {
  std::map<std::string, float> odds;
  PopulateOdds(&odds);
  random_selector_.SetOdds(odds);
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

