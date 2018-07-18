// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libpasswordprovider/password_provider.h"

#include <keyutils.h>

#include "base/logging.h"
#include "libpasswordprovider/password.h"

namespace password_provider {

namespace {

constexpr char kKeyringDescription[] = "password keyring";
constexpr char kKeyringKeyType[] = "keyring";
constexpr char kPasswordKeyDescription[] = "password";
constexpr char kPasswordKeyType[] = "user";

key_serial_t RequestKey(const char* type, const char* description) {
  key_serial_t keyring_serial =
      find_key_by_type_and_desc(kKeyringKeyType, kKeyringDescription, 0);
  if (keyring_serial == -1) {
    // This is also called in cases where keys might not exist (e.g., cleaning
    // up on logout) so not finding any keys is not an error.
    VLOG(1) << "Error finding keyring. errno: " << errno;
    return keyring_serial;
  }

  return keyctl_search(keyring_serial, type, description, 0);
}

int RevokeKey(const char* type, const char* description) {
  key_serial_t key_serial = RequestKey(type, description);
  if (key_serial == -1) {
    return errno;
  }

  int result = keyctl_revoke(key_serial);
  if (result == -1) {
    return errno;
  }

  return result;
}

void HandleKeyError(const char* type, const char* description) {
  int result = RevokeKey(type, description);
  if (result != 0) {
    PLOG(ERROR) << "Error revoking key: " << description;
  }
}

}  // namespace

PasswordProvider::PasswordProvider() {}

bool PasswordProvider::SavePassword(const Password& password) const {
  DCHECK_GT(password.size(), 0);
  DCHECK(password.GetRaw());

  key_serial_t keyring_id = add_key(kKeyringKeyType, kKeyringDescription, NULL,
                                    0, KEY_SPEC_PROCESS_KEYRING);
  if (keyring_id == -1) {
    PLOG(ERROR) << "Error creating keyring.";
    return false;
  }

  int result =
      keyctl_setperm(keyring_id, KEY_POS_ALL | KEY_USR_VIEW | KEY_USR_READ |
                                     KEY_USR_SEARCH | KEY_USR_WRITE);

  if (result == -1) {
    PLOG(ERROR) << "Error setting permissions on keyring. ";
    return false;
  }

  key_serial_t key_serial =
      add_key(kPasswordKeyType, kPasswordKeyDescription, password.GetRaw(),
              password.size(), keyring_id);

  if (key_serial == -1) {
    PLOG(ERROR) << "Error adding key to keyring.";
    return false;
  }

  result =
      keyctl_setperm(key_serial, KEY_POS_ALL | KEY_USR_VIEW | KEY_USR_READ |
                                     KEY_USR_SEARCH | KEY_USR_WRITE);

  if (result == -1) {
    PLOG(ERROR) << "Error setting permissions on key. ";
    HandleKeyError(kPasswordKeyType, kPasswordKeyDescription);
    return false;
  }

  return true;
}

std::unique_ptr<Password> PasswordProvider::GetPassword() const {
  key_serial_t key_serial =
      RequestKey(kPasswordKeyType, kPasswordKeyDescription);
  if (key_serial == -1) {
    PLOG(WARNING) << "Could not find key.";
    return nullptr;
  }

  auto password = std::make_unique<Password>();
  if (!password->Init()) {
    LOG(ERROR) << "Error allocating buffer for password";
    return nullptr;
  }

  int result =
      keyctl_read(key_serial, password->GetMutableRaw(), password->max_size());
  if (result > password->max_size()) {
    LOG(ERROR) << "Password too large for buffer. Max size: "
               << password->max_size();
    return nullptr;
  }

  if (result == -1) {
    PLOG(ERROR) << "Error reading key.";
    return nullptr;
  }

  password->SetSize(result);
  return password;
}

bool PasswordProvider::DiscardPassword() const {
  int result = RevokeKey(kPasswordKeyType, kPasswordKeyDescription);
  if (result != 0) {
    // This is also called in cases where keys might not exist (e.g., cleaning
    // up on logout) so not finding any keys is not an error.
    VLOG(1) << "Error revoking key. errno: " << errno;
    return false;
  }

  return true;
}

}  // namespace password_provider
