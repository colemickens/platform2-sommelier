// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/login_screen_storage.h"

#include <algorithm>
#include <memory>
#include <utility>

#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/files/scoped_temp_dir.h>
#include <brillo/errors/error.h>
#include <gtest/gtest.h>

#include "login_manager/login_screen_storage/login_screen_storage_index.pb.h"
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

// Checks that two given lists of login screen storage keys are equal.
bool KeyListsAreEqual(std::vector<std::string> lhs,
                      std::vector<std::string> rhs) {
  sort(lhs.begin(), lhs.end());
  sort(rhs.begin(), rhs.end());
  return lhs == rhs;
}

// Checks that a given instace of |LoginScreenStorageIndex| has a set of keys
// equal to |expected_keys|.
bool IndexKeysEqualTo(const LoginScreenStorageIndex& index,
                      std::vector<std::string> expected_keys) {
  auto keys = index.keys();
  std::vector<std::string> keys_vec(keys.begin(), keys.end());
  return KeyListsAreEqual(std::move(keys_vec), std::move(expected_keys));
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
  base::FilePath GetKeyPath(const std::string& key) const {
    return base::FilePath(storage_path_)
        .Append(secret_util::StringToSafeFilename(key));
  }

  base::FilePath GetIndexPath() const {
    return base::FilePath(storage_path_)
        .Append(kLoginScreenStorageIndexFilename);
  }

  LoginScreenStorageIndex LoadIndex() const {
    const base::FilePath index_path = GetIndexPath();
    EXPECT_TRUE(base::PathExists(index_path));

    std::string index_blob;
    LoginScreenStorageIndex index;
    if (base::ReadFileToString(index_path, &index_blob))
      index.ParseFromString(index_blob);
    return index;
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

TEST_P(LoginScreenStorageTest, CannotRetrieveDeletedKey) {
  base::ScopedFD value_fd = MakeValueFD(kTestValue);

  brillo::ErrorPtr error;
  storage_->Store(&error, kTestKey, GetParam(), value_fd);
  EXPECT_FALSE(error.get());

  storage_->Delete(kTestKey);

  brillo::dbus_utils::FileDescriptor out_value_fd;
  storage_->Retrieve(&error, kTestKey, &out_value_fd);
  EXPECT_TRUE(error.get());
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

TEST_F(LoginScreenStorageTestPeristent, OnlyStoredKeysAreListedInIndex) {
  const std::string kDifferentTestKey = "different_test_key";
  base::DeleteFile(storage_path_, /*recursive=*/true);
  brillo::ErrorPtr error;

  {
    base::ScopedFD value_fd = MakeValueFD(kTestValue);
    storage_->Store(&error, kTestKey,
                    MakeMetadata(/*clear_on_session_exit=*/false), value_fd);
    EXPECT_FALSE(error.get());
    EXPECT_TRUE(KeyListsAreEqual(storage_->ListKeys(), {kTestKey}));
    EXPECT_TRUE(IndexKeysEqualTo(LoadIndex(), {kTestKey}));
  }

  // Index contains both keys after adding a diffrent key/value pair.
  {
    base::ScopedFD value_fd = MakeValueFD(kTestValue);
    storage_->Store(&error, kDifferentTestKey,
                    MakeMetadata(/*clear_on_session_exit=*/false), value_fd);
    EXPECT_FALSE(error.get());
    EXPECT_TRUE(
        KeyListsAreEqual(storage_->ListKeys(), {kTestKey, kDifferentTestKey}));
    EXPECT_TRUE(IndexKeysEqualTo(LoadIndex(), {kTestKey, kDifferentTestKey}));
  }

  // Index doesn't contain a key after overwriting it with an in-memory value,
  // but index still contains other keys.
  {
    base::ScopedFD value_fd = MakeValueFD(kTestValue);
    storage_->Store(&error, kTestKey,
                    MakeMetadata(/*clear_on_session_exit=*/true), value_fd);
    EXPECT_FALSE(error.get());
    // |kTestKey| should still be listed as a key, but shouldn't be present in
    // index.
    EXPECT_TRUE(
        KeyListsAreEqual(storage_->ListKeys(), {kTestKey, kDifferentTestKey}));
    EXPECT_TRUE(IndexKeysEqualTo(LoadIndex(), {kDifferentTestKey}));
  }

  // Index doesn't contain a key after deleting it.
  {
    storage_->Delete(kDifferentTestKey);
    EXPECT_TRUE(KeyListsAreEqual(storage_->ListKeys(), {kTestKey}));
    EXPECT_TRUE(IndexKeysEqualTo(LoadIndex(), {}));
  }
}

}  // namespace login_manager
