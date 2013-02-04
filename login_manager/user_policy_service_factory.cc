// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/user_policy_service_factory.h"

#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <base/file_path.h>
#include <base/file_util.h>
#include <base/logging.h>
#include <base/message_loop_proxy.h>
#include <base/stringprintf.h>

#include "chromeos/cryptohome.h"

#include "login_manager/policy_key.h"
#include "login_manager/policy_store.h"
#include "login_manager/user_policy_service.h"
#include "login_manager/system_utils.h"

namespace em = enterprise_management;

namespace login_manager {

namespace {

// Daemon name we use for storing per-user data on the file system.
const char kDaemonName[] = "session_manager";
// Name of the subdirectory to store policy in.
const FilePath::CharType kPolicyDir[] = FILE_PATH_LITERAL("policy");
// The policy protobuffer blob is written to this file.
const FilePath::CharType kPolicyDataFile[] = FILE_PATH_LITERAL("policy");
// Holds the public key for policy signing.
const FilePath::CharType kPolicyKeyFile[] = FILE_PATH_LITERAL("key");

// Directory that contains the public keys for user policy verification.
// These keys are duplicates from the key contained in the vault, so that the
// chrome process can read them; the authoritative version of the key is still
// the vault's.
const FilePath::CharType kPolicyKeyCopyDir[] =
    FILE_PATH_LITERAL("/var/run/user_policy");
// Name of the policy key files.
const FilePath::CharType kPolicyKeyCopyFile[] = FILE_PATH_LITERAL("policy.pub");

}  // namespace

UserPolicyServiceFactory::UserPolicyServiceFactory(
    uid_t uid,
    const scoped_refptr<base::MessageLoopProxy>& main_loop,
    SystemUtils* system_utils)
    : uid_(uid),
      main_loop_(main_loop),
      system_utils_(system_utils) {
}

UserPolicyServiceFactory::~UserPolicyServiceFactory() {
}

PolicyService* UserPolicyServiceFactory::Create(const std::string& username) {
  using chromeos::cryptohome::home::GetDaemonPath;
  FilePath policy_dir(GetDaemonPath(username, kDaemonName).Append(kPolicyDir));
  if (!file_util::CreateDirectory(policy_dir)) {
    PLOG(ERROR) << "Failed to create user policy directory.";
    return NULL;
  }

  scoped_ptr<PolicyKey> key(
      new PolicyKey(policy_dir.Append(kPolicyKeyFile)));
  bool key_load_success = key->PopulateFromDiskIfPossible();
  if (!key_load_success) {
    LOG(ERROR) << "Failed to load user policy key from disk.";
    return NULL;
  }

  scoped_ptr<PolicyStore> store(
      new PolicyStore(policy_dir.Append(kPolicyDataFile)));
  bool policy_success = store->LoadOrCreate();
  if (!policy_success)  // Non-fatal, so log, and keep going.
    LOG(WARNING) << "Failed to load user policy data, continuing anyway.";

  using chromeos::cryptohome::home::SanitizeUserName;
  const std::string sanitized(SanitizeUserName(username));
  const FilePath key_copy_file(base::StringPrintf("%s/%s/%s",
                                                  kPolicyKeyCopyDir,
                                                  sanitized.c_str(),
                                                  kPolicyKeyCopyFile));

  UserPolicyService* service = new UserPolicyService(
      store.Pass(), key.Pass(), key_copy_file, main_loop_, system_utils_);
  service->PersistKeyCopy();
  return service;
}

}  // namespace login_manager
