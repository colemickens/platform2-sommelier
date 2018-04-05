// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "smbprovider/in_memory_credential_store.h"

#include <base/logging.h>
#include <libpasswordprovider/password.h>

namespace smbprovider {

InMemoryCredentialStore::InMemoryCredentialStore() = default;
InMemoryCredentialStore::~InMemoryCredentialStore() = default;

bool InMemoryCredentialStore::AddCredentials(
    const std::string& mount_root,
    const std::string& workgroup,
    const std::string& username,
    const base::ScopedFD& password_fd) {
  if (HasCredentials(mount_root)) {
    return false;
  }

  credentials_.emplace(mount_root, SmbCredentials(workgroup, username,
                                                  GetPassword(password_fd)));
  return true;
}

bool InMemoryCredentialStore::AddEmptyCredentials(
    const std::string& mount_root) {
  if (HasCredentials(mount_root)) {
    return false;
  }

  credentials_.emplace(mount_root, SmbCredentials());
  return true;
}

bool InMemoryCredentialStore::RemoveCredentials(const std::string& mount_root) {
  return credentials_.erase(mount_root) != 0;
}

bool InMemoryCredentialStore::HasCredentials(
    const std::string& mount_root) const {
  return credentials_.count(mount_root) == 1;
}

size_t InMemoryCredentialStore::CredentialsCount() const {
  return credentials_.size();
}

const SmbCredentials& InMemoryCredentialStore::GetCredentials(
    const std::string& mount_root) const {
  DCHECK(HasCredentials(mount_root));

  return credentials_.find(mount_root)->second;
}

}  // namespace smbprovider
