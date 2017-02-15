// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Stub implementation of Samba net. Does not talk to server, but simply returns
// fixed responses to predefined input.

#include <string>

#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/files/scoped_file.h>
#include <base/logging.h>
#include <base/strings/string_util.h>
#include <base/strings/stringprintf.h>

#include "authpolicy/platform_helper.h"
#include "authpolicy/samba_interface_internal.h"
#include "authpolicy/stub_common.h"

namespace authpolicy {

namespace {

const char kStubKeytab[] = "Stub keytab file";

// Various stub error messages.
const char kSmbConfArgMissingError[] =
    "Can't load /etc/samba/smb.conf - run testparm to debug it";
const char kNetworkError[] = "No logon servers";
const char kWrongPasswordError[] =
    "Failed to join domain: failed to lookup DC info for domain 'REALM.COM' "
    "over rpc: Logon failure";
const char kJoinAccessDeniedError[] =
    "Failed to join domain: Failed to set account flags for machine account "
    "(NT_STATUS_ACCESS_DENIED)";
const char kMachineNameTooLongError[] =
    "Our netbios name can be at most %zd chars long, \"%s\" is %zd chars long\n"
    "Failed to join domain: The format of the specified computer name is "
    "invalid.";
const char kBadMachineNameError[] =
    "Failed to join domain: failed to join domain 'REALM.COM' over rpc: "
    "Improperly formed account name";
const char kInsufficientQuotaError[] =
    "Insufficient quota exists to complete the operation";

// Size limit for machine name.
const size_t kMaxMachineNameSize = 15;

// Stub net ads info response.
const char kStubInfo[] = R"!!!(LDAP server: 111.222.33.44
LDAP server name: dcname.realm.com
Realm: REALM.COM
Bind Path: dc=REALM,dc=COM
LDAP port: 389
Server time: Fri, 03 Feb 2017 05:24:05 PST
KDC server: 111.222.33.44
Server time offset: -91
Last machine account password change:
Wed, 31 Dec 1969 16:00:00 PST)!!!";

// Stub net ads gpo list response.
const char kStubGpoList[] = R"!!!(---------------------
name:   Local Policy
displayname:  Local Policy
version:  0 (0x00000000)
version_user:  0 (0x0000)
version_machine: 0 (0x0000)
filesyspath:  (null)
dspath:  (null)
options:  0 GPFLAGS_ALL_ENABLED
link:   (null)
link_type:  5 machine_extensions: (null)
user_extensions: (null))!!!";

// Stub net ads search response.
const char kStubSearch[] = R"!!!(Got 1 replies
objectClass: top
objectClass: person
objectClass: organizationalPerson
objectClass: user
cn: John Doe
sn: Doe
givenName: John
initials: JD
distinguishedName: CN=John Doe,OU=some-ou,DC=realm,DC=com
instanceType: 4
whenCreated: 20161018155136.0Z
whenChanged: 20170217134227.0Z
displayName: John Doe
uSNCreated: 287406
uSNChanged: 307152
name: John Doe
objectGUID: d3c6a5b1-be2f-49b9-8d03-1d7f6dedc1d7
userAccountControl: 512
badPwdCount: 0
codePage: 0
countryCode: 0
badPasswordTime: 131309487458845506
lastLogoff: 0
lastLogon: 131320568639495686
pwdLastSet: 131292078840924254
primaryGroupID: 513
objectSid: S-1-5-21-250062649-3667841115-373469193-1134
accountExpires: 9223372036854775807
logonCount: 1453
sAMAccountName: jdoe
sAMAccountType: 805306368
userPrincipalName: jdoe@chrome.lan
objectCategory: CN=Person,CN=Schema,CN=Configuration,DC=chrome,DC=lan
dSCorePropagationData: 20161024075536.0Z
dSCorePropagationData: 20161024075311.0Z
dSCorePropagationData: 20161019075502.0Z
dSCorePropagationData: 16010101000000.0Z
lastLogonTimestamp: 131318125471489990
msDS-SupportedEncryptionTypes: 0)!!!";

// Writes a fake keytab file.
void WriteKeytabFile() {
  std::string keytab_path = GetKeytabFilePath();
  CHECK(!keytab_path.empty());
  // Note: base::WriteFile triggers a seccomp failure, so do it old-school.
  base::ScopedFILE kt_file(fopen(keytab_path.c_str(), "w"));
  CHECK(kt_file);
  CHECK_EQ(1U, fwrite(kStubKeytab, strlen(kStubKeytab), 1, kt_file.get()));
}

// Reads the smb.conf file at |smb_conf_path| and extracts the netbios name.
std::string GetMachineNameFromSmbConf(const std::string& smb_conf_path) {
  std::string smb_conf;
  CHECK(base::ReadFileToString(base::FilePath(smb_conf_path), &smb_conf));
  std::string machine_name;
  CHECK(internal::FindToken(smb_conf, '=', "netbios name", &machine_name));
  return machine_name;
}

}  // namespace

// Handles a stub 'net ads join' call.
int HandleJoin(const std::string& command_line,
               const std::string& smb_conf_path) {
  // Read the password from stdin.
  std::string password;
  if (!ReadPipeToString(STDIN_FILENO, &password)) {
    LOG(ERROR) << "Failed to read password";
    return kExitCodeError;
  }
  const std::string kUserFlag = "-U ";

  // Read machine name from smb.conf.
  std::string machine_name = GetMachineNameFromSmbConf(smb_conf_path);
  CHECK(!machine_name.empty());

  // Stub too long machine name error.
  if (machine_name.size() > kMaxMachineNameSize) {
    WriteOutput(base::StringPrintf(kMachineNameTooLongError,
                                   kMaxMachineNameSize,
                                   machine_name.c_str(),
                                   machine_name.size()),
                "");
    return kExitCodeError;
  }

  // Stub bad machine name error.
  if (machine_name == base::ToUpperASCII(kBadMachineName)) {
    WriteOutput(kBadMachineNameError, "");
    return kExitCodeError;
  }

  // Stub insufficient quota error.
  if (internal::Contains(command_line,
                         kUserFlag + kInsufficientQuotaUserPrincipal)) {
    WriteOutput(kInsufficientQuotaError, "");
    return kExitCodeError;
  }

  // Stub non-existing account error (same error as 'wrong password' error).
  if (internal::Contains(command_line, kUserFlag + kNonExistingUserPrincipal)) {
    WriteOutput(kWrongPasswordError, "");
    return kExitCodeError;
  }

  // Stub network error.
  if (internal::Contains(command_line,
                         kUserFlag + kNetworkErrorUserPrincipal)) {
    WriteOutput("", kNetworkError);
    return kExitCodeError;
  }

  // Stub access denied error.
  if (internal::Contains(command_line,
                         kUserFlag + kAccessDeniedUserPrincipal)) {
    WriteOutput(kJoinAccessDeniedError, "");
    return kExitCodeError;
  }

  // Stub valid user principal. Switch behavior based on password.
  if (internal::Contains(command_line, kUserFlag + kUserPrincipal)) {
    // Stub wrong password.
    if (password == kWrongPassword) {
      WriteOutput(kWrongPasswordError, "");
      return kExitCodeError;
    }
    // Stub valid password.
    if (password == kPassword) {
      WriteKeytabFile();
      return kExitCodeOk;
    }
    LOG(ERROR) << "UNHANDLED PASSWORD " << password;
    return kExitCodeError;
  }

  LOG(ERROR) << "UNHANDLED COMMAND LINE " << command_line;
  return kExitCodeError;
}

int HandleCommandLine(const std::string& command_line,
                      const std::string& smb_conf_path) {
  // Stub net ads workgroup, return a fake workgroup.
  if (StartsWithCaseSensitive(command_line, "ads workgroup")) {
    WriteOutput("Workgroup: WOKGROUP", "");
    return kExitCodeOk;
  }

  // Stub net ads join.
  if (StartsWithCaseSensitive(command_line, "ads join"))
    return HandleJoin(command_line, smb_conf_path);

  // Stub net ads info, return stub information.
  if (StartsWithCaseSensitive(command_line, "ads info")) {
    WriteOutput(kStubInfo, "");
    return kExitCodeOk;
  }

  // Stub net ads gpo list, return stub GPO list.
  if (StartsWithCaseSensitive(command_line, "ads gpo list")) {
    WriteOutput("", kStubGpoList);
    return kExitCodeOk;
  }

  // Stub net ads search, return stub search result.
  if (StartsWithCaseSensitive(command_line, "ads search")) {
    WriteOutput(kStubSearch, "");
    return kExitCodeOk;
  }

  LOG(ERROR) << "UNHANDLED COMMAND LINE " << command_line;
  return kExitCodeError;
}

}  // namespace authpolicy

int main(int argc, char* argv[]) {
  // Find Samba configuration path ("-s" argument).
  std::string smb_conf_path;
  for (int n = 1; n < argc; ++n) {
    if (strcmp(argv[n], "-s") == 0 && n + 1 < argc) {
      smb_conf_path = argv[n + 1];
      break;
    }
  }
  if (smb_conf_path.empty()) {
    authpolicy::WriteOutput("", authpolicy::kSmbConfArgMissingError);
    return authpolicy::kExitCodeError;
  }

  const std::string command_line = authpolicy::GetCommandLine(argc, argv);
  return authpolicy::HandleCommandLine(command_line, smb_conf_path);
}
