// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "debugd/src/verify_ro_tool.h"

#include <string>
#include <vector>

#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/logging.h>

#include "debugd/src/process_with_output.h"
#include "debugd/src/verify_ro_utils.h"

namespace {

constexpr char kGsctool[] = "/usr/sbin/gsctool";

}  // namespace

namespace debugd {

std::string VerifyRoTool::GetGscOnUsbRWFirmwareVer() {
  std::string output;

  // Need to disable sandbox for accessing USB.
  //
  // TODO(garryxiao): use SandboxAs() instead once we move suzy-q access from
  // chronos to a new user and group.
  int exit_code = ProcessWithOutput::RunProcess(
      kGsctool,
      {"-f", "-M"},
      false,         // requires_root
      true,          // disable_sandbox
      nullptr,       // stdin
      &output,       // stdout
      nullptr,       // stderr
      nullptr);

  if (exit_code != 0) {
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
  return GetKeysValuesFromProcessOutput(output, {"RW_FW_VER"});
}

std::string VerifyRoTool::GetGscOnUsbBoardID() {
  std::string output;

  // Need to disable sandbox for accessing USB.
  //
  // TODO(garryxiao): use SandboxAs() instead once we move suzy-q access from
  // chronos to a new user and group.
  int exit_code = ProcessWithOutput::RunProcess(
      kGsctool,
      {"-i", "-M"},
      false,         // requires_root
      true,          // disable_sandbox
      nullptr,       // stdin
      &output,       // stdout
      nullptr,       // stderr
      nullptr);

  if (exit_code != 0) {
    LOG(WARNING) << __func__ << ": process exited with a non-zero status.";
    return "<process exited with a non-zero status>";
  }

  // A normal output of gsctool -i -M looks like
  //
  // ...
  // BID_TYPE=5a534b4d
  // BID_TYPE_INV=a5acb4b2
  // BID_FLAGS=00007f80
  return GetKeysValuesFromProcessOutput(
      output, {"BID_TYPE", "BID_TYPE_INV", "BID_FLAGS"});
}

std::string VerifyRoTool::GetGscImageRWFirmwareVer(
    const std::string& image_file) {
  // A normal output of gsctool -M -b image.bin looks like
  //
  // ...
  // IMAGE_RO_FW_VER=0.0.11
  // IMAGE_RW_FW_VER=0.3.11
  // ...
  //
  // What we are interested in is the line with the key IMAGE_RW_FW_VER.
  return GetKeysValuesFromImage(image_file, {"IMAGE_RW_FW_VER"});
}

std::string VerifyRoTool::GetGscImageBoardID(const std::string& image_file) {
  // A normal output of gsctool -M -b image.bin looks like
  //
  // ...
  // IMAGE_RO_FW_VER=0.0.11
  // IMAGE_RW_FW_VER=0.3.11
  // IMAGE_BID_STRING=01234567
  // IMAGE_BID_MASK=00001111
  // IMAGE_BID_FLAGS=76543210
  //
  // What we are interested in is the lines with the keys IMAGE_BID_*.
  std::vector<std::string>
      keys({"IMAGE_BID_STRING", "IMAGE_BID_MASK", "IMAGE_BID_FLAGS"});
  return GetKeysValuesFromImage(image_file, keys);
}

std::string VerifyRoTool::GetKeysValuesFromImage(
    const std::string& image_file, const std::vector<std::string>& keys) {
  if (!base::PathExists(base::FilePath(image_file))) {
    LOG(ERROR) << "bad image file: " << image_file;
    return "<bad image file>";
  }

  std::string output;
  int exit_code = ProcessWithOutput::RunProcess(
      kGsctool,
      {"-M", "-b", image_file},
      false,         // requires_root
      false,         // disable_sandbox
      nullptr,       // stdin
      &output,       // stdout
      nullptr,       // stderr
      nullptr);

  if (exit_code != 0) {
    LOG(WARNING) << __func__ << ": process exited with a non-zero status.";
    return "<process exited with a non-zero status>";
  }

  return GetKeysValuesFromProcessOutput(output, keys);
}

}  // namespace debugd
