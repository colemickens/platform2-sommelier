// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modemfwd/modem_helper.h"

#include <memory>
#include <utility>

#include <base/files/file.h>
#include <base/files/file_util.h>
#include <base/macros.h>
#include <base/memory/ptr_util.h>
#include <base/strings/string_split.h>
#include <base/strings/stringprintf.h>
#include <brillo/process.h>
#include <chromeos/switches/modemfwd_switches.h>

namespace {

bool RunHelperProcess(const base::FilePath& helper_path,
                      const std::string& arg,
                      std::string* output) {
  brillo::ProcessImpl helper;
  helper.AddArg(helper_path.value());
  helper.AddArg("--" + arg);

  // Set up output redirection, if requested. We keep the file open
  // across the process lifetime to ensure nobody is swapping out the
  // file from underneath us while the helper is running.
  base::File output_base_file;
  if (output) {
    base::FilePath output_path;
    FILE* output_file = base::CreateAndOpenTemporaryFile(&output_path);
    if (output_file == nullptr) {
      LOG(ERROR) << "Failed to create tempfile for helper process output";
      return false;
    }
    output_base_file = base::File(fileno(output_file));
    DCHECK(output_base_file.IsValid());

    helper.RedirectOutput(output_path.value());
  }

  int exit_code = helper.Run();

  // Collect output if requested. Note that we only collect 1024 bytes of
  // output here. We could read everything, but the API is simple enough
  // that we shouldn't need more than this (at least for the time being).
  if (output && output_base_file.IsValid()) {
    const int kBufSize = 1024;
    char buf[kBufSize];
    int bytes_read = output_base_file.ReadAtCurrentPos(buf, kBufSize);
    if (bytes_read != -1)
      output->assign(buf, bytes_read);
  }

  if (exit_code != 0) {
    LOG(ERROR) << "Failed to perform \"" << arg << "\" on the modem";
    return false;
  }

  return true;
}

// Ensures we reboot the modem to prevent us from leaving it in a bad state.
class FlashMode {
 public:
  static std::unique_ptr<FlashMode> Create(const base::FilePath& helper_path) {
    if (!RunHelperProcess(helper_path, modemfwd::kPrepareToFlash, nullptr))
      return nullptr;

    return base::WrapUnique(new FlashMode(helper_path));
  }

  ~FlashMode() { RunHelperProcess(helper_path_, modemfwd::kReboot, nullptr); }

 private:
  // Use the static factory method above.
  explicit FlashMode(const base::FilePath& helper_path)
      : helper_path_(helper_path) {}

  base::FilePath helper_path_;

  DISALLOW_COPY_AND_ASSIGN(FlashMode);
};

}  // namespace

namespace modemfwd {

class ModemHelperImpl : public ModemHelper {
 public:
  explicit ModemHelperImpl(const base::FilePath& helper_path)
      : helper_path_(helper_path) {}
  ~ModemHelperImpl() override = default;

  bool GetFirmwareInfo(FirmwareInfo* out_info) override {
    CHECK(out_info);

    std::string helper_output;
    if (!RunHelperProcess(helper_path_, kGetFirmwareInfo, &helper_output))
      return false;

    std::vector<std::string> parsed_output = base::SplitString(
        helper_output, "\n", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    if (parsed_output.size() != 3) {
      LOG(WARNING) << "Modem helper returned malformed firmware version info";
      return false;
    }

    out_info->main_version = parsed_output[0];
    out_info->carrier_uuid = parsed_output[1];
    out_info->carrier_version = parsed_output[2];
    return true;
  }

  // modemfwd::ModemHelper overrides.
  bool FlashMainFirmware(const base::FilePath& path_to_fw) override {
    auto flash_mode = FlashMode::Create(helper_path_);
    if (!flash_mode)
      return false;

    return RunHelperProcess(helper_path_,
                            base::StringPrintf("%s=%s", kFlashMainFirmware,
                                               path_to_fw.value().c_str()),
                            nullptr);
  }

  bool FlashCarrierFirmware(const base::FilePath& path_to_fw) override {
    auto flash_mode = FlashMode::Create(helper_path_);
    if (!flash_mode)
      return false;

    return RunHelperProcess(helper_path_,
                            base::StringPrintf("%s=%s", kFlashCarrierFirmware,
                                               path_to_fw.value().c_str()),
                            nullptr);
  }

 private:
  base::FilePath helper_path_;

  DISALLOW_COPY_AND_ASSIGN(ModemHelperImpl);
};

std::unique_ptr<ModemHelper> CreateModemHelper(
    const base::FilePath& helper_path) {
  return std::make_unique<ModemHelperImpl>(helper_path);
}

}  // namespace modemfwd
