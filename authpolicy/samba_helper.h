// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AUTHPOLICY_SAMBA_HELPER_H_
#define AUTHPOLICY_SAMBA_HELPER_H_

#include <string>

namespace authpolicy {

class Anonymizer;

// Group policy flags.
const int kGpFlagAllEnabled = 0x00;
const int kGpFlagUserDisabled = 0x01;
const int kGpFlagMachineDisabled = 0x02;
const int kGpFlagAllDisabled = 0x03;
const int kGpFlagCount = 0x04;
const int kGpFlagInvalid = 0x04;

extern const char* const kGpFlagsStr[];

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

// Returns true if the string contains the given substring.
bool Contains(const std::string& str, const std::string& substr);

// Converts a valid GUID (see base::IsValidGUID()) to an octet string, see e.g.
// http://stackoverflow.com/questions/1545630/searching-for-a-objectguid-in-ad.
// Returns an empty string on error.
std::string GuidToOctetString(const std::string& guid);

// Converts an octet string to a GUID. Inverse of GuidToOctetString(). Only for
// testing! Just performs basic size checks, no strict format checks. Returns an
// empty string on error.
std::string OctetStringToGuidForTesting(const std::string& octet_str);

// Logs |str| to INFO, prepending |header|. Splits |str| into lines and logs the
// lines. This works around a restriction of syslog of 8kb per log and fixes
// unreadable logs where \n is replaced by #012. Anonymizes logs with
// |anonymizer| to remove sensitive data.
void LogLongString(const char* header,
                   const std::string& str,
                   Anonymizer* anonymizer);

}  // namespace authpolicy

#endif  // AUTHPOLICY_SAMBA_HELPER_H_
