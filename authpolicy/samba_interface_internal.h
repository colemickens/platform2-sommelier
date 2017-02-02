// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AUTHPOLICY_SAMBA_INTERFACE_INTERNAL_H_
#define AUTHPOLICY_SAMBA_INTERFACE_INTERNAL_H_

#include <string>
#include <vector>

#include <base/files/file_path.h>

namespace authpolicy {
namespace internal {

// Group policy flags.
const int kGpFlagUserDisabled = 0x01;
const int kGpFlagMachineDisabled = 0x02;
const int kGpFlagInvalid = 0x04;

// Parses user_name@some.realm into its components and normalizes (uppercases)
// the part behind the @. |user_name| is 'user_name', |realm| is |SOME.REALM|
// and |normalized_user_principal_name| is user_name@SOME.REALM.
bool ParseUserPrincipalName(const std::string& user_principal_name,
                            std::string* user_name,
                            std::string* realm,
                            std::string* normalized_user_principal_name);

// Parses the given |in_str| consisting of individual lines for
//   ... \n
//   |token| <token_separator> |result| \n
//   ... \n
// and returns the first non-empty |result|. Whitespace is trimmed.
bool FindToken(const std::string& in_str,
               char token_separator,
               const std::string& token,
               std::string* result);

// Parses a GPO version string, which consists of a number and the same number
// as base-16 hex number, e.g. '31 (0x0000001f)'.
bool ParseGpoVersion(const std::string& str, unsigned int* version);

// Parses a group policy flags string, which consists of a number 0-3 and a
// descriptive name. See |kGpFlag*| for possible values.
bool ParseGpFlags(const std::string& str, int* gp_flags);

}  // namespace internal
}  // namespace authpolicy

#endif  // AUTHPOLICY_SAMBA_INTERFACE_INTERNAL_H_
