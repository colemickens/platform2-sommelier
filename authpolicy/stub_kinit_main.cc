// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Stub implementation of kinit. Does not talk to server, but simply returns
// fixed responses to predefined input.

#include <string>

#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/logging.h>
#include <base/strings/string_util.h>
#include <base/strings/stringprintf.h>

#include "authpolicy/platform_helper.h"
#include "authpolicy/samba_helper.h"
#include "authpolicy/stub_common.h"

namespace authpolicy {
namespace {

// kinit error messages. stub_kinit reproduces kinit errors because authpolicy
// reads and interprets error messages from stdout/stderr.
const char kNonExistingPrincipalErrorFormat[] =
    "kinit: Client '%s' not found in Kerberos database while getting initial "
    "credentials";
const char kWrongPasswordError[] =
    "kinit: Preauthentication failed while getting initial credentials";
const char kPasswordExpiredStdout[] =
    "Password expired.  You must change it now.";
const char kPasswordExpiredStderr[] =
    "Cannot read password while getting initial credentials";
const char kNetworkError[] = "Cannot resolve network address for KDC in realm";
const char kCannotContactKdc[] = "Cannot contact any KDC";
const char kKdcIpKey[] = "kdc = [";

// For a given |machine_name|, tests if the |command_line| starts with the
// corresponding machine principal (using a testing realm)..
bool TestMachinePrincipal(const std::string& command_line,
                          const std::string& machine_name) {
  const std::string machine_principal =
      base::ToUpperASCII(machine_name) + "$@" + kRealm;
  return StartsWithCaseSensitive(command_line, machine_principal.c_str());
}

// Checks whether the Kerberos configuration file contains the KDC IP.
bool Krb5ConfContainsKdcIp() {
  std::string krb5_conf_path = GetKrb5ConfFilePath();
  CHECK(!krb5_conf_path.empty());

  std::string krb5_conf;
  CHECK(base::ReadFileToString(base::FilePath(krb5_conf_path), &krb5_conf));
  return Contains(krb5_conf, kKdcIpKey);
}

int HandleCommandLine(const std::string& command_line) {
  // Read the password from stdin.
  std::string password;
  if (!ReadPipeToString(STDIN_FILENO, &password)) {
    LOG(ERROR) << "Failed to read password";
    return kExitCodeError;
  }

  // Stub non-existing account error.
  if (StartsWithCaseSensitive(command_line, kNonExistingUserPrincipal)) {
    WriteOutput("",
                base::StringPrintf(kNonExistingPrincipalErrorFormat,
                                   kNonExistingUserPrincipal));
    return kExitCodeError;
  }

  // Stub network error error.
  if (StartsWithCaseSensitive(command_line, kNetworkErrorUserPrincipal)) {
    WriteOutput("", kNetworkError);
    return kExitCodeError;
  }

  // Stub kinit retry if the krb5.conf contains the KDC IP.
  if (StartsWithCaseSensitive(command_line, kKdcRetryUserPrincipal)) {
    if (Krb5ConfContainsKdcIp()) {
      WriteOutput("", kCannotContactKdc);
      return kExitCodeError;
    }
    return kExitCodeOk;
  }

  // Stub valid user principal. Switch behavior based on password.
  if (StartsWithCaseSensitive(command_line, kUserPrincipal)) {
    // Stub wrong password error.
    if (password == kWrongPassword) {
      WriteOutput("", kWrongPasswordError);
      return kExitCodeError;
    }

    // Stub expired password error.
    if (password == kExpiredPassword) {
      WriteOutput(kPasswordExpiredStdout, kPasswordExpiredStderr);
      return kExitCodeError;
    }

    // Stub valid password.
    if (password == kPassword)
      return kExitCodeOk;

    LOG(ERROR) << "UNHANDLED PASSWORD " << password;
    return kExitCodeError;
  }

  // Stub valid machine authentication.
  if (TestMachinePrincipal(command_line, kMachineName) ||
      TestMachinePrincipal(command_line, kEmptyGpoMachineName) ||
      TestMachinePrincipal(command_line, kGpoDownloadErrorMachineName) ||
      TestMachinePrincipal(command_line, kOneGpoMachineName) ||
      TestMachinePrincipal(command_line, kTwoGposMachineName)) {
    // Machine authentication requires a keytab, not a password.
    CHECK(password.empty());
    std::string keytab_path = GetKeytabFilePath();
    CHECK(!keytab_path.empty());
    return kExitCodeOk;
  }

  LOG(ERROR) << "UNHANDLED COMMAND LINE " << command_line;
  return kExitCodeError;
}

}  // namespace
}  // namespace authpolicy

int main(int argc, char* argv[]) {
  std::string command_line = authpolicy::GetCommandLine(argc, argv);
  return authpolicy::HandleCommandLine(command_line);
}
