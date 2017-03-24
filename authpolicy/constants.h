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

// Env variable for the Kerberos keytab (machine password).
extern const char kKrb5KTEnvKey[];
// Env variable for the Kerberos credentials cache.
extern const char kKrb5CCEnvKey[];
// Env variable for krb5.conf file.
extern const char kKrb5ConfEnvKey[];
// Prefix for some environment variable values that specify a file path.
extern const char kFilePrefix[];

// Net ads search keys.
const char kSearchObjectGUID[] = "objectGUID";
const char kSearchSAMAccountName[] = "sAMAccountName";
const char kSearchDisplayName[] = "displayName";
const char kSearchGivenName[] = "givenName";

enum ExitCodes {
  EXIT_CODE_OK = 0,
  EXIT_CODE_BAD_COMMAND,
  EXIT_CODE_FIND_TOKEN_FAILED,
  EXIT_CODE_READ_INPUT_FAILED,
  EXIT_CODE_PARSE_INPUT_FAILED,
  EXIT_CODE_WRITE_OUTPUT_FAILED,
};

enum class PolicyScope {
  USER,     // User policy
  MACHINE,  // Machine/device policy
};

}  // namespace authpolicy

#endif  // AUTHPOLICY_CONSTANTS_H_
