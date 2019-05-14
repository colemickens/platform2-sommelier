// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "kerberos/account_manager.h"

#include <utility>

#include <base/files/file_util.h>
#include <base/logging.h>
#include <base/sha1.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/string_util.h>

#include "bindings/kerberos_containers.pb.h"
#include "kerberos/krb5_interface.h"

namespace kerberos {

namespace {

// Kerberos config files are stored as storage_dir + principal hash + this.
constexpr char kKrb5ConfFilePart[] = "_krb5.conf";
// Kerberos credential caches are stored as storage_dir + principal hash + this.
constexpr char kKrb5CCFilePart[] = "_krb5cc";
// Account data is stored as storage_dir + this.
constexpr char kAccountsFile[] = "accounts";

// Size limit for GetKerberosFiles (1 MB).
constexpr size_t kKrb5FileSizeLimit = 1024 * 1024;

// Returns the SHA1 hash of |principal_name| as hex string. This is used to
// generate (almost guaranteed) unique filenames for account data.
std::string HashPrincipal(const std::string& principal_name) {
  std::string hash = base::SHA1HashString(principal_name);
  return base::ToLowerASCII(base::HexEncode(hash.data(), hash.size()));
}

// Reads the file at |path| into |data|. Returns |ERROR_LOCAL_IO| if the file
// could not be read.
WARN_UNUSED_RESULT ErrorType LoadFile(const base::FilePath& path,
                                      std::string* data) {
  data->clear();
  if (!base::ReadFileToStringWithMaxSize(path, data, kKrb5FileSizeLimit)) {
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
    KerberosFilesChangedCallback kerberos_files_changed)
    // TODO(https://crbug.com/951740): Make |krb5_| overridable for testing.
    : storage_dir_(std::move(storage_dir)),
      kerberos_files_changed_(std::move(kerberos_files_changed)),
      krb5_(std::make_unique<Krb5Interface>()) {}

AccountManager::~AccountManager() = default;

ErrorType AccountManager::SaveAccounts() const {
  // Copy |accounts_| into proto message.
  StorageAccountsList storage_accounts;
  for (const auto& it : accounts_) {
    const std::string& principal_name = it.first;
    const AccountData* data = it.second.get();
    DCHECK(data);

    StorageAccount* storage_account = storage_accounts.add_accounts();
    storage_account->set_principal_name(principal_name);
    storage_account->set_is_managed(data->is_managed);
    // TODO(https://crbug.com/952239): Set additional properties.
  }

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
  StorageAccountsList storage_accounts;
  if (!storage_accounts.ParseFromString(accounts_blob)) {
    LOG(ERROR) << "Failed to parse accounts list from string";
    return ERROR_LOCAL_IO;
  }

  // Copy data into |accounts_|..
  for (int n = 0; n < storage_accounts.accounts_size(); ++n) {
    const StorageAccount& storage_account = storage_accounts.accounts(n);

    auto data = std::make_unique<AccountData>();
    data->is_managed = storage_account.is_managed();
    // TODO(https://crbug.com/952239): Set additional properties.

    accounts_[storage_account.principal_name()] = std::move(data);
  }

  return ERROR_NONE;
}

ErrorType AccountManager::AddAccount(const std::string& principal_name,
                                     bool is_managed) {
  auto it = accounts_.find(principal_name);
  if (it != accounts_.end()) {
    // Policy should overwrite user-added accounts, but user-added accounts
    // should not overwrite policy accounts.
    if (is_managed && !it->second->is_managed) {
      it->second->is_managed = is_managed;
      SaveAccounts();
    }
    return ERROR_DUPLICATE_PRINCIPAL_NAME;
  }

  auto data = std::make_unique<AccountData>();
  data->is_managed = is_managed;
  accounts_[principal_name] = std::move(data);
  SaveAccounts();
  return ERROR_NONE;
}

ErrorType AccountManager::RemoveAccount(const std::string& principal_name) {
  auto it = accounts_.find(principal_name);
  if (it == accounts_.end())
    return ERROR_UNKNOWN_PRINCIPAL_NAME;
  AccountData* data = it->second.get();
  DCHECK(data);

  base::DeleteFile(GetKrb5ConfPath(principal_name), false /* recursive */);
  base::DeleteFile(GetKrb5CCPath(principal_name), false /* recursive */);
  accounts_.erase(it);

  SaveAccounts();
  TriggerKerberosFilesChanged(principal_name);
  return ERROR_NONE;
}

ErrorType AccountManager::ListAccounts(std::vector<Account>* accounts) const {
  for (const auto& it : accounts_) {
    const std::string& principal_name = it.first;
    const AccountData* data = it.second.get();
    DCHECK(data);

    Account account;
    account.set_principal_name(principal_name);
    account.set_is_managed(data->is_managed);
    // TODO(https://crbug.com/952239): Set additional properties.

    // Do a best effort reporting results, don't bail on the first error. If
    // there's a broken account, the user is able to recover the situation this
    // way (reauthenticate or remove account and add back).

    // Check PathExists, so that no error is printed if the file doesn't exist.
    std::string krb5conf;
    const base::FilePath krb5conf_path = GetKrb5ConfPath(principal_name);
    if (base::PathExists(krb5conf_path) &&
        LoadFile(krb5conf_path, &krb5conf) == ERROR_NONE) {
      account.set_krb5conf(krb5conf);
    }

    // A missing krb5cc file just translates to an invalid ticket (lifetime 0).
    Krb5Interface::TgtStatus tgt_status;
    const base::FilePath krb5cc_path = GetKrb5CCPath(principal_name);
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
  if (error == ERROR_NONE)
    TriggerKerberosFilesChanged(principal_name);
  return error;
}

ErrorType AccountManager::AcquireTgt(const std::string& principal_name,
                                     const std::string& password) const {
  const AccountData* data = GetAccountData(principal_name);
  if (!data)
    return ERROR_UNKNOWN_PRINCIPAL_NAME;

  ErrorType error =
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

void AccountManager::TriggerKerberosFilesChanged(
    const std::string& principal_name) const {
  if (!kerberos_files_changed_.is_null())
    kerberos_files_changed_.Run(principal_name);
}

base::FilePath AccountManager::GetKrb5ConfPath(
    const std::string& principal_name) const {
  return storage_dir_.Append(HashPrincipal(principal_name) + kKrb5ConfFilePart);
}

base::FilePath AccountManager::GetKrb5CCPath(
    const std::string& principal_name) const {
  return storage_dir_.Append(HashPrincipal(principal_name) + kKrb5CCFilePart);
}

base::FilePath AccountManager::GetAccountsPath() const {
  return storage_dir_.Append(kAccountsFile);
}

const AccountManager::AccountData* AccountManager::GetAccountData(
    const std::string& principal_name) const {
  auto it = accounts_.find(principal_name);
  if (it == accounts_.end())
    return nullptr;
  const AccountData* data = it->second.get();
  DCHECK(data);
  return data;
}

}  // namespace kerberos
