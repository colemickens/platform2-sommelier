// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "kerberos/account_manager.h"

#include <limits>
#include <utility>

#include <base/base64.h>
#include <base/files/file_util.h>
#include <base/logging.h>
#include <base/strings/string_util.h>
#include <libpasswordprovider/password.h>
#include <libpasswordprovider/password_provider.h>

#include "kerberos/krb5_interface.h"

namespace kerberos {

namespace {

constexpr int kInvalidIndex = -1;

// Kerberos config files are stored as storage_dir/account_dir/this.
constexpr char kKrb5ConfFilePart[] = "krb5.conf";
// Kerberos credential caches are stored as storage_dir/account_dir/this.
constexpr char kKrb5CCFilePart[] = "krb5cc";
// Passwords are stored as storage_dir/account_dir/this.
constexpr char kPasswordFilePart[] = "password";
// Account data is stored as storage_dir + this.
constexpr char kAccountsFile[] = "accounts";

// Size limit for file (1 MB).
constexpr size_t kFileSizeLimit = 1024 * 1024;

// Returns the base64 encoded |principal_name|. This is used to create safe
// filenames while at the same time allowing easy debugging.
std::string GetSafeFilename(const std::string& principal_name) {
  std::string encoded_principal;
  base::Base64Encode(principal_name, &encoded_principal);
  return encoded_principal;
}

// Reads the file at |path| into |data|. Returns |ERROR_LOCAL_IO| if the file
// could not be read.
WARN_UNUSED_RESULT ErrorType LoadFile(const base::FilePath& path,
                                      std::string* data) {
  data->clear();
  if (!base::ReadFileToStringWithMaxSize(path, data, kFileSizeLimit)) {
    PLOG(ERROR) << "Failed to read " << path.value();
    data->clear();
    return ERROR_LOCAL_IO;
  }
  return ERROR_NONE;
}

// Writes |data| to the file at |path|. Returns |ERROR_LOCAL_IO| if the file
// could not be written.
ErrorType SaveFile(const base::FilePath& path, const std::string& data) {
  const int data_size = static_cast<int>(data.size());
  if (base::WriteFile(path, data.data(), data_size) != data_size) {
    LOG(ERROR) << "Failed to write '" << path.value() << "'";
    return ERROR_LOCAL_IO;
  }
  return ERROR_NONE;
}

}  // namespace

AccountManager::AccountManager(
    base::FilePath storage_dir,
    KerberosFilesChangedCallback kerberos_files_changed,
    KerberosTicketExpiringCallback kerberos_ticket_expiring,
    std::unique_ptr<Krb5Interface> krb5,
    std::unique_ptr<password_provider::PasswordProviderInterface>
        password_provider)
    : storage_dir_(std::move(storage_dir)),
      kerberos_files_changed_(std::move(kerberos_files_changed)),
      kerberos_ticket_expiring_(std::move(kerberos_ticket_expiring)),
      krb5_(std::move(krb5)),
      password_provider_(std::move(password_provider)) {
  DCHECK(kerberos_files_changed_);
  DCHECK(kerberos_ticket_expiring_);
}

AccountManager::~AccountManager() = default;

ErrorType AccountManager::SaveAccounts() const {
  // Copy |accounts_| into proto message.
  AccountDataList storage_accounts;
  for (const auto& account : accounts_)
    *storage_accounts.add_accounts() = account;

  // Store serialized proto message on disk.
  std::string accounts_blob;
  if (!storage_accounts.SerializeToString(&accounts_blob)) {
    LOG(ERROR) << "Failed to serialize accounts list to string";
    return ERROR_LOCAL_IO;
  }
  return SaveFile(GetAccountsPath(), accounts_blob);
}

ErrorType AccountManager::LoadAccounts() {
  accounts_.clear();

  // A missing file counts as a file with empty data.
  const base::FilePath accounts_path = GetAccountsPath();
  if (!base::PathExists(accounts_path))
    return ERROR_NONE;

  // Load serialized proto blob.
  std::string accounts_blob;
  ErrorType error = LoadFile(accounts_path, &accounts_blob);
  if (error != ERROR_NONE)
    return error;

  // Parse blob into proto message.
  AccountDataList storage_accounts;
  if (!storage_accounts.ParseFromString(accounts_blob)) {
    LOG(ERROR) << "Failed to parse accounts list from string";
    return ERROR_LOCAL_IO;
  }

  // Copy data into |accounts_|.
  accounts_.reserve(storage_accounts.accounts_size());
  for (int n = 0; n < storage_accounts.accounts_size(); ++n)
    accounts_.push_back(storage_accounts.accounts(n));

  return ERROR_NONE;
}

ErrorType AccountManager::AddAccount(const std::string& principal_name,
                                     bool is_managed) {
  int index = GetAccountIndex(principal_name);
  if (index != kInvalidIndex) {
    // Policy should overwrite user-added accounts, but user-added accounts
    // should not overwrite policy accounts.
    if (!accounts_[index].is_managed() && is_managed) {
      DeleteAllFilesFor(principal_name);
      accounts_[index].set_is_managed(is_managed);
      SaveAccounts();
    }
    return ERROR_DUPLICATE_PRINCIPAL_NAME;
  }

  // Create the account directory.
  const base::FilePath account_dir = GetAccountDir(principal_name);
  base::File::Error error;
  if (!base::CreateDirectoryAndGetError(account_dir, &error)) {
    LOG(ERROR) << "Failed to create directory '" << account_dir.value()
               << "': " << base::File::ErrorToString(error);
    return ERROR_LOCAL_IO;
  }

  // Create account record.
  AccountData data;
  data.set_principal_name(principal_name);
  data.set_is_managed(is_managed);
  accounts_.push_back(std::move(data));
  SaveAccounts();
  return ERROR_NONE;
}

ErrorType AccountManager::RemoveAccount(const std::string& principal_name) {
  int index = GetAccountIndex(principal_name);
  if (index == kInvalidIndex)
    return ERROR_UNKNOWN_PRINCIPAL_NAME;

  DeleteAllFilesFor(principal_name);
  accounts_.erase(accounts_.begin() + index);

  SaveAccounts();
  return ERROR_NONE;
}

void AccountManager::DeleteAllFilesFor(const std::string& principal_name) {
  const bool krb5cc_existed = base::PathExists(GetKrb5CCPath(principal_name));
  CHECK(base::DeleteFile(GetAccountDir(principal_name), true /* recursive */));
  if (krb5cc_existed)
    TriggerKerberosFilesChanged(principal_name);
}

ErrorType AccountManager::ClearAccounts(ClearMode mode) {
  // Early out.
  if (accounts_.size() == 0)
    return ERROR_NONE;

  switch (mode) {
    case CLEAR_ALL: {
      ClearAllAccounts();
      break;
    }
    case CLEAR_ONLY_UNMANAGED_ACCOUNTS: {
      ClearUnmanagedAccounts();
      break;
    }
    case CLEAR_ONLY_UNMANAGED_REMEMBERED_PASSWORDS: {
      ClearRememberedPasswordsForUnmanagedAccounts();
      break;
    }
  }
  return ERROR_NONE;
}

void AccountManager::ClearAllAccounts() {
  for (const auto& account : accounts_)
    DeleteAllFilesFor(account.principal_name());
  accounts_.clear();
  SaveAccounts();
}

void AccountManager::ClearUnmanagedAccounts() {
  for (int n = static_cast<int>(accounts_.size()) - 1; n >= 0; --n) {
    if (accounts_[n].is_managed())
      continue;
    DeleteAllFilesFor(accounts_[n].principal_name());
    accounts_.erase(accounts_.begin() + n);
  }
  SaveAccounts();
}

void AccountManager::ClearRememberedPasswordsForUnmanagedAccounts() {
  for (const auto& account : accounts_) {
    if (account.is_managed())
      continue;
    CHECK(base::DeleteFile(GetPasswordPath(account.principal_name()),
                           false /* recursive */));
  }
}

ErrorType AccountManager::ListAccounts(std::vector<Account>* accounts) const {
  for (const auto& it : accounts_) {
    Account account;
    account.set_principal_name(it.principal_name());
    account.set_is_managed(it.is_managed());
    account.set_password_was_remembered(
        base::PathExists(GetPasswordPath(it.principal_name())));
    account.set_use_login_password(it.use_login_password());

    // TODO(https://crbug.com/952239): Set additional properties.

    // Do a best effort reporting results, don't bail on the first error. If
    // there's a broken account, the user is able to recover the situation
    // this way (reauthenticate or remove account and add back).

    // Check PathExists, so that no error is printed if the file doesn't
    // exist.
    std::string krb5conf;
    const base::FilePath krb5conf_path = GetKrb5ConfPath(it.principal_name());
    if (base::PathExists(krb5conf_path) &&
        LoadFile(krb5conf_path, &krb5conf) == ERROR_NONE) {
      account.set_krb5conf(krb5conf);
    }

    // A missing krb5cc file just translates to an invalid ticket (lifetime
    // 0).
    Krb5Interface::TgtStatus tgt_status;
    const base::FilePath krb5cc_path = GetKrb5CCPath(it.principal_name());
    if (base::PathExists(krb5cc_path) &&
        krb5_->GetTgtStatus(krb5cc_path, &tgt_status) == ERROR_NONE) {
      account.set_tgt_validity_seconds(tgt_status.validity_seconds);
      account.set_tgt_renewal_seconds(tgt_status.renewal_seconds);
    }

    accounts->push_back(std::move(account));
  }

  return ERROR_NONE;
}

ErrorType AccountManager::SetConfig(const std::string& principal_name,
                                    const std::string& krb5conf) const {
  const AccountData* data = GetAccountData(principal_name);
  if (!data)
    return ERROR_UNKNOWN_PRINCIPAL_NAME;

  ErrorType error = SaveFile(GetKrb5ConfPath(principal_name), krb5conf);

  // Triggering the signal is only necessary if the credential cache exists.
  if (error == ERROR_NONE && base::PathExists(GetKrb5CCPath(principal_name)))
    TriggerKerberosFilesChanged(principal_name);
  return error;
}

ErrorType AccountManager::AcquireTgt(const std::string& principal_name,
                                     std::string password,
                                     bool remember_password,
                                     bool use_login_password) {
  AccountData* data = GetMutableAccountData(principal_name);
  if (!data)
    return ERROR_UNKNOWN_PRINCIPAL_NAME;

  // Remember whether to use the login password.
  if (data->use_login_password() != use_login_password) {
    data->set_use_login_password(use_login_password);
    SaveAccounts();
  }

  ErrorType error = use_login_password
                        ? UpdatePasswordFromLogin(principal_name, &password)
                        : UpdatePasswordFromSaved(principal_name,
                                                  remember_password, &password);
  if (error != ERROR_NONE)
    return error;

  // Acquire a Kerberos ticket-granting-ticket.
  error =
      krb5_->AcquireTgt(principal_name, password, GetKrb5CCPath(principal_name),
                        GetKrb5ConfPath(principal_name));

  // Assume the ticket changed if AcquireTgt() was successful.
  if (error == ERROR_NONE)
    TriggerKerberosFilesChanged(principal_name);
  return error;
}

ErrorType AccountManager::GetKerberosFiles(const std::string& principal_name,
                                           KerberosFiles* files) const {
  files->clear_krb5cc();
  files->clear_krb5conf();

  const AccountData* data = GetAccountData(principal_name);
  if (!data)
    return ERROR_UNKNOWN_PRINCIPAL_NAME;

  // By convention, no credential cache means no error.
  const base::FilePath krb5cc_path = GetKrb5CCPath(principal_name);
  if (!base::PathExists(krb5cc_path))
    return ERROR_NONE;

  std::string krb5cc;
  ErrorType error = LoadFile(krb5cc_path, &krb5cc);
  if (error != ERROR_NONE)
    return error;

  std::string krb5conf;
  error = LoadFile(GetKrb5ConfPath(principal_name), &krb5conf);
  if (error != ERROR_NONE)
    return error;

  files->mutable_krb5cc()->assign(krb5cc.begin(), krb5cc.end());
  files->mutable_krb5conf()->assign(krb5conf.begin(), krb5conf.end());
  return ERROR_NONE;
}

void AccountManager::TriggerKerberosTicketExpiringForExpiredTickets() {
  for (const auto& it : accounts_) {
    const base::FilePath krb5cc_path = GetKrb5CCPath(it.principal_name());
    if (!base::PathExists(krb5cc_path))
      continue;

    // A ticket where GetTgtStatus fails is considered broken and hence invalid.
    Krb5Interface::TgtStatus tgt_status;
    if (krb5_->GetTgtStatus(krb5cc_path, &tgt_status) != ERROR_NONE ||
        tgt_status.validity_seconds <= 0) {
      TriggerKerberosTicketExpiring(it.principal_name());
    }
  }
}

// static
std::string AccountManager::GetSafeFilenameForTesting(
    const std::string& principal_name) {
  return GetSafeFilename(principal_name);
}

void AccountManager::TriggerKerberosFilesChanged(
    const std::string& principal_name) const {
  kerberos_files_changed_.Run(principal_name);
}

void AccountManager::TriggerKerberosTicketExpiring(
    const std::string& principal_name) const {
  kerberos_ticket_expiring_.Run(principal_name);
}

base::FilePath AccountManager::GetAccountDir(
    const std::string& principal_name) const {
  return storage_dir_.Append(GetSafeFilename(principal_name));
}

base::FilePath AccountManager::GetKrb5ConfPath(
    const std::string& principal_name) const {
  return GetAccountDir(principal_name).Append(kKrb5ConfFilePart);
}

base::FilePath AccountManager::GetKrb5CCPath(
    const std::string& principal_name) const {
  return GetAccountDir(principal_name).Append(kKrb5CCFilePart);
}

base::FilePath AccountManager::GetPasswordPath(
    const std::string& principal_name) const {
  return GetAccountDir(principal_name).Append(kPasswordFilePart);
}

base::FilePath AccountManager::GetAccountsPath() const {
  return storage_dir_.Append(kAccountsFile);
}

ErrorType AccountManager::UpdatePasswordFromLogin(
    const std::string& principal_name, std::string* password) {
  // Erase a previously remembered password.
  base::DeleteFile(GetPasswordPath(principal_name), false /* recursive */);

  // Get login password from |password_provider_|.
  std::unique_ptr<password_provider::Password> login_password =
      password_provider_->GetPassword();
  if (!login_password || login_password->size() == 0) {
    password->clear();
    LOG(WARNING) << "Unable to retrieve login password";
  } else {
    *password = std::string(login_password->GetRaw(), login_password->size());
  }
  return ERROR_NONE;
}

ErrorType AccountManager::UpdatePasswordFromSaved(
    const std::string& principal_name,
    bool remember_password,
    std::string* password) {
  // Decision table what to do with the password:
  // pw empty / remember| false                      | true
  // -------------------+----------------------------+------------------------
  // false              | use given, erase file      | use given, save to file
  // true               | load from file, erase file | load from file

  // Remember password (even if authentication is going to fail below).
  const base::FilePath password_path = GetPasswordPath(principal_name);
  if (!password->empty() && remember_password)
    SaveFile(password_path, *password);

  // Try to load a saved password if available and none is given.
  if (password->empty() && base::PathExists(password_path)) {
    ErrorType error = LoadFile(password_path, password);
    if (error != ERROR_NONE)
      return error;
  }

  // Erase a previously remembered password.
  if (!remember_password)
    base::DeleteFile(password_path, false /* recursive */);

  return ERROR_NONE;
}

int AccountManager::GetAccountIndex(const std::string& principal_name) const {
  for (size_t n = 0; n < accounts_.size(); ++n) {
    if (accounts_[n].principal_name() == principal_name) {
      CHECK(n <= std::numeric_limits<int>::max());
      return static_cast<int>(n);
    }
  }
  return kInvalidIndex;
}

const AccountData* AccountManager::GetAccountData(
    const std::string& principal_name) const {
  int index = GetAccountIndex(principal_name);
  return index != kInvalidIndex ? &accounts_[index] : nullptr;
}

AccountData* AccountManager::GetMutableAccountData(
    const std::string& principal_name) {
  int index = GetAccountIndex(principal_name);
  return index != kInvalidIndex ? &accounts_[index] : nullptr;
}

}  // namespace kerberos
