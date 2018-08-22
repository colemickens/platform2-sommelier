// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SMBPROVIDER_SMB_CREDENTIAL_H_
#define SMBPROVIDER_SMB_CREDENTIAL_H_

#include <memory>
#include <string>
#include <utility>

#include <base/macros.h>
#include <libpasswordprovider/password.h>

namespace smbprovider {

struct SmbCredential {
  std::string workgroup;
  std::string username;
  std::unique_ptr<password_provider::Password> password;

  SmbCredential() = default;
  SmbCredential(const std::string& workgroup,
                const std::string& username,
                std::unique_ptr<password_provider::Password> password)
      : workgroup(workgroup),
        username(username),
        password(std::move(password)) {}

  SmbCredential(SmbCredential&& other) = default;
  SmbCredential& operator=(SmbCredential&& other) = default;

  DISALLOW_COPY_AND_ASSIGN(SmbCredential);
};

}  // namespace smbprovider

#endif  // SMBPROVIDER_SMB_CREDENTIAL_H_
