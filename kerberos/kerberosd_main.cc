// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include <base/logging.h>
#include <base/run_loop.h>
#include <base/strings/stringprintf.h>
#include <brillo/syslog_logging.h>
#include <libpasswordprovider/password_provider.h>
#include "base/command_line.h"

#include "kerberos/account_manager.h"
#include "kerberos/error_strings.h"
#include "kerberos/kerberos_daemon.h"
#include "kerberos/kerberos_metrics.h"
#include "kerberos/krb5_interface_impl.h"
#include "kerberos/krb5_jail_wrapper.h"

namespace kerberos {
namespace {

int AddAccount(AccountManager* mgr,
               const std::string& principal,
               const std::string& password) {
  constexpr char kKrb5ConfData[] = R"([libdefaults]
    default_tgs_enctypes = aes256-cts-hmac-sha1-96 aes128-cts-hmac-sha1-96
    default_tkt_enctypes = aes256-cts-hmac-sha1-96 aes128-cts-hmac-sha1-96
    permitted_enctypes = aes256-cts-hmac-sha1-96 aes128-cts-hmac-sha1-96)";

  ErrorType error = mgr->AddAccount(principal, false);
  CHECK_EQ(ERROR_NONE, error);
  error = mgr->SetConfig(principal, kKrb5ConfData);
  CHECK_EQ(ERROR_NONE, error);
  error = mgr->AcquireTgt(principal, password, false, false);
  LOG(ERROR) << "AcquireTgt -> " << GetErrorString(error);
  return error == ERROR_NONE ? 0 : 1;
}

int RemoveAccount(AccountManager* mgr, const std::string& principal) {
  ErrorType error = mgr->RemoveAccount(principal);
  return error == ERROR_NONE ? 0 : 1;
}

int ListAccounts(AccountManager* mgr) {
  std::vector<Account> accounts;
  ErrorType error = mgr->ListAccounts(&accounts);
  LOG(ERROR) << "ListAccounts -> " << GetErrorString(error);
  LOG(ERROR) << "Listing " << accounts.size() << " accounts";
  for (const auto& account : accounts) {
    LOG(INFO) << account.principal_name() << " conf=" << account.krb5conf()
              << " valid=" << account.tgt_validity_seconds()
              << " renewal=" << account.tgt_renewal_seconds();
  }
  return 0;
}

void OnFilesChanged(const std::string& principal_name) {}
void OnTicketExpiring(const std::string& principal_name) {}

int HandleCommandLine(int argc, char* argv[]) {
  base::MessageLoop loop_;
  char option = argv[1][0];
  const base::FilePath storage_dir("/tmp");
  KerberosMetrics metrics(storage_dir);
  AccountManager mgr(
      storage_dir, base::BindRepeating(&OnFilesChanged),
      base::BindRepeating(&OnTicketExpiring),
      std::make_unique<Krb5JailWrapper>(std::make_unique<Krb5InterfaceImpl>()),
      std::make_unique<password_provider::PasswordProvider>(), &metrics);

  switch (option) {
    case 'a':
      // Add account
      if (argc <= 3) {
        LOG(ERROR) << "AcquireTgt. Usage: kerberosd a <principal> <password>";
        return 1;
      }

      return AddAccount(&mgr, argv[2], argv[3]) || ListAccounts(&mgr);

    case 'r':
      // Remove accounts
      if (argc <= 2) {
        LOG(ERROR) << "RemoveAccount. Usage: kerberosd r <principal>";
        return 1;
      }
      return RemoveAccount(&mgr, argv[2]);

    case 'l':
      // List accounts
      if (argc <= 1) {
        LOG(ERROR) << "ListAccounts. Usage: kerberosd l";
        return 1;
      }
      return ListAccounts(&mgr);
  }

  LOG(ERROR) << "Unknown option '" << option << "'. Should be 'a' or 'l'.";
  return 1;
}

}  // namespace
}  // namespace kerberos

int main(int argc, char* argv[]) {
  base::CommandLine::Init(argc, argv);
  brillo::OpenLog("kerberosd", true /* log_pid */);
  brillo::InitLog(brillo::kLogToSyslog | brillo::kLogToStderrIfTty |
                  brillo::kLogToStderr);

  if (argc > 1 && strchr("arl", argv[1][0]))
    return kerberos::HandleCommandLine(argc, argv);

  // Run daemon.
  LOG(INFO) << "kerberosd starting";
  kerberos::KerberosDaemon daemon;
  int result = daemon.Run();
  LOG(INFO) << "kerberosd stopping with exit code " << result;

  return result;
}
