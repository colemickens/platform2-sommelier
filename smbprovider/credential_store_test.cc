// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/files/file_util.h>
#include <base/macros.h>
#include <gtest/gtest.h>
#include <libpasswordprovider/password.h>

#include "smbprovider/credential_store.h"
#include "smbprovider/smbprovider_test_helper.h"
#include "smbprovider/temp_file_manager.h"

namespace smbprovider {

class CredentialStoreTest : public testing::Test {
 public:
  CredentialStoreTest() = default;
  ~CredentialStoreTest() override = default;

 protected:
  TempFileManager temp_file_manager_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CredentialStoreTest);
};

TEST_F(CredentialStoreTest, ReturnsEmptyPasswordWithInvalidFd) {
  std::unique_ptr<password_provider::Password> password =
      GetPassword(base::ScopedFD());
  EXPECT_FALSE(password);
}

TEST_F(CredentialStoreTest, ReturnsEmptyPasswordWithEmptyPassword) {
  base::ScopedFD password_fd =
      WritePasswordToFile(&temp_file_manager_, "" /* password */);
  EXPECT_TRUE(password_fd.is_valid());

  // password_fd should be false since the password was empty.
  std::unique_ptr<password_provider::Password> password =
      GetPassword(password_fd);
  EXPECT_FALSE(password);
}

TEST_F(CredentialStoreTest, GetPasswordGetsValidPassword) {
  const std::string password = "test123";
  base::ScopedFD password_fd =
      WritePasswordToFile(&temp_file_manager_, password);
  EXPECT_TRUE(password_fd.is_valid());

  std::unique_ptr<password_provider::Password> password_ptr =
      GetPassword(password_fd);
  EXPECT_TRUE(password_ptr);

  EXPECT_EQ(password_ptr->size(), password.size());
  EXPECT_EQ(std::string(password_ptr->GetRaw()), password);
}

}  // namespace smbprovider
