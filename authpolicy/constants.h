// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AUTHPOLICY_CONSTANTS_H_
#define AUTHPOLICY_CONSTANTS_H_

namespace authpolicy {

// Commands for the parser.
extern const char kCmdParseRealmInfo[];
extern const char kCmdParseWorkgroup[];
extern const char kCmdParseAccountInfo[];
extern const char kCmdParseUserGpoList[];
extern const char kCmdParseDeviceGpoList[];
extern const char kCmdParseUserPreg[];
extern const char kCmdParseDevicePreg[];
extern const char kCmdParseTgtLifetime[];

// Env variable for the Kerberos keytab (machine password).
extern const char kKrb5KTEnvKey[];
// Env variable for the Kerberos credentials cache.
extern const char kKrb5CCEnvKey[];
// Env variable for krb5.conf file.
extern const char kKrb5ConfEnvKey[];
// Prefix for some environment variable values that specify a file path.
extern const char kFilePrefix[];

// Prefix for Active Directory account ids. A prefixed |account_id| is usually
// called |account_id_key|. Must match Chromium AccountId::kKeyAdIdPrefix.
// Exposed for unit tests.
extern const char kActiveDirectoryPrefix[];

// Net ads search keys.
extern const char kSearchObjectGUID[];
extern const char kSearchSAMAccountName[];
extern const char kSearchDisplayName[];
extern const char kSearchGivenName[];

enum ExitCodes {
  EXIT_CODE_OK = 0,
  EXIT_CODE_BAD_COMMAND = 1,
  EXIT_CODE_FIND_TOKEN_FAILED = 2,
  EXIT_CODE_READ_INPUT_FAILED = 3,
  EXIT_CODE_PARSE_INPUT_FAILED = 4,
  EXIT_CODE_WRITE_OUTPUT_FAILED = 5,
};

enum class PolicyScope {
  USER,     // User policy
  MACHINE,  // Machine/device policy
};

}  // namespace authpolicy

#endif  // AUTHPOLICY_CONSTANTS_H_
