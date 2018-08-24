// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SMBPROVIDER_CREDENTIAL_STORE_H_
#define SMBPROVIDER_CREDENTIAL_STORE_H_

#include <memory>
#include <string>

#include <base/files/file_util.h>
#include <base/macros.h>
#include <libpasswordprovider/password.h>

namespace smbprovider {

struct SmbCredential;

// Gets a password_provider::Password object from |password_fd|. The data has to
// be in the format of "{password_length}{password}". If the read fails, this
// returns an empty unique_ptr.
std::unique_ptr<password_provider::Password> GetPassword(
    const base::ScopedFD& password_fd);

// Manages the credential for a given mount root. There can only be one
// credential per unique mount root.
class CredentialStore {
 public:
  CredentialStore();
  virtual ~CredentialStore();

  // Adds the credential for |mount_root| to the store. This will return false
  // if a credential already exists for the given |mount_root|.
  virtual bool AddCredential(const std::string& mount_root,
                             SmbCredential credential) = 0;

  // Adds an empty set of credential for |mount_root|. This will return false
  // if a credential already exists for the given |mount_root|.
  virtual bool AddEmptyCredential(const std::string& mount_root) = 0;

  // Removes credential for |mount_root|. This will return false if credential
  // do not exist for |mount_root|.
  virtual bool RemoveCredential(const std::string& mount_root) = 0;

  // Returns true if credential exist for |mount_root|. This returns true if
  // the credential exist but are empty.
  virtual bool HasCredential(const std::string& mount_root) const = 0;

  // Returns the number of credential the store currently has.
  virtual size_t CredentialCount() const = 0;

  // Samba authentication function callback. DCHECKS that the buffer lengths are
  // non-zero. Returns false when buffer lengths cannot support credential
  // length or when credential is not found for |share_path|.
  bool GetAuthentication(const std::string& share_path,
                         char* workgroup,
                         int32_t workgroup_length,
                         char* username,
                         int32_t username_length,
                         char* password,
                         int32_t password_length) const;

 protected:
  // Returns the credential for |mount_root|. Error if there isn't a credential
  // for |mount_root|.
  virtual const SmbCredential& GetCredential(
      const std::string& mount_root) const = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(CredentialStore);
};

}  // namespace smbprovider

#endif  // SMBPROVIDER_CREDENTIAL_STORE_H_
