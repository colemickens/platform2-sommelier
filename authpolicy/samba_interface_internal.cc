// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "authpolicy/samba_interface_internal.h"

#include <base/strings/string_split.h>
#include <base/strings/string_util.h>

namespace {

const char* kGpFlagsStr[] = {
  "0 GPFLAGS_ALL_ENABLED",
  "1 GPFLAGS_USER_SETTINGS_DISABLED",
  "2 GPFLAGS_MACHINE_SETTINGS_DISABLED",
  "3 GPFLAGS_ALL_DISABLED",
};

}  // namespace

namespace authpolicy {
namespace internal {

bool ParseUserPrincipalName(const std::string& user_principal_name,
                            std::string* out_user_name,
                            std::string* out_realm,
                            std::string* out_normalized_user_principal_name,
                            ErrorType* out_error) {
  std::vector<std::string> parts = base::SplitString(
      user_principal_name, "@", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  if (parts.size() != 2 || parts.at(0).empty() || parts.at(1).empty()) {
    LOG(ERROR) << "Failed to parse user principal name '" << user_principal_name
               << "'. Expected form 'user@some.realm'.";
    *out_error = ERROR_PARSE_UPN_FAILED;
    return false;
  }
  *out_user_name = parts.at(0);
  *out_realm = base::ToUpperASCII(parts.at(1));
  *out_normalized_user_principal_name = *out_user_name + "@" + *out_realm;
  return true;
}

bool FindToken(const std::string& in_str,
               char token_separator,
               const std::string& token,
               std::string* out_result) {
  std::vector<std::string> lines = base::SplitString(
      in_str, "\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  for (const std::string& line : lines) {
    size_t sep_pos = line.find(token_separator);
    if (sep_pos != std::string::npos) {
      std::string line_token;
      base::TrimWhitespaceASCII(line.substr(0, sep_pos), base::TRIM_ALL,
                                &line_token);
      if (line_token == token) {
        std::string result;
        base::TrimWhitespaceASCII(line.substr(sep_pos + 1), base::TRIM_ALL,
                                  &result);
        if (!result.empty()) {
          *out_result = result;
          return true;
        }
      }
    }
  }
  return false;
}

bool ParseGpoVersion(const std::string& str, unsigned int* out_num) {
  int num = 0, num_hex = 0;
  if (sscanf(str.c_str(), "%u (0x%08x)", &num, &num_hex) != 2 || num != num_hex)
    return false;

  DCHECK(out_num);
  *out_num = num;
  return true;
}

bool ParseGpFlags(const std::string& str, int* out_gp_flags) {
  for (int n = 0; n < static_cast<int>(arraysize(kGpFlagsStr)); ++n) {
    if (str == kGpFlagsStr[n]) {
      *out_gp_flags = n;
      return true;
    }
  }
  return false;
}

}  // namespace internal
}  // namespace authpolicy
