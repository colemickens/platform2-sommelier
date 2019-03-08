// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef KERBEROS_ACCOUNT_MANAGER_H_
#define KERBEROS_ACCOUNT_MANAGER_H_

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <base/callback.h>
#include <base/compiler_specific.h>
#include <base/files/file_path.h>
#include <base/macros.h>
#include <base/optional.h>

#include "kerberos/proto_bindings/kerberos_service.pb.h"

namespace kerberos {

class Krb5Interface;

// Manages Kerberos tickets for a set of accounts keyed by principal name
// (user@REALM.COM).
class AccountManager {
 public:
  using KerberosFilesChangedCallback =
      base::RepeatingCallback<void(const std::string& principal_name)>;

  // |storage_dir| is the path where configs and credential caches are stored.
  // |kerberos_files_changed| is a callback that gets called when either the
  // Kerberos credential cache or the configuration file changes for a specific
  // account. Use in combination with GetKerberosFiles() to get the latest
  // files.
  explicit AccountManager(base::FilePath storage_dir,
                          KerberosFilesChangedCallback kerberos_files_changed);
  ~AccountManager();

  // Adds an account keyed by |principal_name| (user@REALM.COM) to the list of
  // accounts. Returns |ERROR_DUPLICATE_PRINCIPAL_NAME| if the account is
  // already present.
  ErrorType AddAccount(const std::string& principal_name) WARN_UNUSED_RESULT;

  // The following methods return |ERROR_UNKNOWN_PRINCIPAL_NAME| if
  // |principal_name| (user@REALM.COM) is not known.

  // Removes the account keyed by |principal_name| from the list of accounts.
  ErrorType RemoveAccount(const std::string& principal_name) WARN_UNUSED_RESULT;

  // Returns a list of all existing accounts, including current status like
  // remaining Kerberos ticket lifetime. Does a best effort returning results.
  // See documentation of |Account| for more details.
  ErrorType ListAccounts(std::vector<Account>* accounts);

  // Sets the Kerberos configuration (krb5.conf) used for the given
  // |principal_name|.
  ErrorType SetConfig(const std::string& principal_name,
                      const std::string& krb5_conf) WARN_UNUSED_RESULT;

  // Acquires a Kerberos ticket-granting-ticket for the account keyed by
  // |principal_name|.
  ErrorType AcquireTgt(const std::string& principal_name,
                       const std::string& password) WARN_UNUSED_RESULT;

  // Retrieves the Kerberos credential cache and the configuration file for the
  // account keyed by |principal_name|. Returns ERROR_NONE if both files could
  // be retrieved or if the credential cache is missing. Returns ERROR_LOCAL_IO
  // if any of the files failed to read.
  ErrorType GetKerberosFiles(const std::string& principal_name,
                             KerberosFiles* files) WARN_UNUSED_RESULT;

 private:
  // Directory where account data is stored.
  const base::FilePath storage_dir_;

  // Gets called when the Kerberos configuration or credential cache changes for
  // a specific account.
  const KerberosFilesChangedCallback kerberos_files_changed_;

  // Interface for Kerberos methods (may be overridden for tests).
  const std::unique_ptr<Krb5Interface> krb5_;

  // Calls |kerberos_files_changed_| if set.
  void TriggerKerberosFilesChanged(const std::string& principal_name);

  struct AccountData {
    // File path for the Kerberos configuration.
    base::FilePath krb5conf_path;
    // File path for the Kerberos credential cache.
    base::FilePath krb5cc_path;
  };

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
