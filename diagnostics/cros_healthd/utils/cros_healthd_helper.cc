// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdlib>
#include <string>
#include <vector>

#include <base/logging.h>
#include <base/process/launch.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/string_split.h>
#include <base/strings/string_util.h>
#include <brillo/syslog_logging.h>
#include <re2/re2.h>

namespace {
constexpr int kTotalArgCount = 2;

}  // namespace

int main(int argc, char* argv[]) {
  brillo::InitLog(brillo::kLogToSyslog);
  if (argc != kTotalArgCount) {
    LOG(ERROR) << "Incorrect number of args. Expected: " << kTotalArgCount
               << ", Actual: " << argc;
    return EXIT_FAILURE;
  }

  const std::string& ectool_command = argv[1];
  std::string ectool_output;
  std::vector<std::string> ectool_args = base::SplitString(
      ectool_command, " ", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (!base::GetAppOutputAndError(ectool_args, &ectool_output)) {
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
  int64_t smart_metric;
  if (!base::HexStringToInt64(reg_value, &smart_metric)) {
    LOG(ERROR) << "Failed to convert smart_metric from hexadecimal to int64";
    return EXIT_FAILURE;
  }

  printf("%ld", smart_metric);

  return EXIT_SUCCESS;
}
