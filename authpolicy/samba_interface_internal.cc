// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "authpolicy/samba_interface_internal.h"

#include <vector>

#include <base/logging.h>
#include <base/strings/string_split.h>
#include <base/strings/string_util.h>

namespace authpolicy {
namespace internal {

// Flags for parsing GPO.
const char* const kGpFlagsStr[] = {
  "0 GPFLAGS_ALL_ENABLED",
  "1 GPFLAGS_USER_SETTINGS_DISABLED",
  "2 GPFLAGS_MACHINE_SETTINGS_DISABLED",
  "3 GPFLAGS_ALL_DISABLED",
};

bool ParseUserPrincipalName(const std::string& user_principal_name,
                            std::string* user_name,
                            std::string* realm,
                            std::string* normalized_user_principal_name) {
  std::vector<std::string> parts = base::SplitString(
      user_principal_name, "@", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  if (parts.size() != 2 || parts.at(0).empty() || parts.at(1).empty()) {
    LOG(ERROR) << "Failed to parse user principal name '" << user_principal_name
               << "'. Expected form 'user@some.realm'.";
    return false;
  }
  *user_name = parts.at(0);
  *realm = base::ToUpperASCII(parts.at(1));
  *normalized_user_principal_name = *user_name + "@" + *realm;
  return true;
}

bool FindToken(const std::string& in_str,
               char token_separator,
               const std::string& token,
               std::string* result) {
  std::vector<std::string> lines = base::SplitString(
      in_str, "\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  for (const std::string& line : lines) {
    size_t sep_pos = line.find(token_separator);
    if (sep_pos != std::string::npos) {
      std::string line_token;
      base::TrimWhitespaceASCII(line.substr(0, sep_pos), base::TRIM_ALL,
                                &line_token);
      if (line_token == token) {
        base::TrimWhitespaceASCII(line.substr(sep_pos + 1), base::TRIM_ALL,
                                  result);
        if (!result->empty())
          return true;
      }
    }
  }
  LOG(ERROR) << "Failed to find '" << token << "' in '" << in_str << "'";
  return false;
}

bool ParseGpoVersion(const std::string& str, unsigned int* version) {
  DCHECK(version);
  *version = 0;
  unsigned int version_hex = 0;
  if (sscanf(str.c_str(), "%u (0x%08x)", version, &version_hex) != 2 ||
      *version != version_hex)
    return false;

  return true;
}

bool ParseGpFlags(const std::string& str, int* gp_flags) {
  for (int flag = 0; flag < static_cast<int>(arraysize(kGpFlagsStr)); ++flag) {
    if (str == kGpFlagsStr[flag]) {
      *gp_flags = flag;
      return true;
    }
  }
  return false;
}

bool Contains(const std::string& str, const std::string& substr) {
  return str.find(substr) != std::string::npos;
}

}  // namespace internal
}  // namespace authpolicy
