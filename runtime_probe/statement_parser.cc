// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/json/json_reader.h>

#include "runtime_probe/statement_parser.h"

using base::DictionaryValue;

namespace runtime_probe {
std::unique_ptr<DictionaryValue> ParseProbeConfig(
    const std::string& config_file_path) {
  std::string statement_json;
  if (!base::ReadFileToString(base::FilePath(config_file_path),
                              &statement_json)) {
    LOG(ERROR) << "Config file doesn't exist. "
               << "Input config file path is: " << config_file_path;
    return nullptr;
  }
  std::unique_ptr<DictionaryValue> dict_val =
      DictionaryValue::From(base::JSONReader::Read(statement_json));
  if (dict_val == nullptr) {
    LOG(ERROR) << "Failed to parse JSON statement. "
               << "Input JSON string is: " << statement_json;
  }
  return dict_val;
}

}  // namespace runtime_probe
