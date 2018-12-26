/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <string>
#include <utility>
#include <vector>

#include <base/process/launch.h>
#include <base/values.h>

#include <pcrecpp.h>

#include "runtime_probe/functions/ectool_i2cread.h"

using std::to_string;

namespace runtime_probe {

EctoolI2Cread::DataType EctoolI2Cread::Eval() const {
  DataType result{};
  constexpr auto kEctoolBinaryPath = "/usr/sbin/ectool";
  constexpr auto kEctoolSubcommand = "i2cread";
  constexpr auto kRegexPattern =
      R"(^Read from I2C port [\d]+ at .* offset .* = (.+)$)";

  std::vector<std::string> ectool_cmd_arg{
      kEctoolBinaryPath, kEctoolSubcommand, to_string(size_),
      to_string(port_),  to_string(addr_),  to_string(offset_)};
  std::string ectool_output;

  if (!base::GetAppOutput(ectool_cmd_arg, &ectool_output)) {
    LOG(ERROR) << "Failed to execute command \"ectool i2cread\"";
  } else {
    VLOG(1) << "ectool i2cread output:\n" << ectool_output;
    pcrecpp::RE re(kRegexPattern);
    std::string reg_value;
    if (re.PartialMatch(ectool_output, &reg_value)) {
      base::DictionaryValue dict_value;
      dict_value.SetString(key_, reg_value);
      result.push_back(std::move(dict_value));
    }
  }

  return result;
}

}  // namespace runtime_probe
