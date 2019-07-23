// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/login_screen_storage.h"

#include <memory>

#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/files/scoped_temp_dir.h>
#include <brillo/errors/error.h>
#include <gtest/gtest.h>

#include "login_manager/proto_bindings/login_screen_storage.pb.h"
#include "login_manager/secret_util.h"

namespace login_manager {

namespace {

constexpr char kLoginScreenStoragePath[] = "login_screen_storage";
constexpr char kTestKey[] = "testkey";
constexpr char kTestValue[] = "testvalue";

LoginScreenStorageMetadata MakeMetadata(bool clear_on_session_exit) {
  LoginScreenStorageMetadata metadata;
  metadata.set_clear_on_session_exit(clear_on_session_exit);
  return metadata;
}

base::ScopedFD MakeValueFD(const std::string& value) {
  const std::vector<uint8_t> kValueVector =
      std::vector<uint8_t>(value.begin(), value.end());
  return secret_util::WriteSizeAndDataToPipe(kValueVector);
}

}  // namespace

class LoginScreenStorageTestBase : public ::testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(tmpdir_.CreateUniqueTempDir());
    storage_path_ = tmpdir_.GetPath().Append(kLoginScreenStoragePath);
    storage_ = std::make_unique<LoginScreenStorage>(storage_path_);
  }

 protected:
  base::FilePath GetKeyPath(const std::string& key) {
    return base::FilePath(storage_path_)
        .Append(secret_util::StringToSafeFilename(key));
  }

  base::ScopedTempDir tmpdir_;
  base::FilePath storage_path_;
  std::unique_ptr<LoginScreenStorage> storage_;
};

class LoginScreenStorageTest
    : public LoginScreenStorageTestBase,
      public testing::WithParamInterface<LoginScreenStorageMetadata> {};

TEST_P(LoginScreenStorageTest, StoreRetrieve) {
  base::ScopedFD value_fd = MakeValueFD(kTestValue);

  brillo::ErrorPtr error;
  storage_->Store(&error, kTestKey, GetParam(), value_fd);
  EXPECT_FALSE(error.get());

  brillo::dbus_utils::FileDescriptor out_value_fd;
  storage_->Retrieve(&error, kTestKey, &out_value_fd);
  EXPECT_FALSE(error.get());

  std::vector<uint8_t> value;
  EXPECT_TRUE(secret_util::ReadSecretFromPipe(out_value_fd.get(), &value));
  EXPECT_EQ(kTestValue, std::string(value.begin(), value.end()));

  // Writing a different value to make sure it will replace the old one.

  const char kDifferentValue[] = "different_value";
  base::ScopedFD different_value_fd = MakeValueFD(kDifferentValue);
  storage_->Store(&error, kTestKey, GetParam(), different_value_fd);
  EXPECT_FALSE(error.get());

  storage_->Retrieve(&error, kTestKey, &out_value_fd);
  EXPECT_FALSE(error.get());

  EXPECT_TRUE(secret_util::ReadSecretFromPipe(out_value_fd.get(), &value));
  EXPECT_EQ(kDifferentValue, std::string(value.begin(), value.end()));
}

INSTANTIATE_TEST_CASE_P(
    LoginScreenStorageTest,
    LoginScreenStorageTest,
    testing::Values(MakeMetadata(/*clear_on_session_exit=*/false),
                    MakeMetadata(/*clear_on_session_exit=*/true)));

using LoginScreenStorageTestPeristent = LoginScreenStorageTestBase;

TEST_F(LoginScreenStorageTestPeristent, StoreOverridesPersistentKey) {
  brillo::ErrorPtr error;
  {
    base::ScopedFD value_fd = MakeValueFD(kTestValue);
    EXPECT_TRUE(base::CreateDirectory(storage_path_));
    storage_->Store(&error, kTestKey,
                    MakeMetadata(/*clear_on_session_exit=*/false), value_fd);
    EXPECT_FALSE(error.get());
  }

  const base::FilePath key_path = GetKeyPath(kTestKey);
  EXPECT_TRUE(base::PathExists(key_path));

  {
    base::ScopedFD value_fd = MakeValueFD(kTestValue);
    storage_->Store(&error, kTestKey,
                    MakeMetadata(/*clear_on_session_exit=*/true), value_fd);
    EXPECT_FALSE(error.get());
  }

  EXPECT_FALSE(base::PathExists(key_path));
}

TEST_F(LoginScreenStorageTestPeristent, StoreCreatesDirectoryIfNotExistant) {
  base::DeleteFile(storage_path_, /*recursive=*/true);

  base::ScopedFD value_fd = MakeValueFD(kTestValue);
  brillo::ErrorPtr error;
  storage_->Store(&error, kTestKey,
                  MakeMetadata(/*clear_on_session_exit=*/false), value_fd);
  EXPECT_FALSE(error.get());

  EXPECT_TRUE(base::DirectoryExists(storage_path_));
  EXPECT_TRUE(base::PathExists(GetKeyPath(kTestKey)));
}

}  // namespace login_manager
