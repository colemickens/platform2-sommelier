// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "authpolicy/tgt_manager.h"

#include <base/files/file_util.h>
#include <base/strings/stringprintf.h>
#include <base/threading/platform_thread.h>

#include "authpolicy/authpolicy_metrics.h"
#include "authpolicy/constants.h"
#include "authpolicy/jail_helper.h"
#include "authpolicy/platform_helper.h"
#include "authpolicy/process_executor.h"
#include "authpolicy/samba_interface_internal.h"

namespace authpolicy {

namespace {

// Kerberos configuration file data.
const char kKrb5ConfData[] =
    "[libdefaults]\n"
    // Only allow AES. (No DES, no RC4.)
    "\tdefault_tgs_enctypes = aes256-cts-hmac-sha1-96 aes128-cts-hmac-sha1-96\n"
    "\tdefault_tkt_enctypes = aes256-cts-hmac-sha1-96 aes128-cts-hmac-sha1-96\n"
    "\tpermitted_enctypes = aes256-cts-hmac-sha1-96 aes128-cts-hmac-sha1-96\n"
    // Prune weak ciphers from the above list. With current settings it’s a
    // no-op, but still.
    "\tallow_weak_crypto = false\n"
    // Default is 300 seconds, but we might add a policy for that in the future.
    "\tclockskew = 300\n"
    // Required for password change.
    "\tdefault_realm = %s\n";
const char kKrb5RealmData[] =
    "[realms]\n"
    "\t%s = {\n"
    "\t\tkdc = [%s]\n"
    "\t}\n";

// Env variable to trace debug info of kinit.
const char kKrb5TraceEnvKey[] = "KRB5_TRACE";

// Maximum kinit tries.
const int kKinitMaxTries = 60;
// Wait interval between two kinit tries.
const int kKinitRetryWaitSeconds = 1;

// Keys for interpreting kinit output.
const char kKeyBadUserName[] =
    "not found in Kerberos database while getting initial credentials";
const char kKeyBadPassword[] =
    "Preauthentication failed while getting initial credentials";
const char kKeyPasswordExpiredStdout[] =
    "Password expired.  You must change it now.";
const char kKeyPasswordExpiredStderr[] =
    "Cannot read password while getting initial credentials";
const char kKeyCannotResolve[] =
    "Cannot resolve network address for KDC in realm";
const char kKeyCannotContactKDC[] = "Cannot contact any KDC";

ErrorType GetKinitError(const ProcessExecutor& kinit_cmd) {
  const std::string& kinit_out = kinit_cmd.GetStdout();
  const std::string& kinit_err = kinit_cmd.GetStderr();

  if (internal::Contains(kinit_err, kKeyCannotContactKDC)) {
    LOG(ERROR) << "kinit failed - failed to contact KDC";
    return ERROR_CONTACTING_KDC_FAILED;
  }
  if (internal::Contains(kinit_err, kKeyBadUserName)) {
    LOG(ERROR) << "kinit failed - bad user name";
    return ERROR_BAD_USER_NAME;
  }
  if (internal::Contains(kinit_err, kKeyBadPassword)) {
    LOG(ERROR) << "kinit failed - bad password";
    return ERROR_BAD_PASSWORD;
  }
  // Check both stderr and stdout here since any kinit error in the change-
  // password-workflow would otherwise be interpreted as 'password expired'.
  if (internal::Contains(kinit_out, kKeyPasswordExpiredStdout) &&
      internal::Contains(kinit_err, kKeyPasswordExpiredStderr)) {
    LOG(ERROR) << "kinit failed - password expired";
    return ERROR_PASSWORD_EXPIRED;
  }
  if (internal::Contains(kinit_err, kKeyCannotResolve)) {
    LOG(ERROR) << "kinit failed - cannot resolve KDC realm";
    return ERROR_NETWORK_PROBLEM;
  }
  LOG(ERROR) << "kinit failed with exit code " << kinit_cmd.GetExitCode();
  return ERROR_KINIT_FAILED;
}

}  // namespace

TgtManager::TgtManager(const PathService* path_service,
                       AuthPolicyMetrics* metrics,
                       const JailHelper* jail_helper,
                       Path config_path,
                       Path credential_cache_path)
    : paths_(path_service),
      metrics_(metrics),
      jail_helper_(jail_helper),
      config_path_(config_path),
      credential_cache_path_(credential_cache_path) {}

TgtManager::~TgtManager() {
  // Do a best-effort cleanup.
  base::DeleteFile(base::FilePath(paths_->Get(config_path_)),
                   false /* recursive */);
  base::DeleteFile(base::FilePath(paths_->Get(credential_cache_path_)),
                   false /* recursive */);
}

ErrorType TgtManager::AcquireTgtWithPassword(const std::string& principal,
                                             int password_fd,
                                             const std::string& realm,
                                             const std::string& kdc_ip) {
  realm_ = realm;
  kdc_ip_ = kdc_ip;

  // Duplicate password pipe in case we'll need to retry kinit.
  base::ScopedFD password_dup(DuplicatePipe(password_fd));
  if (!password_dup.is_valid())
    return ERROR_LOCAL_IO;

  ProcessExecutor kinit_cmd({paths_->Get(Path::KINIT), principal});
  kinit_cmd.SetInputFile(password_fd);
  ErrorType error = RunKinit(&kinit_cmd, false /* propagation_retry */);
  if (error != ERROR_CONTACTING_KDC_FAILED)
    return error;

  LOG(WARNING) << "Retrying kinit without KDC IP config in the krb5.conf";
  kdc_ip_.clear();
  kinit_cmd.SetInputFile(password_dup.get());
  return RunKinit(&kinit_cmd, false /* propagation_retry */);
}

ErrorType TgtManager::AcquireTgtWithKeytab(const std::string& principal,
                                           Path keytab_path,
                                           bool propagation_retry,
                                           const std::string& realm,
                                           const std::string& kdc_ip) {
  realm_ = realm;
  kdc_ip_ = kdc_ip;

  // Call kinit to get the Kerberos ticket-granting-ticket.
  ProcessExecutor kinit_cmd({paths_->Get(Path::KINIT), principal, "-k"});
  kinit_cmd.SetEnv(kKrb5KTEnvKey, kFilePrefix + paths_->Get(keytab_path));
  ErrorType error = RunKinit(&kinit_cmd, propagation_retry);
  if (error != ERROR_CONTACTING_KDC_FAILED)
    return error;

  LOG(WARNING) << "Retrying kinit without KDC IP config in the krb5.conf";
  kdc_ip_.clear();
  return RunKinit(&kinit_cmd, propagation_retry);
}

ErrorType TgtManager::RunKinit(ProcessExecutor* kinit_cmd,
                               bool propagation_retry) const {
  // Write configuration.
  ErrorType error = WriteKrb5Conf();
  if (error != ERROR_NONE)
    return error;

  // Set Kerberos credential cache and configuration file paths.
  kinit_cmd->SetEnv(kKrb5CCEnvKey, paths_->Get(credential_cache_path_));
  kinit_cmd->SetEnv(kKrb5ConfEnvKey, kFilePrefix + paths_->Get(config_path_));

  error = ERROR_NONE;
  const int max_tries = (propagation_retry ? kKinitMaxTries : 1);
  int tries, failed_tries = 0;
  for (tries = 1; tries <= max_tries; ++tries) {
    if (tries > 1) {
      base::PlatformThread::Sleep(
          base::TimeDelta::FromSeconds(kKinitRetryWaitSeconds));
    }
    SetupKinitTrace(kinit_cmd);
    if (jail_helper_->SetupJailAndRun(
            kinit_cmd, Path::KINIT_SECCOMP, TIMER_KINIT)) {
      error = ERROR_NONE;
      break;
    }
    failed_tries++;
    OutputKinitTrace();
    error = GetKinitError(*kinit_cmd);
    // If kinit fails because credentials are not propagated yet, these are
    // the error types you get.
    if (error != ERROR_BAD_USER_NAME && error != ERROR_BAD_PASSWORD)
      break;
  }
  metrics_->Report(METRIC_KINIT_FAILED_TRY_COUNT, failed_tries);
  return error;
}

ErrorType TgtManager::WriteKrb5Conf() const {
  std::string data = base::StringPrintf(kKrb5ConfData, realm_.c_str());
  if (!kdc_ip_.empty())
    data += base::StringPrintf(kKrb5RealmData, realm_.c_str(), kdc_ip_.c_str());
  const base::FilePath krbconf_path(paths_->Get(config_path_));
  const int data_size = static_cast<int>(data.size());
  if (base::WriteFile(krbconf_path, data.c_str(), data_size) != data_size) {
    LOG(ERROR) << "Failed to write krb5 conf file '" << krbconf_path.value()
               << "'";
    return ERROR_LOCAL_IO;
  }

  return ERROR_NONE;
}

void TgtManager::SetupKinitTrace(ProcessExecutor* kinit_cmd) const {
  if (!trace_kinit_)
    return;
  const std::string& trace_path = paths_->Get(Path::KRB5_TRACE);
  {
    // Delete kinit trace file (must be done as authpolicyd-exec).
    ScopedSwitchToSavedUid switch_scope;
    if (!base::DeleteFile(base::FilePath(trace_path), false /* recursive */)) {
      LOG(WARNING) << "Failed to delete kinit trace file";
    }
  }
  kinit_cmd->SetEnv(kKrb5TraceEnvKey, trace_path);
}

void TgtManager::OutputKinitTrace() const {
  if (!trace_kinit_)
    return;
  const std::string& trace_path = paths_->Get(Path::KRB5_TRACE);
  std::string trace;
  {
    // Read kinit trace file (must be done as authpolicyd-exec).
    ScopedSwitchToSavedUid switch_scope;
    base::ReadFileToString(base::FilePath(trace_path), &trace);
  }
  LOG(INFO) << "Kinit trace:\n" << trace;
}

}  // namespace authpolicy
