// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SMBPROVIDER_IN_MEMORY_CREDENTIAL_STORE_H_
#define SMBPROVIDER_IN_MEMORY_CREDENTIAL_STORE_H_

#include <map>
#include <string>

#include <base/files/file_util.h>
#include <base/macros.h>
#include <base/memory/weak_ptr.h>

#include "smbprovider/credential_store.h"

namespace smbprovider {

class InMemoryCredentialStore
    : public CredentialStore,
      public base::SupportsWeakPtr<InMemoryCredentialStore> {
 public:
  InMemoryCredentialStore();
  ~InMemoryCredentialStore() override;

  // CredentialStore overrides.
  bool AddCredentials(const std::string& mount_root,
                      const std::string& workgroup,
                      const std::string& username,
                      const base::ScopedFD& password_fd) override;
  bool AddEmptyCredentials(const std::string& mount_root) override;
  bool RemoveCredentials(const std::string& mount_root) override;

  bool HasCredentials(const std::string& mount_root) const override;

  size_t CredentialsCount() const override;

 protected:
  // This is called by CredentialStore::GetAuthentication when accessing the
  // credentials. HasCredentials() should always be called before this. If the
  // |mount_root| isn't found, this will trigger a DCHECK crash.
  const SmbCredentials& GetCredentials(
      const std::string& mount_root) const override;

 private:
  // Mapping of mount_root : credentials.
  std::map<std::string, SmbCredentials> credentials_;

  DISALLOW_COPY_AND_ASSIGN(InMemoryCredentialStore);
};

}  // namespace smbprovider

#endif  // SMBPROVIDER_IN_MEMORY_CREDENTIAL_STORE_H_
