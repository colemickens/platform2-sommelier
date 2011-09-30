// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_CRYPTOHOME_H_
#define CHROMEOS_CRYPTOHOME_H_

#include <base/file_path.h>
#include <string>

namespace chromeos {
namespace cryptohome {
namespace home {

// Returns the path at which the user home for |username| will be mounted.
// Returns "" for failures.
FilePath GetUserPath(const std::string& username);

// Returns the path at which the root home for |username| will be mounted.
// Returns "" for failures.
FilePath GetRootPath(const std::string& username);

// Returns the path at which the daemon |daemon| should store per-user data.
FilePath GetDaemonPath(const std::string& username, const std::string& daemon);

// Returns a sanitized form of |username|. For x != y, SanitizeUserName(x) !=
// SanitizeUserName(y).
std::string SanitizeUserName(const std::string& username);

} // namespace home
} // namespace cryptohome
} // namespace chromeos

#endif // CHROMEOS_CRYPTOHOME_H_
