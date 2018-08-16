// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include <base/macros.h>
#include <gtest/gtest.h>
#include <libpasswordprovider/password.h>

#include "smbprovider/in_memory_credential_store.h"
#include "smbprovider/smbprovider_test_helper.h"
#include "smbprovider/temp_file_manager.h"

namespace smbprovider {

namespace {
constexpr char kMountRoot[] = "smb://192.168.0.1/test";
constexpr char kWorkgroup[] = "domain";
constexpr char kUsername[] = "user1";
constexpr char kPassword[] = "admin";

constexpr int32_t kBufferSize = 256;
}  // namespace

class InMemoryCredentialStoreTest : public testing::Test {
 public:
  InMemoryCredentialStoreTest() = default;
  ~InMemoryCredentialStoreTest() override = default;

  bool AddCredential(const std::string& mount_root,
                     const std::string& workgroup,
                     const std::string& username,
                     const std::string& password) {
    return credential_.AddCredential(
        mount_root, workgroup, username,
        WritePasswordToFile(&temp_files_, password));
  }

  void ExpectCredentialsEqual(const std::string& mount_root,
                              const std::string& workgroup,
                              const std::string& username,
                              const std::string& password) {
    char workgroup_buffer[kBufferSize];
    char username_buffer[kBufferSize];
    char password_buffer[kBufferSize];

    EXPECT_TRUE(credential_.GetAuthentication(
        mount_root, workgroup_buffer, kBufferSize, username_buffer, kBufferSize,
        password_buffer, kBufferSize));

    EXPECT_EQ(std::string(workgroup_buffer), workgroup);
    EXPECT_EQ(std::string(username_buffer), username);
    EXPECT_EQ(std::string(password_buffer), password);
  }

 protected:
  TempFileManager temp_files_;
  InMemoryCredentialStore credential_;

 private:
  DISALLOW_COPY_AND_ASSIGN(InMemoryCredentialStoreTest);
};

TEST_F(InMemoryCredentialStoreTest, NoCredential) {
  // Verify the state of an empty |InMemoryCredentialStore|
  EXPECT_EQ(0, credential_.CredentialCount());
  EXPECT_FALSE(credential_.RemoveCredential(kMountRoot));
  EXPECT_EQ(0, credential_.CredentialCount());
  EXPECT_FALSE(credential_.HasCredential(kMountRoot));
}

TEST_F(InMemoryCredentialStoreTest, AddingCredential) {
  EXPECT_TRUE(AddCredential(kMountRoot, kWorkgroup, kUsername, kPassword));
  EXPECT_EQ(1, credential_.CredentialCount());
  EXPECT_TRUE(credential_.HasCredential(kMountRoot));
  ExpectCredentialsEqual(kMountRoot, kWorkgroup, kUsername, kPassword);
}

TEST_F(InMemoryCredentialStoreTest, BufferNullTerminatedWhenLengthTooSmall) {
  EXPECT_TRUE(AddCredential(kMountRoot, kWorkgroup, kUsername, kPassword));
  EXPECT_EQ(1, credential_.CredentialCount());
  EXPECT_TRUE(credential_.HasCredential(kMountRoot));

  // Initialize buffers with 1.
  char workgroup_buffer[kBufferSize] = {1};
  char username_buffer[kBufferSize] = {1};
  char password_buffer[kBufferSize] = {1};

  // Call the authentication function while passing 1 as the buffer sizes. This
  // should return false since the buffer sizes are too small.
  EXPECT_FALSE(credential_.GetAuthentication(
      kMountRoot, workgroup_buffer, 1 /* workgroup_length */, username_buffer,
      1 /* username_length */, password_buffer, 1 /* password_length */));

  // Buffers should be null-terminated.
  EXPECT_EQ('\0', workgroup_buffer[0]);
  EXPECT_EQ('\0', password_buffer[0]);
  EXPECT_EQ('\0', password_buffer[0]);
}

TEST_F(InMemoryCredentialStoreTest, BufferNullTerminatedWhenNoCredsFound) {
  // Initialize buffers with 1.
  char workgroup_buffer[kBufferSize] = {1};
  char username_buffer[kBufferSize] = {1};
  char password_buffer[kBufferSize] = {1};

  // This should return false when no credential are found.
  EXPECT_FALSE(credential_.GetAuthentication(
      kMountRoot, workgroup_buffer, kBufferSize, username_buffer, kBufferSize,
      password_buffer, kBufferSize));

  // Buffers should be null-terminated.
  EXPECT_EQ('\0', workgroup_buffer[0]);
  EXPECT_EQ('\0', password_buffer[0]);
  EXPECT_EQ('\0', password_buffer[0]);
}

TEST_F(InMemoryCredentialStoreTest, AddingEmptyCredential) {
  EXPECT_TRUE(credential_.AddEmptyCredential(kMountRoot));
  EXPECT_EQ(1, credential_.CredentialCount());
  EXPECT_TRUE(credential_.HasCredential(kMountRoot));
  ExpectCredentialsEqual(kMountRoot, "" /* workgroup */, "" /* username */,
                         "" /* password */);
}

TEST_F(InMemoryCredentialStoreTest, CredentialWithoutWorkgroup) {
  EXPECT_TRUE(
      AddCredential(kMountRoot, "" /* workgroup */, kUsername, kPassword));
  ExpectCredentialsEqual(kMountRoot, "" /* workgroup */, kUsername, kPassword);
}

TEST_F(InMemoryCredentialStoreTest, CredentialWithoutPassword) {
  EXPECT_TRUE(
      AddCredential(kMountRoot, kWorkgroup, kUsername, "" /* password */));
  ExpectCredentialsEqual(kMountRoot, kWorkgroup, kUsername, "");
}

TEST_F(InMemoryCredentialStoreTest, AddingMultipleCredentials) {
  const std::string mount_root2 = "smb://192.168.0.1/share";
  const std::string workgroup2 = "workgroup2";
  const std::string username2 = "user2";
  const std::string password2 = "root";

  EXPECT_TRUE(AddCredential(kMountRoot, kWorkgroup, kUsername, kPassword));
  EXPECT_TRUE(AddCredential(mount_root2, workgroup2, username2, password2));

  EXPECT_EQ(2, credential_.CredentialCount());
  EXPECT_TRUE(credential_.HasCredential(kMountRoot));
  EXPECT_TRUE(credential_.HasCredential(mount_root2));
  ExpectCredentialsEqual(kMountRoot, kWorkgroup, kUsername, kPassword);
  ExpectCredentialsEqual(mount_root2, workgroup2, username2, password2);
}

TEST_F(InMemoryCredentialStoreTest, CantAddSameMount) {
  const std::string workgroup2 = "workgroup2";
  const std::string username2 = "user2";
  const std::string password2 = "root2";

  EXPECT_TRUE(AddCredential(kMountRoot, kWorkgroup, kUsername, kPassword));

  // Should return false since the credential is already added for that
  // mount.
  EXPECT_FALSE(AddCredential(kMountRoot, workgroup2, username2, password2));
  EXPECT_EQ(1, credential_.CredentialCount());
  EXPECT_TRUE(credential_.HasCredential(kMountRoot));
  ExpectCredentialsEqual(kMountRoot, kWorkgroup, kUsername, kPassword);
}

TEST_F(InMemoryCredentialStoreTest, CantRemoveWrongCredential) {
  EXPECT_TRUE(AddCredential(kMountRoot, kWorkgroup, kUsername, kPassword));

  // Should be false since the a mount root that doesn't exist is passed.
  EXPECT_FALSE(credential_.RemoveCredential("smb://0.0.0.0"));

  EXPECT_EQ(1, credential_.CredentialCount());
  EXPECT_TRUE(credential_.HasCredential(kMountRoot));
  ExpectCredentialsEqual(kMountRoot, kWorkgroup, kUsername, kPassword);
}

TEST_F(InMemoryCredentialStoreTest, RemoveCredential) {
  EXPECT_TRUE(AddCredential(kMountRoot, kWorkgroup, kUsername, kPassword));
  EXPECT_TRUE(credential_.RemoveCredential(kMountRoot));

  EXPECT_EQ(0, credential_.CredentialCount());
  EXPECT_FALSE(credential_.HasCredential(kMountRoot));
}

TEST_F(InMemoryCredentialStoreTest, RemoveCredentialFromMultiple) {
  const std::string mount_root2 = "smb://192.168.0.1/share";
  const std::string workgroup2 = "workgroup2";
  const std::string username2 = "user2";
  const std::string password2 = "root";

  EXPECT_TRUE(AddCredential(kMountRoot, kWorkgroup, kUsername, kPassword));
  EXPECT_TRUE(AddCredential(mount_root2, workgroup2, username2, password2));
  EXPECT_EQ(2, credential_.CredentialCount());

  EXPECT_TRUE(credential_.RemoveCredential(kMountRoot));

  EXPECT_EQ(1, credential_.CredentialCount());
  EXPECT_FALSE(credential_.HasCredential(kMountRoot));
  EXPECT_TRUE(credential_.HasCredential(mount_root2));
  ExpectCredentialsEqual(mount_root2, workgroup2, username2, password2);

  EXPECT_TRUE(credential_.RemoveCredential(mount_root2));
  EXPECT_EQ(0, credential_.CredentialCount());
}

}  // namespace smbprovider
