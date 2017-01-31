// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "authpolicy/stub_common.h"

#include <base/files/file_util.h>
#include <base/strings/string_util.h>

namespace authpolicy {

const int kExitCodeOk = 0;
const int kExitCodeError = 1;

const char kKeytabEnvKey[] = "KRB5_KTNAME";
const char kKeytabPrefix[] = "FILE:";

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

void WriteOutput(const std::string& stdout_str, const std::string& stderr_str) {
  CHECK(base::WriteFileDescriptor(
      STDOUT_FILENO, stdout_str.c_str(), stdout_str.size()));
  CHECK(base::WriteFileDescriptor(
      STDERR_FILENO, stderr_str.c_str(), stderr_str.size()));
}

std::string GetKeytabFilePath() {
  const char* env_value = getenv(kKeytabEnvKey);
  if (!env_value)
    return std::string();

  std::string prefixed_keytab_path = env_value;
  if (!StartsWithCaseSensitive(prefixed_keytab_path, kKeytabPrefix))
    return std::string();

  return prefixed_keytab_path.substr(strlen(kKeytabPrefix));
}

}  // namespace authpolicy
