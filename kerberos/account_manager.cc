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

#include "kerberos/krb5_interface.h"

namespace kerberos {

namespace {

// Kerberos config files are stored as storage_dir + principal hash + this.
constexpr char kKrb5ConfFilePart[] = "_krb5.conf";
// Kerberos credential caches are stored as storage_dir + principal hash + this.
constexpr char kKrb5CCFilePart[] = "_krb5cc";

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
WARN_UNUSED_RESULT ErrorType ReadFile(const base::FilePath& path,
                                      std::string* data) {
  data->clear();
  if (!base::ReadFileToStringWithMaxSize(path, data, kKrb5FileSizeLimit)) {
    PLOG(ERROR) << "Failed to read '" << path.value() << "'";
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

ErrorType AccountManager::AddAccount(const std::string& principal_name) {
  if (accounts_.find(principal_name) != accounts_.end())
    return ERROR_DUPLICATE_PRINCIPAL_NAME;

  auto data = std::make_unique<AccountData>();
  std::string hash = HashPrincipal(principal_name);
  data->krb5conf_path = storage_dir_.Append(hash + kKrb5ConfFilePart);
  data->krb5cc_path = storage_dir_.Append(hash + kKrb5CCFilePart);
  accounts_[principal_name] = std::move(data);
  return ERROR_NONE;
}

ErrorType AccountManager::RemoveAccount(const std::string& principal_name) {
  auto it = accounts_.find(principal_name);
  if (it == accounts_.end())
    return ERROR_UNKNOWN_PRINCIPAL_NAME;
  AccountData* data = it->second.get();
  DCHECK(data);

  base::DeleteFile(data->krb5conf_path, false /* recursive */);
  base::DeleteFile(data->krb5cc_path, false /* recursive */);
  accounts_.erase(it);

  TriggerKerberosFilesChanged(principal_name);
  return ERROR_NONE;
}

ErrorType AccountManager::ListAccounts(std::vector<Account>* accounts) {
  for (const auto& it : accounts_) {
    const std::string& principal_name = it.first;
    const AccountData* data = it.second.get();
    DCHECK(data);

    Account account;
    account.set_principal_name(principal_name);

    // Do a best effort reporting results, don't bail on the first error. If
    // there's a broken account, the user is able to recover the situation this
    // way (reauthenticate or remove account and add back).

    // Check PathExists, so that no error is printed if the file doesn't exist.
    std::string krb5conf;
    if (base::PathExists(data->krb5conf_path) &&
        ReadFile(data->krb5conf_path, &krb5conf) == ERROR_NONE) {
      account.set_krb5conf(krb5conf);
    }

    // A missing krb5cc file just translates to an invalid ticket (lifetime 0).
    Krb5Interface::TgtStatus tgt_status;
    if (base::PathExists(data->krb5cc_path) &&
        krb5_->GetTgtStatus(data->krb5cc_path, &tgt_status) == ERROR_NONE) {
      account.set_tgt_validity_seconds(tgt_status.validity_seconds);
      account.set_tgt_renewal_seconds(tgt_status.renewal_seconds);
    }

    accounts->push_back(std::move(account));
  }

  return ERROR_NONE;
}

ErrorType AccountManager::SetConfig(const std::string& principal_name,
                                    const std::string& krb5conf) {
  base::Optional<AccountData> data = GetAccountData(principal_name);
  if (!data)
    return ERROR_UNKNOWN_PRINCIPAL_NAME;

  ErrorType error = SaveFile(data->krb5conf_path, krb5conf);
  if (error == ERROR_NONE)
    TriggerKerberosFilesChanged(principal_name);
  return error;
}

ErrorType AccountManager::AcquireTgt(const std::string& principal_name,
                                     const std::string& password) {
  base::Optional<AccountData> data = GetAccountData(principal_name);
  if (!data)
    return ERROR_UNKNOWN_PRINCIPAL_NAME;

  ErrorType error = krb5_->AcquireTgt(principal_name, password,
                                      data->krb5cc_path, data->krb5conf_path);

  // Assume the ticket changed if AcquireTgt() was successful.
  if (error == ERROR_NONE)
    TriggerKerberosFilesChanged(principal_name);
  return error;
}

ErrorType AccountManager::GetKerberosFiles(const std::string& principal_name,
                                           KerberosFiles* files) {
  files->clear_krb5cc();
  files->clear_krb5conf();

  base::Optional<AccountData> data = GetAccountData(principal_name);
  if (!data)
    return ERROR_UNKNOWN_PRINCIPAL_NAME;

  // By convention, no credential cache means no error.
  if (!base::PathExists(data->krb5cc_path))
    return ERROR_NONE;

  std::string krb5cc;
  ErrorType error = ReadFile(data->krb5cc_path, &krb5cc);
  if (error != ERROR_NONE)
    return error;

  std::string krb5conf;
  error = ReadFile(data->krb5conf_path, &krb5conf);
  if (error != ERROR_NONE)
    return error;

  files->mutable_krb5cc()->assign(krb5cc.begin(), krb5cc.end());
  files->mutable_krb5conf()->assign(krb5conf.begin(), krb5conf.end());
  return ERROR_NONE;
}

void AccountManager::TriggerKerberosFilesChanged(
    const std::string& principal_name) {
  if (!kerberos_files_changed_.is_null())
    kerberos_files_changed_.Run(principal_name);
}

base::Optional<AccountManager::AccountData> AccountManager::GetAccountData(
    const std::string& principal_name) {
  auto it = accounts_.find(principal_name);
  if (it == accounts_.end())
    return base::nullopt;
  AccountData* data = it->second.get();
  DCHECK(data);
  return *data;
}

}  // namespace kerberos
