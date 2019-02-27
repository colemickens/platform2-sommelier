// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A helper which provides methods to extract data from a pam_handle.

#include "chaps/pam_helper.h"

#include <security/pam_appl.h>
#include <security/pam_modules.h>

#include <vector>

#include <base/logging.h>
#include <brillo/secure_blob.h>

#include "chaps/chaps_utility.h"

using brillo::SecureBlob;
using std::string;
using std::vector;

namespace chaps {

namespace {

const char* kUserKey = "chaps_user_key";
const char* kPasswordKey = "chaps_password_key";

static void FreeUser(pam_handle_t* pam_handle, void* data, int error_status) {
  if (data != NULL) {
    string* user = reinterpret_cast<string*>(data);
    delete user;
  }
}

static void FreePassword(pam_handle_t* pam_handle,
                         void* data,
                         int error_status) {
  if (data != NULL) {
    SecureBlob* password = reinterpret_cast<SecureBlob*>(data);
    password->clear();
    delete password;
  }
}

}  // namespace

PamHelper::~PamHelper() {}

bool PamHelper::GetPamUser(pam_handle_t* pam_handle, string* user) {
  const char* user_raw;
  int result;

  result = pam_get_user(pam_handle, &user_raw, NULL);
  if (result != PAM_SUCCESS) {
    LOG(ERROR) << "Could not get the pam user name: "
               << pam_strerror(pam_handle, result);
    return false;
  }

  *user = string(user_raw);
  // Note: user_raw is the actual data, so should not be overwritten or freed.
  return true;
}

bool PamHelper::GetPamPassword(pam_handle_t* pam_handle,
                               bool old_password,
                               SecureBlob* data) {
  const char* data_raw;
  int result;
  int pam_item_type;

  pam_item_type = old_password ? PAM_OLDAUTHTOK : PAM_AUTHTOK;

  result = pam_get_item(pam_handle, pam_item_type,
                        reinterpret_cast<const void**>(&data_raw));
  if (result != PAM_SUCCESS || data_raw == NULL) {
    // TODO(rmcilroy): Prompt for password if possible.
    LOG(WARNING) << "Could not get pam password: "
                 << pam_strerror(pam_handle, result);
    return false;
  }

  string data_string(data_raw, strlen(data_raw));
  SecureBlob tmp(data_string);
  data->swap(tmp);

  // Note: data_raw is the actual data, so should not be overwritten or freed.
  return true;
}

bool PamHelper::SaveUserAndPassword(pam_handle_t* pam_handle,
                                    const string& user,
                                    const SecureBlob& password) {
  void* user_data = new string(user);
  if (pam_set_data(pam_handle, kUserKey, user_data, FreeUser) != PAM_SUCCESS) {
    LOG(ERROR) << "Could not save user name in PAM handle";
    return false;
  }

  void* password_data = new SecureBlob(password);
  if (pam_set_data(pam_handle, kPasswordKey, password_data, FreePassword) !=
      PAM_SUCCESS) {
    LOG(ERROR) << "Could not save password in PAM handle";
    return false;
  }

  return true;
}

bool PamHelper::RetrieveUserAndPassword(pam_handle_t* pam_handle,
                                        string* user,
                                        SecureBlob* password) {
  CHECK(user);
  CHECK(password);

  const void* user_data;
  if (pam_get_data(pam_handle, kUserKey, &user_data) != PAM_SUCCESS) {
    VLOG(1) << "Could not retrieve user name from PAM handle";
    return false;
  }
  *user = *reinterpret_cast<const string*>(user_data);

  const void* password_data;
  if (pam_get_data(pam_handle, kPasswordKey, &password_data) != PAM_SUCCESS) {
    LOG(INFO) << "Could not retrieve password from PAM handle";
    return false;
  }
  SecureBlob tmp(*reinterpret_cast<const SecureBlob*>(password_data));
  password->swap(tmp);

  return true;
}

bool PamHelper::PutEnvironmentVariable(pam_handle_t* pam_handle,
                                       const string& name,
                                       const string& value) {
  string env_var = name + "=" + value;
  return pam_putenv(pam_handle, env_var.c_str()) == PAM_SUCCESS;
}

bool PamHelper::GetEnvironmentVariable(pam_handle_t* pam_handle,
                                       const string& name,
                                       string* value) {
  CHECK(value);
  const char* value_raw = pam_getenv(pam_handle, name.c_str());
  if (value_raw == NULL || value_raw[0] == '\0')
    return false;
  *value = string(value_raw);
  return true;
}

}  // namespace chaps
