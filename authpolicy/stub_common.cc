// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "authpolicy/stub_common.h"

#include <base/files/file_util.h>
#include <base/strings/string_util.h>

namespace authpolicy {

const int kExitCodeOk = 0;
const int kExitCodeError = 1;

const char kUserPrincipal[] = "user@REALM.COM";
const char kInvalidUserPrincipal[] = "user.REALM.COM";
const char kNonExistingUserPrincipal[] = "non_existing_user@REALM.COM";
const char kNetworkErrorUserPrincipal[] = "network_error_user@REALM.COM";
const char kAccessDeniedUserPrincipal[] = "access_denied_user@REALM.COM";
const char kKdcRetryUserPrincipal[] = "kdc_retry_user@REALM.COM";
const char kInsufficientQuotaUserPrincipal[] =
    "insufficient_quota_user@REALM.COM";

const char kMachineName[] = "testcomp";
const char kMachinePrincipal[] = "TESTCOMP$@REALM.COM";
const char kTooLongMachineName[] = "too_long_machine_name";
const char kBadMachineName[] = "bad?na:me";

const char kPassword[] = "p4zzw!5d";
const char kWrongPassword[] = "pAzzwI5d";
const char kExpiredPassword[] = "rootpw";

namespace {

const char kKeytabEnvKey[] = "KRB5_KTNAME";
const char kKrb5ConfEnvKey[] = "KRB5_CONFIG";
const char kFilePrefix[] = "FILE:";

// Looks up the environment variable with key |env_key|, which is expected to be
// 'FILE:<path>', and returns <path>. Returns and empty string if the variable
// does not exist or does not have the prefix.
std::string GetFileFromEnv(const char* env_key) {
  const char* env_value = getenv(env_key);
  if (!env_value)
    return std::string();

  // Remove FILE: prefix.
  std::string prefixed_path = env_value;
  if (!StartsWithCaseSensitive(prefixed_path, kFilePrefix))
    return std::string();

  return prefixed_path.substr(strlen(kFilePrefix));
}

}  // namespace

std::string GetCommandLine(int argc, const char* const* argv) {
  CHECK_GE(argc, 2);
  std::string command_line = argv[1];
  for (int n = 2; n < argc; ++n) {
    command_line += " ";
    command_line += argv[n];
  }
  return command_line;
}

bool StartsWithCaseSensitive(const std::string& str, const char* search_for) {
  return base::StartsWith(str, search_for, base::CompareCase::SENSITIVE);
}

bool Contains(const std::string& str, const std::string& substr) {
  return str.find(substr) != std::string::npos;
}

void WriteOutput(const std::string& stdout_str, const std::string& stderr_str) {
  CHECK(base::WriteFileDescriptor(
      STDOUT_FILENO, stdout_str.c_str(), stdout_str.size()));
  CHECK(base::WriteFileDescriptor(
      STDERR_FILENO, stderr_str.c_str(), stderr_str.size()));
}

std::string GetKeytabFilePath() {
  return GetFileFromEnv(kKeytabEnvKey);
}

std::string GetKrb5ConfFilePath() {
  return GetFileFromEnv(kKrb5ConfEnvKey);
}

}  // namespace authpolicy
