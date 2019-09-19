// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdlib>
#include <string>

#include <base/logging.h>
#include <base/process/launch.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/string_util.h>
#include <brillo/syslog_logging.h>
#include <re2/re2.h>

int main(int argc, char* argv[]) {
  brillo::InitLog(brillo::kLogToSyslog);
  std::string model_name;
  if (!base::GetAppOutputAndError({"cros_config", "/", "name"}, &model_name)) {
    LOG(ERROR) << "Failed to run cros_config: " << model_name.c_str();
    return EXIT_FAILURE;
  }

  if (model_name != "sona")
    return EXIT_FAILURE;

  std::string ectool_output;
  // The command follows the format:
  // ectool i2cread <num_bits> <port> <addr8> <offset>
  // Note that <num_bits> can either be 8 or 16.
  if (!base::GetAppOutputAndError(
          {"/usr/sbin/ectool", "i2cread", "16", "2", "0x16", "0x1b"},
          &ectool_output)) {
    LOG(ERROR) << "Failed to run ectool: " << ectool_output.c_str();
    return EXIT_FAILURE;
  }
  constexpr auto kRegexPattern =
      R"(^Read from I2C port [\d]+ at .* offset .* = (.+)$)";
  std::string reg_value;
  // CollapseWhitespaceASCII is used to remove the newline from ectool_output.
  // This string is collected as output from the terminal.
  if (!RE2::PartialMatch(base::CollapseWhitespaceASCII(ectool_output, true),
                         kRegexPattern, &reg_value)) {
    LOG(ERROR) << "Failed to match ectool output to the regex";
    return EXIT_FAILURE;
  }
  int64_t manufacture_date_smart;
  if (!base::HexStringToInt64(reg_value, &manufacture_date_smart)) {
    LOG(ERROR) << "Failed to convert hex manf date to int64";
    return EXIT_FAILURE;
  }

  printf("%ld", manufacture_date_smart);

  return EXIT_SUCCESS;
}
