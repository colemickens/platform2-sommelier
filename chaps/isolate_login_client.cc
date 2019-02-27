// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A client which deals with logging a user onto a particular isolate in
// Chaps.

#include "chaps/isolate_login_client.h"

#include <string>

#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <brillo/secure_blob.h>

#include "chaps/chaps_utility.h"
#include "chaps/isolate.h"
#include "chaps/token_file_manager.h"
#include "chaps/token_manager_client.h"

using base::FilePath;
using brillo::SecureBlob;
using std::string;

namespace chaps {

IsolateLoginClient::IsolateLoginClient(
    IsolateCredentialManager* isolate_manager,
    TokenFileManager* file_manager,
    TokenManagerClient* token_manager)
    : isolate_manager_(isolate_manager),
      file_manager_(file_manager),
      token_manager_(token_manager) {}

IsolateLoginClient::~IsolateLoginClient() {}

bool IsolateLoginClient::LoginUser(const string& user,
                                   const SecureBlob& auth_data) {
  LOG(INFO) << "Login user " << user;
  SecureBlob isolate_credential;

  // Log into the user's isolate.
  isolate_manager_->GetUserIsolateCredential(user, &isolate_credential);
  bool new_isolate_created;
  if (!token_manager_->OpenIsolate(&isolate_credential, &new_isolate_created)) {
    LOG(ERROR) << "Failed to open isolate for user " << user;
    return false;
  }

  if (new_isolate_created) {
    LOG(INFO) << "Created new isolate for user " << user;
    // A new isolate was created, save the isolate credential passed back.
    if (!isolate_manager_->SaveIsolateCredential(user, isolate_credential)) {
      LOG(ERROR) << "Failed to write new isolate credential for users " << user;
      return false;
    }
  }

  // Load their token into the isolate.
  FilePath token_path;
  if (!file_manager_->GetUserTokenPath(user, &token_path) &&
      !file_manager_->CreateUserTokenDirectory(token_path)) {
    LOG(ERROR) << "Failed to find or locate token " << token_path.value();
    return false;
  }
  if (!file_manager_->CheckUserTokenPermissions(token_path)) {
    LOG(ERROR) << "Failed load token due to incorrect permissions "
               << token_path.value();
    return false;
  }
  SecureBlob salted_auth_data;
  if (!file_manager_->SaltAuthData(token_path, auth_data, &salted_auth_data)) {
    LOG(ERROR) << "Failed to salt authorization data.";
    return false;
  }
  int slot_id;
  if (!token_manager_->LoadToken(isolate_credential, token_path,
                                 salted_auth_data, user, &slot_id)) {
    return false;
  }
  return true;
}

bool IsolateLoginClient::LogoutUser(const string& user) {
  LOG(INFO) << "Logout user " << user;
  SecureBlob isolate_credential;

  if (!isolate_manager_->GetUserIsolateCredential(user, &isolate_credential)) {
    LOG(ERROR) << "Could not find isolate credential to logout " << user;
    return false;
  }
  token_manager_->CloseIsolate(isolate_credential);
  return true;
}

bool IsolateLoginClient::ChangeUserAuth(const string& user,
                                        const SecureBlob& old_auth_data,
                                        const SecureBlob& new_auth_data) {
  LOG(INFO) << "Change token auth for user " << user;

  FilePath token_path;
  if (!file_manager_->GetUserTokenPath(user, &token_path) ||
      !file_manager_->CheckUserTokenPermissions(token_path)) {
    LOG(ERROR) << "Aborting change user auth due to invalid token "
               << token_path.value();
    return false;
  }

  SecureBlob salted_old_auth_data;
  if (!file_manager_->SaltAuthData(token_path, old_auth_data,
                                   &salted_old_auth_data)) {
    LOG(ERROR) << "Failed to salt old authorization data.";
    return false;
  }

  SecureBlob salted_new_auth_data;
  if (!file_manager_->SaltAuthData(token_path, new_auth_data,
                                   &salted_new_auth_data)) {
    LOG(ERROR) << "Failed to salt new authorization data.";
    return false;
  }

  token_manager_->ChangeTokenAuthData(token_path, salted_old_auth_data,
                                      salted_new_auth_data);

  return true;
}

}  // namespace chaps
