// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "authpolicy/samba_interface_internal.h"

#include <base/strings/string_split.h>
#include <base/strings/string_util.h>

namespace authpolicy {
namespace internal {

bool ParseUserPrincipalName(const std::string& user_principal_name,
                            std::string* out_user_name,
                            std::string* out_realm,
                            std::string* out_workgroup,
                            std::string* out_normalized_user_principal_name,
                            ErrorType* out_error) {
  // If there is no '@' in |user_principal_name|, at_pos is std::string::npos
  // and the call to substr(at_pos + 1) might throw if std::string::npos + 1 !=
  // 0. Hence, we the test for at_pos == std::string::npos.
  const size_t at_pos = user_principal_name.find('@');
  bool error = at_pos == std::string::npos;
  if (!error) {
    *out_user_name = user_principal_name.substr(0, at_pos);
    *out_realm = base::ToUpperASCII(user_principal_name.substr(at_pos + 1));
    const size_t dot_pos = out_realm->find('.');
    *out_workgroup = out_realm->substr(0, dot_pos);
    *out_normalized_user_principal_name = *out_user_name + "@" + *out_realm;
    error = dot_pos == std::string::npos || out_user_name->empty() ||
            out_realm->empty() || out_workgroup->empty();
  }
  if (error) {
    LOG(ERROR) << "Failed to parse user principal name '" << user_principal_name
               << "'. Expected form 'user@workgroup.domain'.";
    *out_error = ERROR_PARSE_UPN_FAILED;
    return false;
  }
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

}  // namespace internal
}  // namespace authpolicy
