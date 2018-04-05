// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SMBPROVIDER_CREDENTIAL_STORE_H_
#define SMBPROVIDER_CREDENTIAL_STORE_H_

#include <memory>
#include <string>
#include <utility>

#include <base/files/file_util.h>
#include <base/macros.h>

namespace password_provider {
class Password;
}  // namespace password_provider

namespace smbprovider {

struct SmbCredentials {
  std::string workgroup;
  std::string username;
  std::unique_ptr<password_provider::Password> password;

  SmbCredentials() = default;
  SmbCredentials(const std::string& workgroup,
                 const std::string& username,
                 std::unique_ptr<password_provider::Password> password)
      : workgroup(workgroup),
        username(username),
        password(std::move(password)) {}

  SmbCredentials(SmbCredentials&& credentials) = default;

  DISALLOW_COPY_AND_ASSIGN(SmbCredentials);
};

// Manages the credentials for a given mount root. There can only be one set of
// credentials per unique mount root.
class CredentialStore {
 public:
  CredentialStore();
  virtual ~CredentialStore();

  // Adds the credentials for |mount_root| to the store. This will return false
  // if a credential already exists for the given |mount_root|.
  virtual bool AddCredentials(const std::string& mount_root,
                              const std::string& workgroup,
                              const std::string& username,
                              const base::ScopedFD& password_fd) = 0;

  // Adds an empty set of credentials for |mount_root|. This will return false
  // if a credential already exists for the given |mount_root|.
  virtual bool AddEmptyCredentials(const std::string& mount_root) = 0;

  // Removes credentials for |mount_root|. This will return false if credentials
  // do not exist for |mount_root|.
  virtual bool RemoveCredentials(const std::string& mount_root) = 0;

  // Returns true if credentials exist for |mount_root|. This returns true if
  // the credentials exist but are empty.
  virtual bool HasCredentials(const std::string& mount_root) const = 0;

  // Returns the number of credentials the store currently has.
  virtual size_t CredentialsCount() const = 0;

  // Samba authentication function callback.
  void GetAuthentication(const std::string& share_path,
                         char* workgroup,
                         int32_t workgroup_length,
                         char* username,
                         int32_t username_length,
                         char* password,
                         int32_t password_length) const;

 protected:
  // Returns the credentials for |mount_root|. Error if there are no credentials
  // for |mount_root|.
  virtual const SmbCredentials& GetCredentials(
      const std::string& mount_root) const = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(CredentialStore);
};

}  // namespace smbprovider

#endif  // SMBPROVIDER_CREDENTIAL_STORE_H_
