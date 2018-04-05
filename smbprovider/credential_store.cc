// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "smbprovider/credential_store.h"

#include <libpasswordprovider/password.h>

namespace smbprovider {

namespace {

// Returns true if |buffer_length| is large enough to contain |str|.
bool CanBufferHoldString(const std::string& str, int32_t buffer_length) {
  return static_cast<int32_t>(str.size()) + 1 <= buffer_length;
}

// Returns true if |buffer_length| is large enough to contain |password|.
bool CanBufferHoldPassword(
    const std::unique_ptr<password_provider::Password>& password,
    int32_t buffer_length) {
  DCHECK(password);

  return static_cast<int32_t>(password->size()) + 1 <= buffer_length;
}

// Sets the first element in the buffer to be a null terminator.
void SetBufferEmpty(char* buffer) {
  DCHECK(buffer);

  buffer[0] = '\0';
}

// Copies |str| to |buffer| and adds a null terminator at the end.
void CopyStringToBuffer(const std::string& str, char* buffer) {
  DCHECK(buffer);

  strncpy(buffer, str.c_str(), str.size());
  buffer[str.size()] = '\0';
}

// Copies |password| to |buffer| and adds a null terminator at the end.
void CopyPasswordToBuffer(
    const std::unique_ptr<password_provider::Password>& password,
    char* buffer) {
  DCHECK(password);
  DCHECK(buffer);

  strncpy(buffer, password->GetRaw(), password->size());
  buffer[password->size()] = '\0';
}

// Checks that the credentials can be inputted given the buffer sizes. Returns
// false if the buffers are too small or if the credentials are empty.
bool CanInputCredentials(int32_t workgroup_length,
                         int32_t username_length,
                         int32_t password_length,
                         const SmbCredentials& credentials) {
  if (!CanBufferHoldString(credentials.workgroup, workgroup_length) ||
      !CanBufferHoldString(credentials.username, username_length)) {
    LOG(ERROR) << "Credential buffers are too small for input.";
    return false;
  }

  if (credentials.password &&
      !CanBufferHoldPassword(credentials.password, password_length)) {
    LOG(ERROR) << "Password buffer is too small for input.";
    return false;
  }

  return true;
}

// Populates the |credentials| into the specified buffers. CanInputCredentials()
// should be called first in order to verify the buffers can contain the
// credentials.
void PopulateCredentials(const SmbCredentials& credentials,
                         char* workgroup_buffer,
                         char* username_buffer,
                         char* password_buffer) {
  DCHECK(workgroup_buffer);
  DCHECK(username_buffer);
  DCHECK(password_buffer);

  CopyStringToBuffer(credentials.workgroup, workgroup_buffer);
  CopyStringToBuffer(credentials.username, username_buffer);

  const bool empty_password = !credentials.password;
  if (empty_password) {
    SetBufferEmpty(password_buffer);
  } else {
    CopyPasswordToBuffer(credentials.password, password_buffer);
  }
}

}  // namespace

std::unique_ptr<password_provider::Password> GetPassword(
    const base::ScopedFD& password_fd) {
  size_t password_length = 0;

  // Read sizeof(size_t) bytes from the file to get the password length.
  bool success = base::ReadFromFD(password_fd.get(),
                                  reinterpret_cast<char*>(&password_length),
                                  sizeof(password_length));
  if (!success) {
    LOG(ERROR) << "Could not read password from file.";
    return std::unique_ptr<password_provider::Password>();
  }

  if (password_length == 0) {
    // Return empty password since there is no password.
    return std::unique_ptr<password_provider::Password>();
  }

  return password_provider::Password::CreateFromFileDescriptor(
      password_fd.get(), password_length);
}

CredentialStore::CredentialStore() = default;
CredentialStore::~CredentialStore() = default;

bool CredentialStore::GetAuthentication(const std::string& share_path,
                                        char* workgroup,
                                        int32_t workgroup_length,
                                        char* username,
                                        int32_t username_length,
                                        char* password,
                                        int32_t password_length) const {
  DCHECK_GT(workgroup_length, 0);
  DCHECK_GT(username_length, 0);
  DCHECK_GT(password_length, 0);

  if (!HasCredentials(share_path)) {
    SetBufferEmpty(workgroup);
    SetBufferEmpty(username);
    SetBufferEmpty(password);
    return false;
  }

  const SmbCredentials& credentials = GetCredentials(share_path);
  if (!CanInputCredentials(workgroup_length, username_length, password_length,
                           credentials)) {
    SetBufferEmpty(workgroup);
    SetBufferEmpty(username);
    SetBufferEmpty(password);
    return false;
  }

  PopulateCredentials(credentials, workgroup, username, password);
  return true;
}

}  // namespace smbprovider
