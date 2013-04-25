// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A token manager provides a set of methods for login agents to create and
// validate token files.

#include "chaps/token_file_manager.h"

#include <grp.h>
#include <pwd.h>
#include <sys/stat.h>

#include <string>

#include <base/file_path.h>
#include <base/file_util.h>

using std::string;
using base::FilePath;

namespace chaps {

namespace {

const FilePath::CharType kTokenFilePath[] =
    FILE_PATH_LITERAL("/var/lib/chaps/tokens/");

const mode_t kTokenDirectoryPermissions = (S_IRUSR | S_IWUSR | S_IXUSR);
const mode_t kFilePermissionsMask = (S_IRWXU | S_IRWXG | S_IRWXO);
} // namespace

TokenFileManager::TokenFileManager(uid_t chapsd_uid, gid_t chapsd_gid)
    : chapsd_uid_(chapsd_uid),
      chapsd_gid_(chapsd_gid) { }

TokenFileManager::~TokenFileManager() { }

bool TokenFileManager::GetUserTokenPath(const string& user,
                                        FilePath* token_path) {
  CHECK(token_path);
  *token_path = FilePath(kTokenFilePath).Append(user);
  return file_util::DirectoryExists(*token_path);
}

bool TokenFileManager::CreateUserTokenDirectory(const FilePath& token_path) {
  if (file_util::DirectoryExists(token_path)) {
    LOG(ERROR) << "Tried to create " << token_path.value()
               << " which already exists";
    return false;
  }
  if (!file_util::CreateDirectory(token_path)) {
    LOG(ERROR) << "Failed to create " << token_path.value();
    return false;
  }

  // Change permissions to be readable and writable only by user chaps.
  if (chmod(token_path.value().c_str(), kTokenDirectoryPermissions)) {
    LOG(ERROR) << "Failed to change permissions of token directory "
               << token_path.value();
    return false;
  }
  if (chown(token_path.value().c_str(), chapsd_uid_, chapsd_gid_)) {
    LOG(ERROR) << "Failed to change ownership of token directory "
               << token_path.value();
    return false;
  }
  return true;
}

bool TokenFileManager::CheckUserTokenPermissions(const FilePath& token_path) {
  struct stat token_stat;
  if (stat(token_path.value().c_str(), &token_stat)) {
    LOG(ERROR) << "Failed to stat token directory " << token_path.value();
    return false;
  }
  if ((token_stat.st_mode & kFilePermissionsMask) !=
      kTokenDirectoryPermissions) {
    LOG(ERROR) << "Incorrect permissions on token directory "
               << token_path.value();
    return false;
  }
  if (token_stat.st_uid != chapsd_uid_) {
    LOG(ERROR) << "Incorrect owner for token directory " << token_path.value();
    return false;
  }
  if (token_stat.st_gid != chapsd_gid_) {
    LOG(ERROR) << "Incorrect group for token directory " << token_path.value();
    return false;
  }
  return true;
}

}
