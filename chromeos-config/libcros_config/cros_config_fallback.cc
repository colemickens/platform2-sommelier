// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Fallback CrosConfig when running on non-unibuild platforms that
// gets info by calling out to external commands (e.g., mosys)

#include "chromeos-config/libcros_config/cros_config_fallback.h"

#include <iostream>
#include <string>
#include <vector>

#include <base/process/launch.h>
#include <base/strings/string_split.h>
#include "chromeos-config/libcros_config/cros_config_interface.h"

namespace {

struct CommandMapEntry {
  // The path and property to match on
  const char* path;
  const char* property;

  // The corresponding command to run, which is just a space-separated
  // argv (not parsed by shell)
  const char* command;
};

constexpr CommandMapEntry kCommandMap[] = {
  {"/firmware", "image-name", "mosys platform model"},
  {"/", "brand-code", "mosys platform brand"}};

}  // namespace

namespace brillo {

CrosConfigFallback::CrosConfigFallback() {}
CrosConfigFallback::~CrosConfigFallback() {}

bool CrosConfigFallback::GetString(const std::string& path,
                                   const std::string& property,
                                   std::string* val_out) {
  CROS_CONFIG_LOG(INFO) << "Using fallback configuration";

  for (auto entry : kCommandMap) {
    if (path == entry.path && property == entry.property) {
      CROS_CONFIG_LOG(INFO)
          << "Equivalent command is \"" << entry.command << "\"";
      std::vector<std::string> argv = base::SplitString(
          entry.command, " ", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);

      if (!base::GetAppOutput(argv, val_out)) {
        CROS_CONFIG_LOG(ERROR)
            << "\"" << entry.command << "\" has non-zero exit code";
        return false;
      }

      // Trim off (one) trailing newline from mosys
      if (val_out->back() == '\n')
        val_out->pop_back();
      return true;
    }
  }

  CROS_CONFIG_LOG(ERROR) << "No defined fallback command for " << path
                         << " " << property;
  return false;
}

}  // namespace brillo
