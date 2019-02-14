// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef KERBEROS_ACCOUNT_MANAGER_H_
#define KERBEROS_ACCOUNT_MANAGER_H_

#include <memory>
#include <string>
#include <unordered_map>

#include <base/compiler_specific.h>
#include <base/macros.h>
#include <base/optional.h>

#include "kerberos/proto_bindings/kerberos_service.pb.h"

namespace kerberos {

// Manages Kerberos tickets for a set of accounts keyed by principal name
// (user@REALM.COM).
class AccountManager {
 public:
  AccountManager();
  ~AccountManager();

  // Adds an account keyed by |principal_name| (user@REALM.COM) to the list of
  // accounts. Returns |ERROR_DUPLICATE_PRINCIPAL_NAME| if the account is
  // already present.
  ErrorType AddAccount(const std::string& principal_name) WARN_UNUSED_RESULT;

  // Removes the account keyed by |principal_name| (user@REALM.COM) from the
  // list of accounts. Returns |ERROR_UNKNOWN_PRINCIPAL_NAME| if there is no
  // such account.
  ErrorType RemoveAccount(const std::string& principal_name) WARN_UNUSED_RESULT;

  // Acquires a Kerberos ticket-granting-ticket for the account keyed by
  // |principal_name| (user@REALM.COM). Returns |ERROR_UNKNOWN_PRINCIPAL_NAME|
  // if there is no such account.
  ErrorType AcquireTgt(const std::string& principal_name,
                       const std::string& password) WARN_UNUSED_RESULT;

  // Retrieves the Kerberos credential cache and the configuration file for the
  // account keyed by |principal_name| (user@REALM.COM). Returns
  // |ERROR_UNKNOWN_PRINCIPAL_NAME| if there is no such account.
  ErrorType GetKerberosFiles(const std::string& principal_name,
                             KerberosFiles* files) WARN_UNUSED_RESULT;

 private:
  struct AccountData {};

  // Returns the AccountData for |principal_name| if available or nullopt
  // otherwise.
  base::Optional<AccountData> GetAccountData(const std::string& principal_name);

  // Maps principal name (user@REALM.COM) to account data.
  using AccountsMap =
      std::unordered_map<std::string, std::unique_ptr<AccountData>>;
  AccountsMap accounts_;

  DISALLOW_COPY_AND_ASSIGN(AccountManager);
};

}  // namespace kerberos

#endif  // KERBEROS_ACCOUNT_MANAGER_H_
