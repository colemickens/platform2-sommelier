// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_CRYPTOHOME_H_
#define CHROMEOS_CRYPTOHOME_H_

#include <base/file_path.h>
#include <string>

namespace chromeos {
namespace cryptohome {
namespace home {

// Returns the common prefix under which the mount points for user homes are
// created.
FilePath GetUserPathPrefix();

// Returns the common prefix under which the mount points for root homes are
// created.
FilePath GetRootPathPrefix();

// Returns the path at which the user home for |username| will be mounted.
// Returns "" for failures.
FilePath GetUserPath(const std::string& username);

// Returns the path at which the root home for |username| will be mounted.
// Returns "" for failures.
FilePath GetRootPath(const std::string& username);

// Returns the path at which the daemon |daemon| should store per-user data.
FilePath GetDaemonPath(const std::string& username, const std::string& daemon);

// Checks whether |sanitized| has the format of a sanitized username.
bool IsSanitizedUserName(const std::string& sanitized);

// Returns a sanitized form of |username|. For x != y, SanitizeUserName(x) !=
// SanitizeUserName(y).
std::string SanitizeUserName(const std::string& username);

// Overrides the common prefix under which the mount points for user homes are
// created. This is used for testing only.
void SetUserHomePrefix(const std::string& prefix);

// Overrides the common prefix under which the mount points for root homes are
// created. This is used for testing only.
void SetRootHomePrefix(const std::string& prefix);

// Overrides the path of the system salt. This is used for testing only.
// DEPRECATED: If you are using this, please move to SetSystemSalt().
void SetSystemSaltPath(const std::string& path);

// Overrides the contents of the system salt.
// salt should be non-NULL and non-empty when attempting to avoid filesystem
// usage in tests.
// Note:
// (1) Never mix usage with SetSystemSaltPath().
// (2) Ownership of the pointer stays with the caller.
void SetSystemSalt(std::string* salt);

// Returns the system salt.
std::string* GetSystemSalt();

} // namespace home
} // namespace cryptohome
} // namespace chromeos

#endif // CHROMEOS_CRYPTOHOME_H_
