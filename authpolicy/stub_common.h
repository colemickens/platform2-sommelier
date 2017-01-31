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

extern const char kKeytabEnvKey[];
extern const char kKeytabPrefix[];

// Returns |argv[1] + " " + argv[2] + " " + ... + argv[argc-1]|.
std::string GetCommandLine(int argc, const char* const* argv);

// Shortcut for base::StartsWith with case-sensitive comparison.
bool StartsWithCaseSensitive(const std::string& str, const char* search_for);

// Writes to stdout and stderr.
void WriteOutput(const std::string& stdout_str, const std::string& stderr_str);

// Reads the keytab file path from the environment. Returns an empty string on
// error.
std::string GetKeytabFilePath();

}  // namespace authpolicy

#endif  // AUTHPOLICY_STUB_COMMON_H_
