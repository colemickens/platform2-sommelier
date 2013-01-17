// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "perf_tool.h"

#include "process_with_output.h"

namespace debugd {

PerfTool::PerfTool() { }

PerfTool::~PerfTool() { }

// Tool methods have the same signature as the generated DBus adaptors. Most
// pertinently, this means they take their DBus::Error argument as a non-const
// reference (hence the NOLINT). Tool methods are generally written in
// can't-fail style, since their output is usually going to be displayed to the
// user; instead of returning a DBus exception, we tend to return a string
// indicating what went wrong.
std::vector<uint8> PerfTool::GetPerfData(const uint32_t& duration,
                                                 DBus::Error& error) { // NOLINT
  std::string output_string;
  GetPerfDataHelper(duration, error, &output_string);
  std::vector<uint8> output_vector(output_string.begin(),
                                   output_string.end());
  return output_vector;
}

void PerfTool::GetPerfDataHelper(const uint32_t& duration,
                                 DBus::Error& error,
                                 std::string* data_string) { // NOLINT
  std::string path = "/usr/bin/quipper";

  if (path.length() > PATH_MAX)
    *data_string = "<path too long>";
  // This whole method is synchronous, so we create a subprocess, let it run to
  // completion, then gather up its output to return it.
  ProcessWithOutput process;
  process.SandboxAs("root", "root");
  if (!process.Init())
    *data_string = "<process init failed>";
  // If you're going to add switches to a command, have a look at the Process
  // interface; there's support for adding options specifically.
  process.AddArg(path);
  process.AddArg(StringPrintf("%u", duration));
  process.AddArg("/usr/sbin/perf record -a");
  // Run the process to completion. If the process might take a while, you may
  // have to make this asynchronous using .Start().
  int status = process.Run();
  if (status != 0)
    *data_string = StringPrintf("<process exited with status: %d", status);
  process.GetOutput(data_string);
}

};  // namespace debugd

