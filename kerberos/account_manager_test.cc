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
#include <libpasswordprovider/fake_password_provider.h>
#include <libpasswordprovider/password_provider_test_utils.h>

#include "kerberos/fake_krb5_interface.h"

namespace {

constexpr char kUser[] = "user@REALM.COM";
constexpr char kUser2[] = "user2@REALM2.COM";
constexpr char kPassword[] = "i<3k3R8e5Oz";
constexpr char kPassword2[] = "ih4zf00d";
constexpr char kKrb5Conf[] = R"(
  [libdefaults]
    default_realm = REALM.COM)";

// Convenience defines to make code more readable
constexpr bool kManaged = true;
constexpr bool kUnmanaged = false;

constexpr bool kRememberPassword = true;
constexpr bool kDontRememberPassword = false;

constexpr bool kUseLoginPassword = true;
constexpr bool kDontUseLoginPassword = false;

constexpr char kEmptyPassword[] = "";

}  // namespace

namespace kerberos {

class AccountManagerTest : public ::testing::Test {
 public:
  AccountManagerTest()
      : kerberos_files_changed_(
            base::BindRepeating(&AccountManagerTest::OnKerberosFilesChanged,
                                base::Unretained(this))),
        kerberos_ticket_expiring_(
            base::BindRepeating(&AccountManagerTest::OnKerberosTicketExpiring,
                                base::Unretained(this))) {}
  ~AccountManagerTest() override = default;

  void SetUp() override {
    ::testing::Test::SetUp();

    // Create temp directory for files written during tests.
    CHECK(storage_dir_.CreateUniqueTempDir());
    accounts_path_ = storage_dir_.GetPath().Append("accounts");
    const base::FilePath account_dir = storage_dir_.GetPath().Append(
        AccountManager::GetSafeFilenameForTesting(kUser));
    krb5cc_path_ = account_dir.Append("krb5cc");
    krb5conf_path_ = account_dir.Append("krb5.conf");
    password_path_ = account_dir.Append("password");

    // Create the manager with a fake krb5 interface.
    auto krb5 = std::make_unique<FakeKrb5Interface>();
    auto password_provider =
        std::make_unique<password_provider::FakePasswordProvider>();
    krb5_ = krb5.get();
    password_provider_ = password_provider.get();
    manager_ = std::make_unique<AccountManager>(
        storage_dir_.GetPath(), kerberos_files_changed_,
        kerberos_ticket_expiring_, std::move(krb5),
        std::move(password_provider));
  }

  void TearDown() override {
    // Make sure the file stored on disk contains the same accounts as the
    // manager instance. This catches cases where AccountManager forgets to save
    // accounts on some change.
    if (base::PathExists(accounts_path_)) {
      std::vector<Account> accounts;
      EXPECT_EQ(ERROR_NONE, manager_->ListAccounts(&accounts));

      AccountManager other_manager(
          storage_dir_.GetPath(), kerberos_files_changed_,
          kerberos_ticket_expiring_, std::make_unique<FakeKrb5Interface>(),
          std::make_unique<password_provider::FakePasswordProvider>());
      other_manager.LoadAccounts();
      std::vector<Account> other_accounts;
      EXPECT_EQ(ERROR_NONE, other_manager.ListAccounts(&other_accounts));

      ASSERT_NO_FATAL_FAILURE(ExpectAccountsEqual(accounts, other_accounts));
    }

    ::testing::Test::TearDown();
  }

  // Add account with default settings.
  ErrorType AddAccount() { return manager_->AddAccount(kUser, kUnmanaged); }

  // Sets some default Kerberos configuration.
  ErrorType SetConfig() { return manager_->SetConfig(kUser, kKrb5Conf); }

  // Acquire Kerberos ticket with default credentials and settings.
  ErrorType AcquireTgt() {
    return manager_->AcquireTgt(kUser, kPassword, kDontRememberPassword,
                                kDontUseLoginPassword);
  }

  void SaveLoginPassword(const char* password) {
    auto password_ptr = password_provider::test::CreatePassword(password);
    password_provider_->SavePassword(*password_ptr);
  }

 protected:
  void OnKerberosFilesChanged(const std::string& principal_name) {
    kerberos_files_changed_count_[principal_name]++;
  }

  void OnKerberosTicketExpiring(const std::string& principal_name) {
    kerberos_ticket_expiring_count_[principal_name]++;
  }

  void ExpectAccountsEqual(const std::vector<Account>& account_list_1,
                           const std::vector<Account>& account_list_2) {
    ASSERT_EQ(account_list_1.size(), account_list_2.size());
    for (size_t n = 0; n < account_list_1.size(); ++n) {
      const Account& account1 = account_list_1[n];
      const Account& account2 = account_list_2[n];

      EXPECT_EQ(account1.principal_name(), account2.principal_name());
      EXPECT_EQ(account1.is_managed(), account2.is_managed());
      EXPECT_EQ(account1.use_login_password(), account2.use_login_password());
      // TODO(https://crbug.com/952239): Check additional properties.
    }
  }

  std::unique_ptr<AccountManager> manager_;

  // Fake Kerberos interface used by |manager_|. Not owned.
  FakeKrb5Interface* krb5_;

  // Fake password provider to get the login password. Not owned.
  password_provider::FakePasswordProvider* password_provider_;

  // Paths of files stored by |manager_|.
  base::ScopedTempDir storage_dir_;
  base::FilePath accounts_path_;
  base::FilePath krb5conf_path_;
  base::FilePath krb5cc_path_;
  base::FilePath password_path_;

  AccountManager::KerberosFilesChangedCallback kerberos_files_changed_;
  AccountManager::KerberosTicketExpiringCallback kerberos_ticket_expiring_;

  std::map<std::string, int> kerberos_files_changed_count_;
  std::map<std::string, int> kerberos_ticket_expiring_count_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AccountManagerTest);
};

// Adding an account succeeds and serializes the file on disk.
TEST_F(AccountManagerTest, AddAccountSuccess) {
  EXPECT_FALSE(base::PathExists(accounts_path_));
  EXPECT_EQ(ERROR_NONE, AddAccount());
  EXPECT_TRUE(base::PathExists(accounts_path_));
}

// AddAccount() fails if the same account is added twice.
TEST_F(AccountManagerTest, AddDuplicateAccountFail) {
  ignore_result(AddAccount());

  EXPECT_TRUE(base::DeleteFile(accounts_path_, false /* recursive */));
  EXPECT_EQ(ERROR_DUPLICATE_PRINCIPAL_NAME, AddAccount());
  EXPECT_FALSE(base::PathExists(accounts_path_));
}

// Adding a managed account overwrites an existing unmanaged account.
TEST_F(AccountManagerTest, ManagedOverridesUnmanaged) {
  ignore_result(manager_->AddAccount(kUser, kUnmanaged));

  EXPECT_EQ(ERROR_NONE, AcquireTgt());
  EXPECT_TRUE(base::PathExists(krb5cc_path_));

  // Overwriting with a managed account should wipe existing files and make the
  // account managed.
  EXPECT_EQ(ERROR_DUPLICATE_PRINCIPAL_NAME,
            manager_->AddAccount(kUser, kManaged));
  EXPECT_FALSE(base::PathExists(krb5cc_path_));

  std::vector<Account> accounts;
  EXPECT_EQ(ERROR_NONE, manager_->ListAccounts(&accounts));
  ASSERT_EQ(1u, accounts.size());
  EXPECT_TRUE(accounts[0].is_managed());
}

// Adding an unmanaged account does not overwrite an existing managed account.
TEST_F(AccountManagerTest, UnmanagedDoesNotOverrideManaged) {
  ignore_result(manager_->AddAccount(kUser, kManaged));

  EXPECT_EQ(ERROR_DUPLICATE_PRINCIPAL_NAME,
            manager_->AddAccount(kUser, kUnmanaged));
  std::vector<Account> accounts;
  EXPECT_EQ(ERROR_NONE, manager_->ListAccounts(&accounts));
  ASSERT_EQ(1u, accounts.size());
  EXPECT_TRUE(accounts[0].is_managed());
}

// RemoveAccount() succeeds if the account exists and serializes the file on
// disk.
TEST_F(AccountManagerTest, RemoveAccountSuccess) {
  ignore_result(AddAccount());

  EXPECT_TRUE(base::DeleteFile(accounts_path_, false /* recursive */));
  EXPECT_EQ(ERROR_NONE, manager_->RemoveAccount(kUser));
  EXPECT_TRUE(base::PathExists(accounts_path_));
}

// RemoveAccount() fails if the account does not exist.
TEST_F(AccountManagerTest, RemoveUnknownAccountFail) {
  EXPECT_EQ(ERROR_UNKNOWN_PRINCIPAL_NAME, manager_->RemoveAccount(kUser));
  EXPECT_FALSE(base::PathExists(accounts_path_));
}

// RemoveAccount() does not trigger KerberosFilesChanged if the credential cache
// does not exists.
TEST_F(AccountManagerTest, RemoveAccountTriggersKFCIfCCExists) {
  ignore_result(AddAccount());

  EXPECT_EQ(ERROR_NONE, manager_->RemoveAccount(kUser));
  EXPECT_EQ(0, kerberos_files_changed_count_[kUser]);
}

// RemoveAccount() triggers KerberosFilesChanged if the credential cache exists.
TEST_F(AccountManagerTest, RemoveAccountDoesNotTriggerKFCIfCCDoesNotExist) {
  ignore_result(AddAccount());

  EXPECT_EQ(ERROR_NONE, AcquireTgt());
  EXPECT_EQ(1, kerberos_files_changed_count_[kUser]);
  EXPECT_EQ(ERROR_NONE, manager_->RemoveAccount(kUser));
  EXPECT_EQ(2, kerberos_files_changed_count_[kUser]);
}

// Repeatedly calling AddAccount() and RemoveAccount() succeeds.
TEST_F(AccountManagerTest, RepeatedAddRemoveSuccess) {
  ignore_result(AddAccount());
  ignore_result(manager_->RemoveAccount(kUser));

  EXPECT_EQ(ERROR_NONE, AddAccount());
  EXPECT_EQ(ERROR_NONE, manager_->RemoveAccount(kUser));
}

// ClearAccounts(CLEAR_ALL) clears all accounts.
TEST_F(AccountManagerTest, ClearAccountsSuccess) {
  ignore_result(manager_->AddAccount(kUser, kUnmanaged));
  ignore_result(manager_->AddAccount(kUser2, kManaged));

  EXPECT_EQ(ERROR_NONE, manager_->ClearAccounts(CLEAR_ALL));
  std::vector<Account> accounts;
  EXPECT_EQ(ERROR_NONE, manager_->ListAccounts(&accounts));
  EXPECT_EQ(0u, accounts.size());
}

// ClearAccounts(CLEAR_ALL) wipes Kerberos configuration and credential cache.
TEST_F(AccountManagerTest, ClearAccountsRemovesKerberosFiles) {
  ignore_result(AddAccount());

  EXPECT_EQ(ERROR_NONE, SetConfig());
  EXPECT_EQ(ERROR_NONE, AcquireTgt());
  EXPECT_TRUE(base::PathExists(krb5conf_path_));
  EXPECT_TRUE(base::PathExists(krb5cc_path_));
  EXPECT_EQ(ERROR_NONE, manager_->ClearAccounts(CLEAR_ALL));
  EXPECT_FALSE(base::PathExists(krb5conf_path_));
  EXPECT_FALSE(base::PathExists(krb5cc_path_));
}

// ClearAccounts(CLEAR_ALL) triggers KerberosFilesChanged if the credential
// cache exists.
TEST_F(AccountManagerTest, ClearAccountsTriggersKFCIfCCExists) {
  ignore_result(AddAccount());

  EXPECT_EQ(ERROR_NONE, AcquireTgt());
  EXPECT_EQ(1, kerberos_files_changed_count_[kUser]);
  EXPECT_EQ(ERROR_NONE, manager_->ClearAccounts(CLEAR_ALL));
  EXPECT_EQ(2, kerberos_files_changed_count_[kUser]);
}

// ClearAccounts(CLEAR_ALL) does not trigger KerberosFilesChanged if the
// credential cache does not exist.
TEST_F(AccountManagerTest, ClearAccountsDoesNotTriggerKFCIfDoesNotCCExist) {
  ignore_result(AddAccount());

  EXPECT_EQ(ERROR_NONE, manager_->ClearAccounts(CLEAR_ALL));
  EXPECT_EQ(0, kerberos_files_changed_count_[kUser]);
}

// ClearAccounts(CLEAR_ONLY_UNMANAGED_ACCOUNTS) clears only unmanaged accounts.
TEST_F(AccountManagerTest, ClearUnmanagedAccountsSuccess) {
  ignore_result(manager_->AddAccount(kUser, kUnmanaged));
  ignore_result(manager_->AddAccount(kUser2, kManaged));

  EXPECT_EQ(ERROR_NONE, manager_->ClearAccounts(CLEAR_ONLY_UNMANAGED_ACCOUNTS));
  std::vector<Account> accounts;
  EXPECT_EQ(ERROR_NONE, manager_->ListAccounts(&accounts));
  ASSERT_EQ(1u, accounts.size());
  EXPECT_EQ(kUser2, accounts[0].principal_name());
}

// ClearAccounts(CLEAR_ONLY_UNMANAGED_REMEMBERED_PASSWORDS) clears only
// passwords of unmanaged accounts.
TEST_F(AccountManagerTest, ClearUnmanagedPasswordsSuccess) {
  // kUser is unmanaged, kUser2 is managed.
  ignore_result(manager_->AddAccount(kUser, kUnmanaged));
  ignore_result(manager_->AddAccount(kUser2, kManaged));
  ignore_result(manager_->AcquireTgt(kUser, kPassword, kRememberPassword,
                                     kDontUseLoginPassword));
  ignore_result(manager_->AcquireTgt(kUser2, kPassword, kRememberPassword,
                                     kDontUseLoginPassword));

  base::FilePath password_path_2 =
      storage_dir_.GetPath()
          .Append(AccountManager::GetSafeFilenameForTesting(kUser2))
          .Append("password");
  EXPECT_TRUE(base::PathExists(password_path_));
  EXPECT_TRUE(base::PathExists(password_path_2));

  EXPECT_EQ(ERROR_NONE,
            manager_->ClearAccounts(CLEAR_ONLY_UNMANAGED_REMEMBERED_PASSWORDS));
  std::vector<Account> accounts;
  EXPECT_EQ(ERROR_NONE, manager_->ListAccounts(&accounts));
  ASSERT_EQ(2u, accounts.size());
  EXPECT_FALSE(base::PathExists(password_path_));
  EXPECT_TRUE(base::PathExists(password_path_2));
}

// SetConfig() succeeds and writes the config to |krb5conf_path_|.
TEST_F(AccountManagerTest, SetConfigSuccess) {
  ignore_result(AddAccount());

  EXPECT_EQ(ERROR_NONE, SetConfig());
  std::string krb5_conf;
  EXPECT_TRUE(base::ReadFileToString(krb5conf_path_, &krb5_conf));
  EXPECT_EQ(krb5_conf, kKrb5Conf);
}

// SetConfig() triggers KerberosFilesChanged if the credential cache exists.
TEST_F(AccountManagerTest, SetConfigTriggersKFCIfCCExists) {
  ignore_result(AddAccount());

  EXPECT_EQ(ERROR_NONE, AcquireTgt());
  EXPECT_EQ(1, kerberos_files_changed_count_[kUser]);
  EXPECT_EQ(ERROR_NONE, SetConfig());
  EXPECT_EQ(2, kerberos_files_changed_count_[kUser]);
}

// SetConfig() does not trigger KerberosFilesChanged if the credential cache
// does not exist.
TEST_F(AccountManagerTest, SetConfigDoesNotTriggerKFCIfDoesNotCCExist) {
  ignore_result(AddAccount());

  EXPECT_EQ(ERROR_NONE, SetConfig());
  EXPECT_EQ(0, kerberos_files_changed_count_[kUser]);
}

// RemoveAccount() removes the config file.
TEST_F(AccountManagerTest, RemoveAccountRemovesConfig) {
  ignore_result(AddAccount());
  ignore_result(SetConfig());

  EXPECT_TRUE(base::PathExists(krb5conf_path_));
  EXPECT_EQ(ERROR_NONE, manager_->RemoveAccount(kUser));
  EXPECT_FALSE(base::PathExists(krb5conf_path_));
}

// AcquireTgt() succeeds and writes a credential cache file.
TEST_F(AccountManagerTest, AcquireTgtSuccess) {
  ignore_result(AddAccount());

  EXPECT_EQ(ERROR_NONE, AcquireTgt());
  EXPECT_TRUE(base::PathExists(krb5cc_path_));
}

// AcquireTgt() triggers KerberosFilesChanged on success.
TEST_F(AccountManagerTest, AcquireTgtTriggersKFCOnSuccess) {
  ignore_result(AddAccount());

  EXPECT_EQ(0, kerberos_files_changed_count_[kUser]);
  EXPECT_EQ(ERROR_NONE, AcquireTgt());
  EXPECT_EQ(1, kerberos_files_changed_count_[kUser]);
}

// AcquireTgt() does not trigger KerberosFilesChanged on failure.
TEST_F(AccountManagerTest, AcquireTgtDoesNotTriggerKFCOnFailure) {
  ignore_result(AddAccount());

  krb5_->set_acquire_tgt_error(ERROR_UNKNOWN);
  EXPECT_EQ(ERROR_UNKNOWN, AcquireTgt());
  EXPECT_EQ(0, kerberos_files_changed_count_[kUser]);
}

// AcquireTgt() saves password to disk if |remember_password| is true and
// removes the file again if |remember_password| is false.
TEST_F(AccountManagerTest, AcquireTgtRemembersPasswordsIfWanted) {
  ignore_result(AddAccount());

  EXPECT_FALSE(base::PathExists(password_path_));
  EXPECT_EQ(ERROR_NONE,
            manager_->AcquireTgt(kUser, kPassword, kRememberPassword,
                                 kDontUseLoginPassword));
  EXPECT_TRUE(base::PathExists(password_path_));

  EXPECT_EQ(ERROR_NONE,
            manager_->AcquireTgt(kUser, kPassword, kDontRememberPassword,
                                 kDontUseLoginPassword));
  EXPECT_FALSE(base::PathExists(password_path_));
}

// AcquireTgt() uses saved password if none is given, no matter if it should be
// remembered again or not.
TEST_F(AccountManagerTest, AcquireTgtLoadsRememberedPassword) {
  ignore_result(AddAccount());
  ignore_result(manager_->AcquireTgt(kUser, kPassword, kRememberPassword,
                                     kDontUseLoginPassword));

  // This should load stored password and keep it.
  EXPECT_EQ(ERROR_NONE,
            manager_->AcquireTgt(kUser, kEmptyPassword, kRememberPassword,
                                 kDontUseLoginPassword));
  EXPECT_TRUE(base::PathExists(password_path_));

  // This should load stored password, but erase it afterwards.
  EXPECT_EQ(ERROR_NONE,
            manager_->AcquireTgt(kUser, kEmptyPassword, kDontRememberPassword,
                                 kDontUseLoginPassword));
  EXPECT_FALSE(base::PathExists(password_path_));

  // Check that the fake krb5 interface returns an error for a missing password.
  // This verifies that the above AcquireTgt() call actually loaded the
  // password from disk.
  EXPECT_EQ(ERROR_BAD_PASSWORD,
            manager_->AcquireTgt(kUser, kEmptyPassword, kDontRememberPassword,
                                 kDontUseLoginPassword));
}

// AcquireTgt() uses the login password if saved.
TEST_F(AccountManagerTest, AcquireTgtUsesLoginPassword) {
  ignore_result(AddAccount());

  // Shouldn't explode if the login password not set yet.
  EXPECT_EQ(ERROR_BAD_PASSWORD,
            manager_->AcquireTgt(kUser, kEmptyPassword, kDontRememberPassword,
                                 kUseLoginPassword));

  SaveLoginPassword(kPassword);
  krb5_->set_expected_password(kPassword);

  // Uses the login password.
  EXPECT_EQ(ERROR_NONE,
            manager_->AcquireTgt(kUser, kEmptyPassword, kDontRememberPassword,
                                 kUseLoginPassword));

  // Check if auth fails without kUseLoginPassword.
  EXPECT_EQ(ERROR_BAD_PASSWORD,
            manager_->AcquireTgt(kUser, kEmptyPassword, kDontRememberPassword,
                                 kDontUseLoginPassword));
}

// AcquireTgt() wipes a saved password if the login password is used.
TEST_F(AccountManagerTest, AcquireTgtWipesStoredPasswordOnUsesLoginPassword) {
  ignore_result(AddAccount());
  ignore_result(manager_->AcquireTgt(kUser, kPassword, kRememberPassword,
                                     kDontUseLoginPassword));
  EXPECT_TRUE(base::PathExists(password_path_));

  SaveLoginPassword(kPassword);

  // Note: kRememberPassword gets ignored if kUseLoginPassword is passed.
  EXPECT_EQ(ERROR_NONE,
            manager_->AcquireTgt(kUser, kEmptyPassword, kRememberPassword,
                                 kUseLoginPassword));
  EXPECT_FALSE(base::PathExists(password_path_));
}

// AcquireTgt() ignores the passed password if the login password is used.
TEST_F(AccountManagerTest, AcquireTgtIgnoresPassedPasswordOnUsesLoginPassword) {
  ignore_result(AddAccount());

  SaveLoginPassword(kPassword);
  krb5_->set_expected_password(kPassword);

  // Auth works despite passed kPassword2 != expected kPassword because the
  // login kPassword is used.
  EXPECT_EQ(ERROR_NONE,
            manager_->AcquireTgt(kUser, kPassword2, kDontRememberPassword,
                                 kUseLoginPassword));
}

// RemoveAccount() removes the credential cache file.
TEST_F(AccountManagerTest, RemoveAccountRemovesCC) {
  ignore_result(AddAccount());
  ignore_result(AcquireTgt());

  EXPECT_TRUE(base::PathExists(krb5cc_path_));
  EXPECT_EQ(ERROR_NONE, manager_->RemoveAccount(kUser));
  EXPECT_FALSE(base::PathExists(krb5cc_path_));
}

// RemoveAccount() removes saved passwords.
TEST_F(AccountManagerTest, RemoveAccountRemovesPassword) {
  ignore_result(AddAccount());
  ignore_result(manager_->AcquireTgt(kUser, kPassword, kRememberPassword,
                                     kDontUseLoginPassword));

  EXPECT_TRUE(base::PathExists(password_path_));
  EXPECT_EQ(ERROR_NONE, manager_->RemoveAccount(kUser));
  EXPECT_FALSE(base::PathExists(password_path_));
}

// ListAccounts() succeeds and contains the expected data.
TEST_F(AccountManagerTest, ListAccountsSuccess) {
  ignore_result(manager_->AddAccount(kUser, kManaged));
  ignore_result(SetConfig());
  ignore_result(manager_->AcquireTgt(kUser, kPassword, kRememberPassword,
                                     kDontUseLoginPassword));
  SaveLoginPassword(kPassword);
  ignore_result(manager_->AddAccount(kUser2, kUnmanaged));
  // Note: kRememberPassword should be ignored here, see below.
  ignore_result(manager_->AcquireTgt(kUser2, kPassword, kRememberPassword,
                                     kUseLoginPassword));
  EXPECT_TRUE(base::PathExists(krb5cc_path_));

  // Set a fake tgt status.
  constexpr int kRenewalSeconds = 10;
  constexpr int kValiditySeconds = 90;
  Krb5Interface::TgtStatus status;
  status.renewal_seconds = kRenewalSeconds;
  status.validity_seconds = kValiditySeconds;
  krb5_->set_tgt_status(std::move(status));

  // Verify that ListAccounts returns the expected account.
  std::vector<Account> accounts;
  EXPECT_EQ(ERROR_NONE, manager_->ListAccounts(&accounts));
  ASSERT_EQ(2u, accounts.size());

  EXPECT_EQ(kUser, accounts[0].principal_name());
  EXPECT_EQ(kKrb5Conf, accounts[0].krb5conf());
  EXPECT_EQ(kRenewalSeconds, accounts[0].tgt_renewal_seconds());
  EXPECT_EQ(kValiditySeconds, accounts[0].tgt_validity_seconds());
  EXPECT_TRUE(accounts[0].is_managed());
  EXPECT_TRUE(accounts[0].password_was_remembered());

  EXPECT_EQ(kUser2, accounts[1].principal_name());
  EXPECT_FALSE(accounts[1].password_was_remembered());
  EXPECT_TRUE(accounts[1].use_login_password());
}

// ListAccounts() ignores failures in GetTgtStatus() and loading the config.
TEST_F(AccountManagerTest, ListAccountsIgnoresFailures) {
  ignore_result(AddAccount());
  ignore_result(SetConfig());
  ignore_result(AcquireTgt());
  EXPECT_TRUE(base::PathExists(krb5cc_path_));

  // Make reading the config fail.
  EXPECT_TRUE(base::SetPosixFilePermissions(krb5conf_path_, 0));

  // Make GetTgtStatus() fail.
  krb5_->set_get_tgt_status_error(ERROR_UNKNOWN);

  // ListAccounts() should still work, despite the errors.
  std::vector<Account> accounts;
  EXPECT_EQ(ERROR_NONE, manager_->ListAccounts(&accounts));
  ASSERT_EQ(1u, accounts.size());
  EXPECT_EQ(kUser, accounts[0].principal_name());

  // The config should not be set since we made reading the file fail.
  EXPECT_FALSE(accounts[0].has_krb5conf());

  // tgt_*_seconds should not be set since we made GetTgtStatus() fail.
  EXPECT_FALSE(accounts[0].has_tgt_renewal_seconds());
  EXPECT_FALSE(accounts[0].has_tgt_validity_seconds());
}

// GetKerberosFiles returns empty KerberosFiles if there is no credential cache,
// even if there is a config.
TEST_F(AccountManagerTest, GetKerberosFilesSucceesWithoutCC) {
  ignore_result(AddAccount());
  ignore_result(SetConfig());

  KerberosFiles files;
  EXPECT_EQ(ERROR_NONE, manager_->GetKerberosFiles(kUser, &files));
  EXPECT_FALSE(files.has_krb5cc());
  EXPECT_FALSE(files.has_krb5conf());
}

// GetKerberosFiles returns the expected KerberosFiles if there is a credential
// cache.
TEST_F(AccountManagerTest, GetKerberosFilesSucceesWithCC) {
  ignore_result(AddAccount());
  ignore_result(SetConfig());
  ignore_result(AcquireTgt());

  KerberosFiles files;
  EXPECT_EQ(ERROR_NONE, manager_->GetKerberosFiles(kUser, &files));
  EXPECT_FALSE(files.krb5cc().empty());
  EXPECT_EQ(kKrb5Conf, files.krb5conf());
}

// Most methods return ERROR_UNKNOWN_PRINCIPAL if called with such a principal.
TEST_F(AccountManagerTest, MethodsReturnUnknownPrincipal) {
  KerberosFiles files;
  EXPECT_EQ(ERROR_UNKNOWN_PRINCIPAL_NAME, manager_->RemoveAccount(kUser));
  EXPECT_EQ(ERROR_UNKNOWN_PRINCIPAL_NAME, SetConfig());
  EXPECT_EQ(ERROR_UNKNOWN_PRINCIPAL_NAME, AcquireTgt());
  EXPECT_EQ(ERROR_UNKNOWN_PRINCIPAL_NAME,
            manager_->GetKerberosFiles(kUser, &files));
}

// Accounts can be saved to disk and loaded from disk.
TEST_F(AccountManagerTest, SerializationSuccess) {
  ignore_result(manager_->AddAccount(kUser, kManaged));
  ignore_result(manager_->AcquireTgt(kUser, kPassword, kDontRememberPassword,
                                     kUseLoginPassword));

  ignore_result(manager_->AddAccount(kUser2, kUnmanaged));
  ignore_result(manager_->AcquireTgt(kUser2, kPassword, kDontRememberPassword,
                                     kDontUseLoginPassword));

  EXPECT_EQ(ERROR_NONE, manager_->SaveAccounts());
  AccountManager other_manager(
      storage_dir_.GetPath(), kerberos_files_changed_,
      kerberos_ticket_expiring_, std::make_unique<FakeKrb5Interface>(),
      std::make_unique<password_provider::FakePasswordProvider>());
  other_manager.LoadAccounts();
  std::vector<Account> accounts;
  EXPECT_EQ(ERROR_NONE, other_manager.ListAccounts(&accounts));
  ASSERT_EQ(2u, accounts.size());

  EXPECT_EQ(kUser, accounts[0].principal_name());
  EXPECT_EQ(kUser2, accounts[1].principal_name());

  EXPECT_TRUE(accounts[0].is_managed());
  EXPECT_FALSE(accounts[1].is_managed());

  EXPECT_TRUE(accounts[0].use_login_password());
  EXPECT_FALSE(accounts[1].use_login_password());

  // TODO(https://crbug.com/952239): Check additional Account properties.
}

// The TriggerKerberosTicketExpiringForExpiredTickets() method works fine.
TEST_F(AccountManagerTest, TriggerKerberosTicketExpiringForExpiredTickets) {
  ignore_result(AddAccount());
  ignore_result(SetConfig());
  ignore_result(AcquireTgt());
  EXPECT_EQ(0, kerberos_ticket_expiring_count_[kUser]);

  // Fake an expired ticket.
  Krb5Interface::TgtStatus status;
  status.validity_seconds = 0;
  krb5_->set_tgt_status(status);
  manager_->TriggerKerberosTicketExpiringForExpiredTickets();
  EXPECT_EQ(1, kerberos_ticket_expiring_count_[kUser]);

  // Fake a valid ticket.
  status.validity_seconds = 1;
  krb5_->set_tgt_status(status);
  manager_->TriggerKerberosTicketExpiringForExpiredTickets();
  EXPECT_EQ(1, kerberos_ticket_expiring_count_[kUser]);
}

}  // namespace kerberos
