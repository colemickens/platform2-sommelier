// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "kerberos/account_manager.h"

#include <map>
#include <memory>
#include <utility>

#include <base/bind.h>
#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/files/scoped_temp_dir.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "kerberos/fake_krb5_interface.h"

namespace {
constexpr char kUser[] = "user@REALM.COM";
}

namespace kerberos {

class AccountManagerTest : public ::testing::Test {
 public:
  AccountManagerTest() = default;
  ~AccountManagerTest() override = default;

  void SetUp() override {
    ::testing::Test::SetUp();

    // Create temp directory for files written during tests.
    CHECK(storage_dir_.CreateUniqueTempDir());

    manager_ = std::make_unique<AccountManager>(
        storage_dir_.GetPath(),
        base::BindRepeating(&AccountManagerTest::OnKerberosFilesChanged,
                            base::Unretained(this)),
        std::make_unique<FakeKrb5Interface>());
  }

 protected:
  void OnKerberosFilesChanged(const std::string& principal_name) {
    kerberos_files_changed_count_[principal_name]++;
  }

  std::unique_ptr<AccountManager> manager_;
  base::ScopedTempDir storage_dir_;
  std::map<std::string, int> kerberos_files_changed_count_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AccountManagerTest);
};

TEST_F(AccountManagerTest, AddAccountSuccess) {
  EXPECT_EQ(ERROR_NONE, manager_->AddAccount(kUser, false /* is_managed */));
  EXPECT_TRUE(base::PathExists(storage_dir_.GetPath().Append("accounts")));
}

// AddAccount fails if the same account is added twice.
TEST_F(AccountManagerTest, AddDuplicateAccountFail) {
  ignore_result(manager_->AddAccount(kUser, false /* is_managed */));
  EXPECT_EQ(ERROR_DUPLICATE_PRINCIPAL_NAME,
            manager_->AddAccount(kUser, false /* is_managed */));
}

// RemoveAccount succeeds if the account exists.
TEST_F(AccountManagerTest, RemoveAccountSuccess) {
  ignore_result(manager_->AddAccount(kUser, false /* is_managed */));
  EXPECT_EQ(ERROR_NONE, manager_->RemoveAccount(kUser));
}

// RemoveAccount fails if the account does not exist.
TEST_F(AccountManagerTest, RemoveUnknownAccountFail) {
  EXPECT_EQ(ERROR_UNKNOWN_PRINCIPAL_NAME, manager_->RemoveAccount(kUser));
}

// Repeatedly calling AddAccount() and RemoveAccount() succeeds.
TEST_F(AccountManagerTest, RepeatedAddRemoveSuccess) {
  ignore_result(manager_->AddAccount(kUser, false /* is_managed */));
  ignore_result(manager_->RemoveAccount(kUser));
  EXPECT_EQ(ERROR_NONE, manager_->AddAccount(kUser, false /* is_managed */));
  EXPECT_EQ(ERROR_NONE, manager_->RemoveAccount(kUser));
}

// TODO(https://crbug.com/952248): Add more tests.
// - SerializeAccounts and DeserializeAccounts()
// - RemoveAccount deletes files on disk
// - RemoveAccount triggers KerberosFilesChanged IF FILES EXISTS (todo)
// - ListAccounts (stub out GetTgtStatus)
// - SetConfig saves file and triggers KerberosFilesChanged
// - AcquireTgt (stub out AcquireTgt) triggers KerberosFilesChanged if there
// was no error.
// - GetKerberosFiles returns ERROR_NONE with no info if krb5cc doesn't exist,
// otherwise returns files.

}  // namespace kerberos
