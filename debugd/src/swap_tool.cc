// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "debugd/src/swap_tool.h"

#include <string>

#include <base/strings/stringprintf.h>

#include "debugd/src/process_with_output.h"

namespace debugd {

namespace {

// This script holds the bulk of the real logic.
const char kSwapHelperScript[] = "/usr/share/cros/init/swap.sh";

std::string RunSwapHelper(const ProcessWithOutput::ArgList& arguments,
                          int* result) {
  std::string stdout, stderr;
  *result = ProcessWithOutput::RunProcessFromHelper(
      kSwapHelperScript, arguments, nullptr, &stdout, &stderr);
  return *result ? stderr : stdout;
}

}  // namespace

std::string SwapTool::SwapEnable(uint32_t size, bool change_now,
                                 DBus::Error* error) const {
  int result;
  std::string output, buf;

  buf = base::StringPrintf("%u", size);
  output = RunSwapHelper({"enable", buf}, &result);
  if (result != EXIT_SUCCESS)
    return output;

  if (change_now)
    output = SwapStartStop(true, error);

  return output;
}

std::string SwapTool::SwapDisable(bool change_now, DBus::Error* error) const {
  int result;
  std::string output;

  output = RunSwapHelper({"disable", }, &result);
  if (result != EXIT_SUCCESS)
    return output;

  if (change_now)
    output = SwapStartStop(false, error);

  return output;
}

std::string SwapTool::SwapStartStop(bool on, DBus::Error* error) const {
  int result;
  std::string output;

  // We always turn off swap because the config might have changed.
  // Also because the code doesn't like to turn on twice.
  output = RunSwapHelper({"stop", }, &result);
  if (result != EXIT_SUCCESS)
    return output;

  if (on)
    output = RunSwapHelper({"start", }, &result);

  return output;
}

std::string SwapTool::SwapStatus(DBus::Error* error) const {
  int result;
  return RunSwapHelper({"status", }, &result);
}


std::string SwapTool::SwapSetMargin(uint32_t margin, DBus::Error* error) const {
  int result;
  std::string output, buf;

  buf = base::StringPrintf("%u", margin);
  return RunSwapHelper({"set_margin", buf}, &result);
}

}  // namespace debugd
