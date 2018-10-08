// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "debugd/src/verify_ro_tool.h"

#include <string>
#include <vector>

#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/logging.h>
#include <base/strings/string_split.h>

namespace {

constexpr char kGsctool[] = "/usr/sbin/gsctool";

}  // namespace

namespace debugd {

std::string VerifyRoTool::GetGscOnUsbRWFirmwareVer() {
  ProcessWithOutput process;

  // Needed for accessing USB.
  process.DisableSandbox();

  if (!process.Init()) {
    LOG(ERROR) << __func__ << ": process init failed.";
    return "<process init failed>";
  }

  process.AddArg(kGsctool);
  process.AddArg("-f");
  process.AddArg("-M");
  if (process.Run() != 0) {
    LOG(WARNING) << __func__ << ": process exited with a non-zero status.";
    return "<process exited with a non-zero status>";
  }

  // A normal output of gsctool -f -M looks like
  //
  //    ...
  // RO_FW_VER=0.0.10
  // RW_FW_VER=0.3.10
  //
  // What we are interested in is the line with the key RW_FW_VER.
  return GetLineWithKeyFromProcess(process, "RW_FW_VER");
}

std::string VerifyRoTool::GetLineWithKeyFromProcess(
    const ProcessWithOutput& process, const std::string& key) {
  std::string output;
  if (!process.GetOutput(&output)) {
    LOG(ERROR) << "process output failed.";
    return "<process output failed>";
  }

  base::StringPairs key_value_pairs;
  base::SplitStringIntoKeyValuePairs(output, '=', '\n', &key_value_pairs);
  for (const auto& key_value : key_value_pairs) {
    if (key == key_value.first) {
      return key + "=" + key_value.second + "\n";
    }
  }

  LOG(ERROR) << "key " << key << " wasn't found in the process output";
  return "<invalid process output>";
}

}  // namespace debugd
