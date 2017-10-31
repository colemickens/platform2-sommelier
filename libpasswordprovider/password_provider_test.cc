// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include <gtest/gtest.h>

#include "base/logging.h"
#include "libpasswordprovider/password.h"
#include "libpasswordprovider/password_provider.h"

namespace password_provider {

// Saving and retrieving password should succeed.
TEST(PasswordProvider, SaveAndGetPassword) {
  const std::string kPasswordStr("thepassword");
  Password password;
  ASSERT_TRUE(password.Init());
  memcpy(password.GetMutableRaw(), kPasswordStr.c_str(), kPasswordStr.size());
  password.SetSize(kPasswordStr.size());

  EXPECT_TRUE(SavePassword(password));
  std::unique_ptr<Password> retrieved_password = GetPassword();
  ASSERT_TRUE(retrieved_password);
  EXPECT_EQ(std::string(retrieved_password->GetRaw()), kPasswordStr);
  EXPECT_EQ(retrieved_password->size(), kPasswordStr.size());
}

// Reading password should fail if password was already discarded.
TEST(PasswordProvider, DiscardAndGetPassword) {
  const std::string kPasswordStr("thepassword");
  Password password;
  ASSERT_TRUE(password.Init());
  memcpy(password.GetMutableRaw(), kPasswordStr.c_str(), kPasswordStr.size());
  password.SetSize(kPasswordStr.size());

  EXPECT_TRUE(SavePassword(password));
  EXPECT_TRUE(DiscardPassword());
  std::unique_ptr<Password> retrieved_password = GetPassword();
  EXPECT_FALSE(retrieved_password);
}

// Retrieving a very long password should succeed.
TEST(PasswordProvider, GetLongPassword) {
  Password password;
  ASSERT_TRUE(password.Init());

  // Create a very long password
  auto long_password = std::make_unique<char[]>(password.max_size());
  memset(long_password.get(), 'a', password.max_size());
  std::string password_str(long_password.get());
  memcpy(password.GetMutableRaw(), long_password.get(), password_str.size());
  password.SetSize(password_str.size());

  EXPECT_TRUE(SavePassword(password));
  std::unique_ptr<Password> retrieved_password = GetPassword();
  ASSERT_TRUE(retrieved_password);
  EXPECT_EQ(std::string(retrieved_password->GetRaw()), password_str);
}

}  // namespace password_provider
