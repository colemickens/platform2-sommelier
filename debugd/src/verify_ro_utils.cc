// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "debugd/src/verify_ro_utils.h"

#include <string>
#include <unordered_set>
#include <vector>

#include <base/logging.h>
#include <base/strings/string_split.h>
#include <base/strings/string_util.h>

namespace debugd {

std::string GetKeysValuesFromProcessOutput(
    const std::string& output, const std::vector<std::string>& keys) {
  base::StringPairs key_value_pairs;
  base::SplitStringIntoKeyValuePairs(output, '=', '\n', &key_value_pairs);

  std::unordered_set<std::string> missing_key_set(keys.begin(), keys.end());

  std::string result;
  for (const auto& key_value : key_value_pairs) {
    const std::string& key = key_value.first;
    const std::string& value = key_value.second;

    if (missing_key_set.erase(key) > 0) {
      result += key + "=" + value + "\n";
    }

    if (missing_key_set.empty()) {
      return result;
    }
  }

  // Some keys are missing in the process output.
  std::vector<std::string>
      missing_keys(missing_key_set.begin(), missing_key_set.end());
  LOG(ERROR) << "Key(s) [" << base::JoinString(missing_keys, ", ")
             << "] weren't found in the process output";
  return "<invalid process output>";
}

}  // namespace debugd
