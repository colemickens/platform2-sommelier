// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AUTHPOLICY_STUB_COMMON_H_
#define AUTHPOLICY_STUB_COMMON_H_

#include <string>

// Common helper methods for stub executables.

namespace authpolicy {

extern const int kExitCodeOk;
extern const int kExitCodeError;

extern const char kUserPrincipal[];              // Valid user principal.
extern const char kInvalidUserPrincipal[];       // Triggers parse error.
extern const char kNonExistingUserPrincipal[];   // Triggers bad user error.
extern const char kNetworkErrorUserPrincipal[];  // Triggers network error.
extern const char kAccessDeniedUserPrincipal[];  // Triggers access denied.
extern const char kKdcRetryUserPrincipal[];      // Triggers retry if krb5.conf.
extern const char kInsufficientQuotaUserPrincipal[];  // Triggers quota error.

extern const char kMachineName[];         // Valid machine name.
extern const char kMachinePrincipal[];    // Corresponding machine principal.
extern const char kTooLongMachineName[];  // Triggers name-too-long error.
extern const char kBadMachineName[];      // Triggers bad machine name error.

extern const char kPassword[];         // Valid password.
extern const char kWrongPassword[];    // Triggers bad password error.
extern const char kExpiredPassword[];  // Triggers expired password error.

// Returns |argv[1] + " " + argv[2] + " " + ... + argv[argc-1]|.
std::string GetCommandLine(int argc, const char* const* argv);

// Shortcut for base::StartsWith with case-sensitive comparison.
bool StartsWithCaseSensitive(const std::string& str, const char* search_for);

// Returns true if the string contains the given substring.
bool Contains(const std::string& str, const std::string& substr);

// Writes to stdout and stderr.
void WriteOutput(const std::string& stdout_str, const std::string& stderr_str);

// Reads the keytab file path from the environment. Returns an empty string on
// error.
std::string GetKeytabFilePath();

// Reads the Kerberos configuration file path from the environment. Returns an
// empty string on error.
std::string GetKrb5ConfFilePath();

}  // namespace authpolicy

#endif  // AUTHPOLICY_STUB_COMMON_H_
