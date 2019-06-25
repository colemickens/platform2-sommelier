// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef KERBEROS_ACCOUNT_MANAGER_H_
#define KERBEROS_ACCOUNT_MANAGER_H_

#include <memory>
#include <string>
#include <vector>

#include <base/callback.h>
#include <base/compiler_specific.h>
#include <base/files/file_path.h>
#include <base/macros.h>
#include <base/optional.h>

#include "bindings/kerberos_containers.pb.h"
#include "kerberos/proto_bindings/kerberos_service.pb.h"

namespace password_provider {
class PasswordProviderInterface;
}

namespace kerberos {

class Krb5Interface;

// Manages Kerberos tickets for a set of accounts keyed by principal name
// (user@REALM.COM).
class AccountManager {
 public:
  using KerberosFilesChangedCallback =
      base::RepeatingCallback<void(const std::string& principal_name)>;
  using KerberosTicketExpiringCallback =
      base::RepeatingCallback<void(const std::string& principal_name)>;

  // |storage_dir| is the path where configs and credential caches are stored.
  // |kerberos_files_changed| is a callback that gets called when either the
  // Kerberos credential cache or the configuration file changes for a specific
  // account. Use in combination with GetKerberosFiles() to get the latest
  // files. |kerberos_ticket_expiring| is a callback that gets called when a
  // Kerberos TGT is about to expire. It should be used to notify the user.
  // |krb5| interacts with lower level Kerberos libraries. It can be overridden
  // for tests. |password_provider| is used to retrieve the login password. It
  // can be overridden for tests.
  AccountManager(base::FilePath storage_dir,
                 KerberosFilesChangedCallback kerberos_files_changed,
                 KerberosTicketExpiringCallback kerberos_ticket_expiring,
                 std::unique_ptr<Krb5Interface> krb5,
                 std::unique_ptr<password_provider::PasswordProviderInterface>
                     password_provider);
  ~AccountManager();

  // Saves all accounts to disk. Returns ERROR_LOCAL_IO and logs on error.
  ErrorType SaveAccounts() const;

  // Loads all accounts from disk. Returns ERROR_LOCAL_IO and logs on error.
  // Removes all old accounts before setting the new ones. Treats a non-existent
  // file on disk as if the file was empty, i.e. loading succeeds and the
  // account list is empty afterwards.
  ErrorType LoadAccounts();

  // Adds an account keyed by |principal_name| (user@REALM.COM) to the list of
  // accounts. |is_managed| indicates whether the account is managed by the
  // KerberosAccounts policy. Returns |ERROR_DUPLICATE_PRINCIPAL_NAME| if the
  // account is already present.
  ErrorType AddAccount(const std::string& principal_name,
                       bool is_managed) WARN_UNUSED_RESULT;

  // The following methods return |ERROR_UNKNOWN_PRINCIPAL_NAME| if
  // |principal_name| (user@REALM.COM) is not known.

  // Removes the account keyed by |principal_name| from the list of accounts.
  ErrorType RemoveAccount(const std::string& principal_name) WARN_UNUSED_RESULT;

  // Removes account data or full accounts, depending on |mode|..
  ErrorType ClearAccounts(ClearMode mode) WARN_UNUSED_RESULT;

  // Returns a list of all existing accounts, including current status like
  // remaining Kerberos ticket lifetime. Does a best effort returning results.
  // See documentation of |Account| for more details.
  ErrorType ListAccounts(std::vector<Account>* accounts) const
      WARN_UNUSED_RESULT;

  // Sets the Kerberos configuration (krb5.conf) used for the given
  // |principal_name|.
  ErrorType SetConfig(const std::string& principal_name,
                      const std::string& krb5_conf) const WARN_UNUSED_RESULT;

  // Acquires a Kerberos ticket-granting-ticket for the account keyed by
  // |principal_name| using |password|. If |password| is empty, a stored
  // password is used if available. If |remember_password| is true and
  // |password| is not empty, the password is stored on disk. If
  // |use_login_password| is true, the primary user's login password is used to
  // authenticate. Both |password| and |remember_password| are ignored by the
  // daemon in this case.
  ErrorType AcquireTgt(const std::string& principal_name,
                       std::string password,
                       bool remember_password,
                       bool use_login_password) WARN_UNUSED_RESULT;

  // Retrieves the Kerberos credential cache and the configuration file for the
  // account keyed by |principal_name|. Returns ERROR_NONE if both files could
  // be retrieved or if the credential cache is missing. Returns ERROR_LOCAL_IO
  // if any of the files failed to read.
  ErrorType GetKerberosFiles(const std::string& principal_name,
                             KerberosFiles* files) const WARN_UNUSED_RESULT;

  // Sends KerberosTicketExpiring signals for each expired Kerberos ticket.
  void TriggerKerberosTicketExpiringForExpiredTickets();

  const base::FilePath& GetStorageDirForTesting() { return storage_dir_; }

  // Returns the base64-encoded |principal_name|.
  static std::string GetSafeFilenameForTesting(
      const std::string& principal_name);

 private:
  // File path helpers. All paths are relative to |storage_dir_|.

  // Directory where files specific to the |principal_name| account are stored.
  base::FilePath GetAccountDir(const std::string& principal_name) const;

  // File path of the Kerberos configuration for the given |principal_name|.
  base::FilePath GetKrb5ConfPath(const std::string& principal_name) const;

  // File path of the Kerberos credential cache for the given |principal_name|.
  base::FilePath GetKrb5CCPath(const std::string& principal_name) const;

  // File path of the Kerberos password for the given |principal_name|.
  base::FilePath GetPasswordPath(const std::string& principal_name) const;

  // File path where |accounts_| is stored.
  base::FilePath GetAccountsPath() const;

  // Deletes all files (credential cache, password etc.) for the given
  // |principal_name|. Triggers KerberosFilesChanged if the credential cache was
  // deleted.
  void DeleteAllFilesFor(const std::string& principal_name);

  // Calls |kerberos_files_changed_|.
  void TriggerKerberosFilesChanged(const std::string& principal_name) const;

  // Calls |kerberos_ticket_expiring_|.
  void TriggerKerberosTicketExpiring(const std::string& principal_name) const;

  // Sets |password| to the login password. Removes a remembered password for
  // |principal_name| if there is any.
  ErrorType UpdatePasswordFromLogin(const std::string& principal_name,
                                    std::string* password);

  // If |password| is empty, loads it from the password file if that exists. If
  // |password| is not empty and |remember_password| is true, saves |password|
  // to the password file. If |remember_password| is false, deletes the password
  // file.
  ErrorType UpdatePasswordFromSaved(const std::string& principal_name,
                                    bool remember_password,
                                    std::string* password);

  // Different modes of clearing accounts, see ClearAccounts().
  void ClearAllAccounts();
  void ClearUnmanagedAccounts();
  void ClearRememberedPasswordsForUnmanagedAccounts();

  // Directory where all account data is stored.
  const base::FilePath storage_dir_;

  // Gets called when the Kerberos configuration or credential cache changes for
  // a specific account.
  const KerberosFilesChangedCallback kerberos_files_changed_;

  // Gets called when the a Kerberos ticket is about to expire in the next
  // couple of minutes or if it already expired.
  const KerberosTicketExpiringCallback kerberos_ticket_expiring_;

  // Interface for Kerberos methods (may be overridden for tests).
  const std::unique_ptr<Krb5Interface> krb5_;

  // Returns the index of the account for |principal_name| or |kInvalidIndex| if
  // the account does not exist.
  int GetAccountIndex(const std::string& principal_name) const;

  // Returns the AccountData for |principal_name| if available or nullptr
  // otherwise. The returned pointer may lose validity if |accounts_| gets
  // modified.
  const AccountData* GetAccountData(const std::string& principal_name) const;
  AccountData* GetMutableAccountData(const std::string& principal_name);

  // List of all accounts. Stored in a vector to keep order of addition.
  std::vector<AccountData> accounts_;

  // Interface to retrieve the login password.
  std::unique_ptr<password_provider::PasswordProviderInterface>
      password_provider_;

  DISALLOW_COPY_AND_ASSIGN(AccountManager);
};

}  // namespace kerberos

#endif  // KERBEROS_ACCOUNT_MANAGER_H_
