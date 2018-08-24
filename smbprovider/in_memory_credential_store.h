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
#include "smbprovider/smb_credential.h"

namespace smbprovider {

class InMemoryCredentialStore
    : public CredentialStore,
      public base::SupportsWeakPtr<InMemoryCredentialStore> {
 public:
  InMemoryCredentialStore();
  ~InMemoryCredentialStore() override;

  // CredentialStore overrides.
  bool AddCredential(const std::string& mount_root,
                     SmbCredential credential) override;
  bool AddEmptyCredential(const std::string& mount_root) override;
  bool RemoveCredential(const std::string& mount_root) override;

  bool HasCredential(const std::string& mount_root) const override;

  size_t CredentialCount() const override;

 protected:
  // This is called by CredentialStore::GetAuthentication when accessing the
  // credential. HasCredential() should always be called before this. If the
  // |mount_root| isn't found, this will trigger a DCHECK crash.
  const SmbCredential& GetCredential(
      const std::string& mount_root) const override;

 private:
  // Mapping of mount_root : credential.
  std::map<std::string, SmbCredential> credential_;

  DISALLOW_COPY_AND_ASSIGN(InMemoryCredentialStore);
};

}  // namespace smbprovider

#endif  // SMBPROVIDER_IN_MEMORY_CREDENTIAL_STORE_H_
