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

// Default realm.
extern const char kRealm[];

// Default, valid user name.
extern const char kUserName[];
// Default, valid user principal.
extern const char kUserPrincipal[];
// Triggers a parse error in SambaInterface code (UPN malformed).
extern const char kInvalidUserPrincipal[];
// Triggers bad user error in kinit and net ads join (user does not exist).
extern const char kNonExistingUserPrincipal[];
// Triggers network error in kinit and net ads join.
extern const char kNetworkErrorUserPrincipal[];
// Triggers an access denied error in net ads join (user cannot add machines).
extern const char kAccessDeniedUserPrincipal[];
// Triggers an error in kinit if krb5.conf contains the KDC IP, which causes
// SambaInterface to retry kinit without KDC IP in krb5.conf.
extern const char kKdcRetryUserPrincipal[];
// Triggers quota error in net ads join (user cannot add additional machines).
extern const char kInsufficientQuotaUserPrincipal[];
// Triggers stub_kinit to produce a TGT that stub_klist interprets as expired.
extern const char kExpiredTgtUserPrincipal[];
// Triggers net ads search to return |kPasswordChangedAccountId| as objectGUID.
extern const char kPasswordChangedUserPrincipal[];
// Corresponding user name.
extern const char kPasswordChangedUserName[];

// Misc account information, used to test whether they're properly parsed and
// encoded.
extern const char kDisplayName[];
extern const char kGivenName[];
extern const uint64_t kPwdLastSet;
extern const uint32_t kUserAccountControl;

// Default, valid account id (aka objectGUID).
extern const char kAccountId[];
// Triggers a net ads search error when searching for this objectGUID.
extern const char kBadAccountId[];
// Triggers pwdLastSet=0 in net ads search.
extern const char kExpiredPasswordAccountId[];
// Triggers pwdLastSet=0 and a userAccountControl flag to never expire the
// password in net ads search.
extern const char kNeverExpirePasswordAccountId[];
// Triggers a different pwdLastSet timestamp in net ads search.
extern const char kPasswordChangedAccountId[];
// User name corresponding to |kPasswordChangedAccountId|.
extern const char kPasswordChangedUserName[];

// Default, valid Kerberos crendentials cache contents (in particular, TGT).
extern const char kValidKrb5CCData[];
// Triggers stub_klist to think that the TGT expired.
extern const char kExpiredKrb5CCData[];

// Default, valid password.
extern const char kPassword[];
// Triggers a wrong/bad password error in kinit.
extern const char kWrongPassword[];
// Triggers expired password error in kinit.
extern const char kExpiredPassword[];

// Default, valid machine name.
extern const char kMachineName[];
// Triggers an error indicating that the machine name is too long.
extern const char kTooLongMachineName[];
// Triggers an invalid machine name error (machine name contains invalid chars).
extern const char kInvalidMachineName[];
// Triggers bad machine error in kinit (machine doesn't exist), but not in join.
extern const char kNonExistingMachineName[];
// Triggers a completely empty GPO list.
extern const char kEmptyGpoMachineName[];
// Triggers a GPO download error.
extern const char kGpoDownloadErrorMachineName[];
// Triggers downloading one GPO with user/machine versions > 0 and no flags.
extern const char kOneGpoMachineName[];
// Triggers downloading two GPOs with user/machine versions > 0 and no flags.
extern const char kTwoGposMachineName[];
// Triggers downloading a GPO with user version 0.
extern const char kZeroUserVersionMachineName[];
// Triggers downloading a GPO with the kGpFlagUserDisabled flag set.
extern const char kDisableUserFlagMachineName[];
// Triggers kinit to be retried a few times for the machine TGT (simulates that
// the account hasn't propagated yet).
extern const char kPropagationRetryMachineName[];

// How many times an account propagation error is simulated if
// |kPropagationRetryMachineName| is used.
const int kNumPropagationRetries = 15;

// Stub GPO GUID, triggers a "download" of testing GPO 1 in stub_smbclient.
extern const char kGpo1Guid[];
// Stub GPO GUID, triggers a "download" of testing GPO 2 in stub_smbclient.
extern const char kGpo2Guid[];
// Stub GPO GUID, triggers a GPO download error in stub_smbclient.
extern const char kErrorGpoGuid[];

// Filename of stub GPO 1 file. This PREG file is written by tests and
// stub_smbclient can be triggered to "download" it, e.g. by using
// kOneGpoMachineName.
extern const char kGpo1Filename[];
// Filename of stub GPO 2 file. "Download" can be triggered by using
// kTwoGposMachineName.
extern const char kGpo2Filename[];

// Returns |argv[1] + " " + argv[2] + " " + ... + argv[argc-1]|.
std::string GetCommandLine(int argc, const char* const* argv);

// Gets the arg value following |name|, e.g. in a command line
// "kinit -c krb5cc_file", calling GetArgValue("-c") would return 'krb5cc_file'.
std::string GetArgValue(int argc, const char* const* argv, const char* name);

// Shortcut for base::StartsWith with case-sensitive comparison.
bool StartsWithCaseSensitive(const std::string& str, const char* search_for);

// Writes to stdout and stderr.
void WriteOutput(const std::string& stdout_str, const std::string& stderr_str);

// Reads the keytab file path from the environment. Returns an empty string on
// error.
std::string GetKeytabFilePath();

// Reads the Kerberos configuration file path from the environment. Returns an
// empty string on error.
std::string GetKrb5ConfFilePath();

// Reads the Kerberos credentials cache file path from the environment. Returns
// an empty string on error.
std::string GetKrb5CCFilePath();

}  // namespace authpolicy

#endif  // AUTHPOLICY_STUB_COMMON_H_
