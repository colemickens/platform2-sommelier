// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A token manager provides a set of methods for login agents to create and
// validate token files.  This is not currently used on ChromeOS, where
// Cryptohome does this job.

#include "chaps/token_file_manager.h"

#include <string>

#include <base/logging.h>
#include <base/macros.h>
#include <brillo/secure_blob.h>

#include "chaps/chaps_utility.h"

using base::FilePath;
using brillo::SecureBlob;
using std::string;

namespace chaps {

TokenFileManager::TokenFileManager(uid_t chapsd_uid, gid_t chapsd_gid)
    : chapsd_uid_(chapsd_uid), chapsd_gid_(chapsd_gid) {
  ignore_result(chapsd_uid_);
  ignore_result(chapsd_gid_);
}

TokenFileManager::~TokenFileManager() {}

bool TokenFileManager::GetUserTokenPath(const string& user,
                                        FilePath* token_path) {
  NOTREACHED();
  return false;
}

bool TokenFileManager::CreateUserTokenDirectory(const FilePath& token_path) {
  NOTREACHED();
  return false;
}

bool TokenFileManager::CheckUserTokenPermissions(const FilePath& token_path) {
  NOTREACHED();
  return false;
}

bool TokenFileManager::SaltAuthData(const FilePath& token_path,
                                    const SecureBlob& auth_data,
                                    SecureBlob* salted_auth_data) {
  NOTREACHED();
  return false;
}

}  // namespace chaps
