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

}  // namespace

AccountManager::AccountManager(const base::FilePath& storage_dir)
    // TODO(https://crbug.com/951740): Make |krb5_| overridable for testing.
    : storage_dir_(storage_dir), krb5_(std::make_unique<Krb5Interface>()) {}

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
  return ERROR_NONE;
}

ErrorType AccountManager::SetConfig(const std::string& principal_name,
                                    const std::string& krb5_conf) {
  base::Optional<AccountData> data = GetAccountData(principal_name);
  if (!data)
    return ERROR_UNKNOWN_PRINCIPAL_NAME;

  const int krb5_conf_size = static_cast<int>(krb5_conf.size());
  if (base::WriteFile(data->krb5conf_path, krb5_conf.data(), krb5_conf_size) !=
      krb5_conf_size) {
    LOG(ERROR) << "Failed to write '" << data->krb5conf_path.value() << "'";
    return ERROR_LOCAL_IO;
  }

  return ERROR_NONE;
}

ErrorType AccountManager::AcquireTgt(const std::string& principal_name,
                                     const std::string& password) {
  base::Optional<AccountData> data = GetAccountData(principal_name);
  if (!data)
    return ERROR_UNKNOWN_PRINCIPAL_NAME;

  return krb5_->AcquireTgt(principal_name, password, data->krb5cc_path,
                           data->krb5conf_path);
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
