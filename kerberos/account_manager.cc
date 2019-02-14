// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "kerberos/account_manager.h"

#include <base/logging.h>

namespace kerberos {

AccountManager::AccountManager() = default;

AccountManager::~AccountManager() = default;

ErrorType AccountManager::AddAccount(const std::string& principal_name) {
  if (accounts_.find(principal_name) != accounts_.end())
    return ERROR_DUPLICATE_PRINCIPAL_NAME;

  accounts_[principal_name] = std::make_unique<AccountData>();
  return ERROR_NONE;
}

ErrorType AccountManager::RemoveAccount(const std::string& principal_name) {
  if (accounts_.erase(principal_name) == 0)
    return ERROR_UNKNOWN_PRINCIPAL_NAME;

  return ERROR_NONE;
}

ErrorType AccountManager::AcquireTgt(const std::string& principal_name,
                                     const std::string& password) {
  base::Optional<AccountData> data = GetAccountData(principal_name);
  if (!data)
    return ERROR_UNKNOWN_PRINCIPAL_NAME;

  // TODO(ljusten): Implement.
  return ERROR_NONE;
}

ErrorType AccountManager::GetKerberosFiles(const std::string& principal_name,
                                           KerberosFiles* files) {
  base::Optional<AccountData> data = GetAccountData(principal_name);
  if (!data)
    return ERROR_UNKNOWN_PRINCIPAL_NAME;

  // TODO(ljusten): Implement.
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
