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

  bool AddCredentials(const std::string& mount_root,
                      const std::string& workgroup,
                      const std::string& username,
                      const std::string& password) {
    return credentials_.AddCredentials(
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

    EXPECT_TRUE(credentials_.GetAuthentication(
        mount_root, workgroup_buffer, kBufferSize, username_buffer, kBufferSize,
        password_buffer, kBufferSize));

    EXPECT_EQ(std::string(workgroup_buffer), workgroup);
    EXPECT_EQ(std::string(username_buffer), username);
    EXPECT_EQ(std::string(password_buffer), password);
  }

 protected:
  TempFileManager temp_files_;
  InMemoryCredentialStore credentials_;

 private:
  DISALLOW_COPY_AND_ASSIGN(InMemoryCredentialStoreTest);
};

TEST_F(InMemoryCredentialStoreTest, NoCredentials) {
  // Verify the state of an empty |InMemoryCredentialStore|
  EXPECT_EQ(0, credentials_.CredentialsCount());
  EXPECT_FALSE(credentials_.RemoveCredentials(kMountRoot));
  EXPECT_EQ(0, credentials_.CredentialsCount());
  EXPECT_FALSE(credentials_.HasCredentials(kMountRoot));
}

TEST_F(InMemoryCredentialStoreTest, AddingCredentials) {
  EXPECT_TRUE(AddCredentials(kMountRoot, kWorkgroup, kUsername, kPassword));
  EXPECT_EQ(1, credentials_.CredentialsCount());
  EXPECT_TRUE(credentials_.HasCredentials(kMountRoot));
  ExpectCredentialsEqual(kMountRoot, kWorkgroup, kUsername, kPassword);
}

TEST_F(InMemoryCredentialStoreTest, BufferNullTerminatedWhenLengthTooSmall) {
  EXPECT_TRUE(AddCredentials(kMountRoot, kWorkgroup, kUsername, kPassword));
  EXPECT_EQ(1, credentials_.CredentialsCount());
  EXPECT_TRUE(credentials_.HasCredentials(kMountRoot));

  // Initialize buffers with 1.
  char workgroup_buffer[kBufferSize] = {1};
  char username_buffer[kBufferSize] = {1};
  char password_buffer[kBufferSize] = {1};

  // Call the authentication function while passing 1 as the buffer sizes. This
  // should return false since the buffer sizes are too small.
  EXPECT_FALSE(credentials_.GetAuthentication(
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

  // This should return false when no credentials are found.
  EXPECT_FALSE(credentials_.GetAuthentication(
      kMountRoot, workgroup_buffer, kBufferSize, username_buffer, kBufferSize,
      password_buffer, kBufferSize));

  // Buffers should be null-terminated.
  EXPECT_EQ('\0', workgroup_buffer[0]);
  EXPECT_EQ('\0', password_buffer[0]);
  EXPECT_EQ('\0', password_buffer[0]);
}

TEST_F(InMemoryCredentialStoreTest, AddingEmptyCredentials) {
  EXPECT_TRUE(credentials_.AddEmptyCredentials(kMountRoot));
  EXPECT_EQ(1, credentials_.CredentialsCount());
  EXPECT_TRUE(credentials_.HasCredentials(kMountRoot));
  ExpectCredentialsEqual(kMountRoot, "" /* workgroup */, "" /* username */,
                         "" /* password */);
}

TEST_F(InMemoryCredentialStoreTest, CredentialsWithoutWorkgroup) {
  EXPECT_TRUE(
      AddCredentials(kMountRoot, "" /* workgroup */, kUsername, kPassword));
  ExpectCredentialsEqual(kMountRoot, "" /* workgroup */, kUsername, kPassword);
}

TEST_F(InMemoryCredentialStoreTest, CredentialsWithoutPassword) {
  EXPECT_TRUE(
      AddCredentials(kMountRoot, kWorkgroup, kUsername, "" /* password */));
  ExpectCredentialsEqual(kMountRoot, kWorkgroup, kUsername, "");
}

TEST_F(InMemoryCredentialStoreTest, AddingMultipleCredentials) {
  const std::string mount_root2 = "smb://192.168.0.1/share";
  const std::string workgroup2 = "workgroup2";
  const std::string username2 = "user2";
  const std::string password2 = "root";

  EXPECT_TRUE(AddCredentials(kMountRoot, kWorkgroup, kUsername, kPassword));
  EXPECT_TRUE(AddCredentials(mount_root2, workgroup2, username2, password2));

  EXPECT_EQ(2, credentials_.CredentialsCount());
  EXPECT_TRUE(credentials_.HasCredentials(kMountRoot));
  EXPECT_TRUE(credentials_.HasCredentials(mount_root2));
  ExpectCredentialsEqual(kMountRoot, kWorkgroup, kUsername, kPassword);
  ExpectCredentialsEqual(mount_root2, workgroup2, username2, password2);
}

TEST_F(InMemoryCredentialStoreTest, CantAddSameMount) {
  const std::string workgroup2 = "workgroup2";
  const std::string username2 = "user2";
  const std::string password2 = "root2";

  EXPECT_TRUE(AddCredentials(kMountRoot, kWorkgroup, kUsername, kPassword));

  // Should return false since the credentials were already added for that
  // mount.
  EXPECT_FALSE(AddCredentials(kMountRoot, workgroup2, username2, password2));
  EXPECT_EQ(1, credentials_.CredentialsCount());
  EXPECT_TRUE(credentials_.HasCredentials(kMountRoot));
  ExpectCredentialsEqual(kMountRoot, kWorkgroup, kUsername, kPassword);
}

TEST_F(InMemoryCredentialStoreTest, CantRemoveWrongCredentials) {
  EXPECT_TRUE(AddCredentials(kMountRoot, kWorkgroup, kUsername, kPassword));

  // Should be false since the a mount root that doesn't exist is passed.
  EXPECT_FALSE(credentials_.RemoveCredentials("smb://0.0.0.0"));

  EXPECT_EQ(1, credentials_.CredentialsCount());
  EXPECT_TRUE(credentials_.HasCredentials(kMountRoot));
  ExpectCredentialsEqual(kMountRoot, kWorkgroup, kUsername, kPassword);
}

TEST_F(InMemoryCredentialStoreTest, RemoveCredentials) {
  EXPECT_TRUE(AddCredentials(kMountRoot, kWorkgroup, kUsername, kPassword));
  EXPECT_TRUE(credentials_.RemoveCredentials(kMountRoot));

  EXPECT_EQ(0, credentials_.CredentialsCount());
  EXPECT_FALSE(credentials_.HasCredentials(kMountRoot));
}

TEST_F(InMemoryCredentialStoreTest, RemoveCredentialsFromMultiple) {
  const std::string mount_root2 = "smb://192.168.0.1/share";
  const std::string workgroup2 = "workgroup2";
  const std::string username2 = "user2";
  const std::string password2 = "root";

  EXPECT_TRUE(AddCredentials(kMountRoot, kWorkgroup, kUsername, kPassword));
  EXPECT_TRUE(AddCredentials(mount_root2, workgroup2, username2, password2));
  EXPECT_EQ(2, credentials_.CredentialsCount());

  EXPECT_TRUE(credentials_.RemoveCredentials(kMountRoot));

  EXPECT_EQ(1, credentials_.CredentialsCount());
  EXPECT_FALSE(credentials_.HasCredentials(kMountRoot));
  EXPECT_TRUE(credentials_.HasCredentials(mount_root2));
  ExpectCredentialsEqual(mount_root2, workgroup2, username2, password2);

  EXPECT_TRUE(credentials_.RemoveCredentials(mount_root2));
  EXPECT_EQ(0, credentials_.CredentialsCount());
}

}  // namespace smbprovider
