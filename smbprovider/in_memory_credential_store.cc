// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "smbprovider/in_memory_credential_store.h"

#include <base/logging.h>
#include <libpasswordprovider/password.h>

namespace smbprovider {

InMemoryCredentialStore::InMemoryCredentialStore() = default;
InMemoryCredentialStore::~InMemoryCredentialStore() = default;

bool InMemoryCredentialStore::AddCredential(const std::string& mount_root,
                                            const std::string& workgroup,
                                            const std::string& username,
                                            const base::ScopedFD& password_fd) {
  if (HasCredential(mount_root)) {
    return false;
  }

  credential_.emplace(
      mount_root, SmbCredential(workgroup, username, GetPassword(password_fd)));
  return true;
}

bool InMemoryCredentialStore::AddEmptyCredential(
    const std::string& mount_root) {
  if (HasCredential(mount_root)) {
    return false;
  }

  credential_.emplace(mount_root, SmbCredential());
  return true;
}

bool InMemoryCredentialStore::RemoveCredential(const std::string& mount_root) {
  return credential_.erase(mount_root) != 0;
}

bool InMemoryCredentialStore::HasCredential(
    const std::string& mount_root) const {
  return credential_.count(mount_root) == 1;
}

size_t InMemoryCredentialStore::CredentialCount() const {
  return credential_.size();
}

const SmbCredential& InMemoryCredentialStore::GetCredential(
    const std::string& mount_root) const {
  DCHECK(HasCredential(mount_root));

  return credential_.find(mount_root)->second;
}

}  // namespace smbprovider
