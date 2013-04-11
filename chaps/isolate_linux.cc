// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This provides some utility functions to do with chaps isolate support.

#include "chaps/isolate.h"

#include <grp.h>
#include <pwd.h>

#include <string>

#include <base/file_path.h>
#include <base/file_util.h>
#include <chromeos/secure_blob.h>

using std::string;
using base::FilePath;
using chromeos::SecureBlob;

namespace chaps {

namespace {

const FilePath::CharType kIsolateFilePath[] =
    FILE_PATH_LITERAL("/var/lib/chaps/isolates/");

} // namespace

IsolateCredentialManager::IsolateCredentialManager() { }

IsolateCredentialManager::~IsolateCredentialManager() { }

bool IsolateCredentialManager::GetCurrentUserIsolateCredential(
    SecureBlob* isolate_credential) {
  CHECK(isolate_credential);

  uid_t uid = getuid();
  struct passwd* pwd = getpwuid(uid);
  if (!pwd) {
    PLOG(ERROR) << "Failed to get user information for current user.";
    return false;
  }

  return GetUserIsolateCredential(pwd->pw_name, isolate_credential);
}

bool IsolateCredentialManager::GetUserIsolateCredential(
    const string& user, SecureBlob* isolate_credential) {
  CHECK(isolate_credential);

  string credential_string;
  FilePath credential_file = FilePath(kIsolateFilePath).Append(user);
  if (!(file_util::PathExists(credential_file) &&
        file_util::ReadFileToString(credential_file, &credential_string))) {
    LOG(INFO) << "Failed to find or read isolate credential for user "
               << user;
    return false;
  }
  SecureBlob new_isolate_credential(credential_string);
  if (new_isolate_credential.size() != kIsolateCredentialBytes) {
    return false;
  }

  *isolate_credential = new_isolate_credential;
  return true;
}

} // namespace chaps
