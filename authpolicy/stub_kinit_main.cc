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

// Helper file for simulating account propagation issues.
const char kPropagationTestFile[] = "propagation_test";

// Returns upper-cased |machine_name|$@REALM.
std::string MakeMachinePrincipal(const std::string& machine_name) {
  return base::ToUpperASCII(machine_name) + "$@" + kRealm;
}

// For a given |machine_name|, tests if the |command_line| starts with the
// corresponding machine principal (using a testing realm)..
bool TestMachinePrincipal(const std::string& command_line,
                          const std::string& machine_name) {
  const std::string machine_principal = MakeMachinePrincipal(machine_name);
  return StartsWithCaseSensitive(command_line, machine_principal.c_str());
}

// Returns true if |command_line| contains a machine principle and not a user
// principle.
bool HasMachinePrincipal(const std::string& command_line) {
  return Contains(command_line, MakeMachinePrincipal(""));
}

// Returns false for the first |kNumPropagationRetries| times the method is
// called and true afterwards. Used to simulate account propagation errors. Only
// works once per test. Uses a test file internally, where each time a byte is
// appended to count retries. Note that each invokation usually happens in a
// separate process, so a static memory location can't be used for counting.
bool HasStubAccountPropagated() {
  const base::FilePath test_dir =
      base::FilePath(GetKrb5ConfFilePath()).DirName();
  base::FilePath test_path = test_dir.Append(kPropagationTestFile);
  int64_t size;
  if (!base::GetFileSize(test_path, &size))
    size = 0;
  if (size == kNumPropagationRetries)
    return true;

  // Note: base::WriteFile triggers a seccomp failure, so do it old-school.
  base::ScopedFILE test_file(fopen(test_path.value().c_str(), "a"));
  CHECK(test_file);
  const char zero = 0;
  CHECK_EQ(1U, fwrite(&zero, 1, 1, test_file.get()));
  return false;
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

  // Stub network error.
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

  // Handle machine principles.
  if (HasMachinePrincipal(command_line)) {
    // Machine authentication requires a keytab, not a password.
    CHECK(password.empty());
    std::string keytab_path = GetKeytabFilePath();
    CHECK(!keytab_path.empty());

    // Stub account propagation error.
    if (TestMachinePrincipal(command_line, kPropagationRetryMachineName) &&
        !HasStubAccountPropagated()) {
      WriteOutput(
          "",
          base::StringPrintf(
              kNonExistingPrincipalErrorFormat,
              MakeMachinePrincipal(kPropagationRetryMachineName).c_str()));
      return kExitCodeError;
    }

    // Stub non-existent machine error (e.g. machine got deleted from ACtive
    // Directory).
    if (TestMachinePrincipal(command_line, kNonExistingMachineName)) {
      // Note: Same error as if the account hasn't propagated yet.
      WriteOutput("",
                  base::StringPrintf(
                      kNonExistingPrincipalErrorFormat,
                      MakeMachinePrincipal(kNonExistingMachineName).c_str()));
      return kExitCodeError;
    }

    // All other machine principles just pass.
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
