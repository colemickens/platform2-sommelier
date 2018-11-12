// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "debugd/src/verify_ro_tool.h"

#include <stdlib.h>
#include <unistd.h>

#include <string>
#include <vector>

#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/logging.h>
#include <base/strings/string_util.h>

#include "debugd/src/error_utils.h"
#include "debugd/src/process_with_id.h"
#include "debugd/src/process_with_output.h"
#include "debugd/src/verify_ro_utils.h"

namespace {

constexpr char kGsctool[] = "/usr/sbin/gsctool";
constexpr char kCr50VerifyRoScript[] = "/usr/share/cros/cr50-verify-ro.sh";
// Parent dir of where Cr50 image and RO db files are stored.
constexpr char kCr50ResourcePath[] = "/opt/google/cr50/";
constexpr char kVerifyRoToolErrorString[] =
    "org.chromium.debugd.error.VerifyRo";

// Exit code of `gsctool <image>` when the DUT's FW is successfully updated.
//
// TODO(garryxiao): try to include the exit status enum from gsctool instead of
// hard-coding it here.
constexpr int kGsctoolAllFwUpdated = 1;

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

  if (exit_code != EXIT_SUCCESS) {
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

  if (exit_code != EXIT_SUCCESS) {
    LOG(WARNING) << __func__ << ": process exited with a non-zero status.";
    return "<process exited with a non-zero status>";
  }

  // A normal output of gsctool -i -M looks like
  //
  // ...
  // BID_TYPE=5a534b4d
  // BID_TYPE_INV=a5acb4b2
  // BID_FLAGS=00007f80
  // BID_RLZ=ZSKM
  return GetKeysValuesFromProcessOutput(
      output, {"BID_TYPE", "BID_TYPE_INV", "BID_FLAGS", "BID_RLZ"});
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

bool VerifyRoTool::FlashImageToGscOnUsb(
    brillo::ErrorPtr* error, const std::string& image_file) {
  if (!CheckCr50ResourceLocation(image_file, false /* is_dir */)) {
    DEBUGD_ADD_ERROR(error,
                     kVerifyRoToolErrorString,
                     "bad image file: " + image_file);
    LOG(ERROR) << "bad image file: " << image_file;
    return false;
  }

  int exit_code = ProcessWithOutput::RunProcess(
      kGsctool,
      {image_file},
      false,         // requires_root
      true,          // disable_sandbox
      nullptr,       // stdin
      nullptr,       // stdout
      nullptr,       // stderr
      error);

  if (exit_code != kGsctoolAllFwUpdated) {
    DEBUGD_ADD_ERROR(error,
                     kVerifyRoToolErrorString,
                     "failed to flash image " + image_file);
    LOG(ERROR) << __func__ << ": failed to flash image " << image_file;
    return false;
  }

  return true;
}

bool VerifyRoTool::VerifyDeviceOnUsbROIntegrity(
    brillo::ErrorPtr* error,
    const std::string& ro_desc_file) {
  if (!CheckCr50ResourceLocation(ro_desc_file, false /* is_dir */)) {
    DEBUGD_ADD_ERROR(error, kVerifyRoToolErrorString,
                     "bad RO descriptor file: " + ro_desc_file);
    LOG(ERROR) << "bad RO descriptor file: " << ro_desc_file;
    return false;
  }

  int exit_code = ProcessWithOutput::RunProcess(kGsctool, {"-O", ro_desc_file},
                                                false,    // requires_root
                                                true,     // disable_sandbox
                                                nullptr,  // stdin
                                                nullptr,  // stdout
                                                nullptr,  // stderr
                                                error);

  if (exit_code != EXIT_SUCCESS) {
    DEBUGD_ADD_ERROR(error, kVerifyRoToolErrorString,
                     "failed to verify RO FW using file " + ro_desc_file);
    LOG(ERROR) << __func__ << ": failed to verify RO FW using file "
               << ro_desc_file;
    return false;
  }

  return true;
}

bool VerifyRoTool::UpdateAndVerifyFWOnUsb(brillo::ErrorPtr* error,
                                          const base::ScopedFD& outfd,
                                          const std::string& image_file,
                                          const std::string& ro_db_dir,
                                          std::string* handle) {
  if (!CheckCr50ResourceLocation(image_file, false /* is_dir */)) {
    DEBUGD_ADD_ERROR(error, kVerifyRoToolErrorString,
                     "Bad FW image file: " + image_file);
    return false;
  }

  if (!CheckCr50ResourceLocation(ro_db_dir, true /* is_dir */)) {
    DEBUGD_ADD_ERROR(error, kVerifyRoToolErrorString,
                     "Bad ro descriptor dir: " + ro_db_dir);
    return false;
  }

  ProcessWithId* p =
      CreateProcess(false /* sandboxed */, false /* access_root_mount_ns */);
  if (!p) {
    DEBUGD_ADD_ERROR(error, kVerifyRoToolErrorString,
                     "Could not create the verify_ro process.");
    return false;
  }

  p->AddArg(kCr50VerifyRoScript);
  p->AddArg(image_file);
  p->AddArg(ro_db_dir);

  p->BindFd(outfd.get(), STDOUT_FILENO);
  p->BindFd(outfd.get(), STDERR_FILENO);

  if (!p->Start()) {
    DEBUGD_ADD_ERROR(error, kVerifyRoToolErrorString,
                     "Failed to run the verify_ro process.");
    return false;
  }

  *handle = p->id();
  return true;
}

std::string VerifyRoTool::GetKeysValuesFromImage(
    const std::string& image_file, const std::vector<std::string>& keys) {
  if (!CheckCr50ResourceLocation(image_file, false /* is_dir */)) {
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

  if (exit_code != EXIT_SUCCESS) {
    LOG(WARNING) << __func__ << ": process exited with a non-zero status.";
    return "<process exited with a non-zero status>";
  }

  return GetKeysValuesFromProcessOutput(output, keys);
}

bool VerifyRoTool::CheckCr50ResourceLocation(const std::string& path,
                                             bool is_dir) {
  base::FilePath absolute_path =
      base::MakeAbsoluteFilePath(base::FilePath(path));
  if (absolute_path.empty()) {
    // |path| doesn't exist.
    return false;
  }

  if (is_dir && !base::DirectoryExists(absolute_path)) {
    // |path| is not a dir.
    return false;
  }

  // Using absolute path here to avoid path spoofing, e.g.,
  // /opt/google/cr50/../../../tmp/badfile
  return base::StartsWith(absolute_path.value(),
                          kCr50ResourcePath,
                          base::CompareCase::SENSITIVE);
}

}  // namespace debugd
