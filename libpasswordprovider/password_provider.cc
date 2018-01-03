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

int RevokeKey(const char* type, const char* description) {
  key_serial_t key_serial =
      find_key_by_type_and_desc(type, description, 0 /* dest_keyring */);
  if (key_serial == -1) {
    return errno;
  }

  int result = keyctl_revoke(key_serial);
  if (result == -1) {
    return errno;
  }

  return result;
}

}  // namespace

PasswordProvider::PasswordProvider() {}

bool PasswordProvider::SavePassword(const Password& password) {
  DCHECK_GT(password.size(), 0);
  DCHECK(password.GetRaw());

  key_serial_t keyring_id = add_key(kKeyringKeyType, kKeyringDescription, NULL,
                                    0, KEY_SPEC_PROCESS_KEYRING);
  if (keyring_id == -1) {
    PLOG(ERROR) << "Error creating keyring.";
    return false;
  }

  key_serial_t key_serial =
      add_key(kPasswordKeyType, kPasswordKeyDescription, password.GetRaw(),
              password.size(), keyring_id);

  if (key_serial == -1) {
    PLOG(ERROR) << "Error adding key to keyring.";
    return false;
  }

  int result =
      keyctl_setperm(key_serial, KEY_POS_ALL | KEY_USR_READ | KEY_USR_SEARCH);
  if (result == -1) {
    PLOG(ERROR) << "Error setting permissions on key. ";
    return false;
  }

  return true;
}

std::unique_ptr<Password> PasswordProvider::GetPassword() {
  key_serial_t key_serial =
      find_key_by_type_and_desc(kPasswordKeyType, kPasswordKeyDescription, 0);
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

bool PasswordProvider::DiscardPassword() {
  int result = RevokeKey(kPasswordKeyType, kPasswordKeyDescription);
  if (result != 0) {
    PLOG(ERROR) << "Error revoking key.";
    return false;
  }

  result = RevokeKey(kKeyringKeyType, kKeyringDescription);
  if (result != 0) {
    PLOG(ERROR) << "Error revoking keyring.";
    return false;
  }

  return true;
}

}  // namespace password_provider
