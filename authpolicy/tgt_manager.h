// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AUTHPOLICY_TGT_MANAGER_H_
#define AUTHPOLICY_TGT_MANAGER_H_

#include <string>

#include <base/cancelable_callback.h>
#include <base/macros.h>
#include <base/single_thread_task_runner.h>

#include "authpolicy/path_service.h"
#include "authpolicy/proto_bindings/active_directory_info.pb.h"

namespace authpolicy {

namespace protos {
class TgtLifetime;
class DebugFlags;
}  // namespace protos

class AuthPolicyMetrics;
class JailHelper;
class PathService;
class ProcessExecutor;

// Responsible for acquiring a ticket-tranting-ticket (TGT) from an Active
// Directory key distribution center (KDC) and managing the TGT. The TGT is
// kept in a file, the credentials cache. Supports authentication via a password
// or a keytab file.
class TgtManager {
 public:
  TgtManager(scoped_refptr<base::SingleThreadTaskRunner> task_runner,
             const PathService* path_service,
             AuthPolicyMetrics* metrics,
             const protos::DebugFlags* flags,
             const JailHelper* jail_helper,
             Path config_path,
             Path credential_cache_path);
  ~TgtManager();

  // Acquires a TGT with the given |principal| (user@REALM or machine$@REALM)
  // and password file descriptor |password_fd|. |realm| is the Active Directory
  // realm (e.g. ENG.EXAMPLE.COM). |kdc_ip| is the key distribution center IP.
  // If the KDC cannot be contacted, the method retries once without prescribing
  // the KDC IP in the Kerberos configuration.
  ErrorType AcquireTgtWithPassword(const std::string& principal,
                                   int password_fd,
                                   const std::string& realm,
                                   const std::string& kdc_ip);

  // Acquires a TGT with the given |principal| (user@REALM or machine$@REALM)
  // and keytab file |keytab_path|. If the account has just been created, it
  // might not have propagated through Active Directory yet. In this case, set
  // |propagation_retry| to true. The method will then retry a few times if an
  // error occurs that indicates a propagation issue. |realm| is the Active
  // Directory realm (e.g. ENG.EXAMPLE.COM). |kdc_ip| is the key distribution
  // center IP. If the KDC cannot be contacted, the method tries again (in
  // addition to potential propagation retries) without prescribing the KDC IP
  // in the Kerberos configuration.
  ErrorType AcquireTgtWithKeytab(const std::string& principal,
                                 Path keytab_path,
                                 bool propagation_retry,
                                 const std::string& realm,
                                 const std::string& kdc_ip);

  // If enabled, the TGT renews automatically by scheduling RenewTgt()
  // periodically on the |task_runner_| (usually the D-Bus thread). Renewal must
  // happen within the the TGT's validity lifetime. The scheduling delay is a
  // fraction of that lifetime.
  void EnableTgtAutoRenewal(bool enabled);

  // Renews a TGT. Must happen within its validity lifetime.
  ErrorType RenewTgt();

  // Returns the lifetime of a TGT.
  ErrorType GetTgtLifetime(protos::TgtLifetime* lifetime);

  // Returns the file path of the Kerberos configuration file.
  Path GetConfigPath() const { return config_path_; }

  // Returns the file path of the Kerberos credential cache.
  Path GetCredentialCachePath() const { return credential_cache_path_; }

  // Disable retry sleep for unit tests.
  void DisableRetrySleepForTesting() { kinit_retry_sleep_enabled_ = false; }

 private:
  // Writes the Kerberos configuration and runs |kinit_cmd|. If
  // |propagation_retry| is true, tries up to |kKinitMaxRetries| times as long
  // as kinit returns an error indicating that the account hasn't propagated
  // through Active Directory yet.
  ErrorType RunKinit(ProcessExecutor* kinit_cmd, bool propagation_retry) const;

  // Writes the krb5 configuration file.
  ErrorType WriteKrb5Conf() const;

  // Turns on kinit trace logging if |flags_->TraceKinit()| is enabled.
  void SetupKinitTrace(ProcessExecutor* kinit_cmd) const;

  // Logs the kinit trace if |flags_->TraceKinit()| is enabled.
  void OutputKinitTrace() const;

  // Cancels |tgt_renewal_callback_|. If |tgt_autorenewal_enabled_| is true and
  // the TGT is valid, schedules RenewTgt() with a delay of a fraction of the
  // TGT's validity lifetime.
  void UpdateTgtAutoRenewal();

  // Callback scheduled to renew the TGT. Calls RenewTgt() internally and prints
  // appropriate error messages.
  void AutoRenewTgt();

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  const PathService* paths_ = nullptr;         // File paths, not owned.
  AuthPolicyMetrics* metrics_ = nullptr;       // UMA statistics, not owned.
  const protos::DebugFlags* flags_ = nullptr;  // Debug flags, not owned.
  const JailHelper* jail_helper_ = nullptr;    // Minijail related, not owned.
  Path config_path_ = Path::INVALID;
  Path credential_cache_path_ = Path::INVALID;

  // Realm and key distribution center (KDC) IP address written to the Kerberos
  // configuration file. |kdc_ip_| is optional, if empty, it is not written.
  // |kdc_ip_| may be cleared programmatically if fetching a TGT with prescribed
  // KDC IP fails with an error code that indicates that the KDC could not be
  // reached. In that case, the code retries and lets Samba query the KDC IP.
  std::string realm_;
  std::string kdc_ip_;

  // Whether the TGT was acquired for a user or machine principal. Determines
  // what error code is returned if the principal was bad.
  bool is_machine_principal_ = false;

  // Callback for automatic TGT renewal.
  base::CancelableClosure tgt_renewal_callback_;
  bool tgt_autorenewal_enabled_ = false;

  // Whether to sleep when retrying kinit (disable for testing).
  bool kinit_retry_sleep_enabled_ = true;

  DISALLOW_COPY_AND_ASSIGN(TgtManager);
};

}  // namespace authpolicy

#endif  // AUTHPOLICY_TGT_MANAGER_H_
