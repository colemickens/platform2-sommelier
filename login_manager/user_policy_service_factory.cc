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

#include "chromeos/cryptohome.h"

#include "login_manager/owner_key.h"
#include "login_manager/policy_store.h"
#include "login_manager/user_policy_service.h"

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

}  // namespace

UserPolicyServiceFactory::UserPolicyServiceFactory(
    uid_t uid,
    const scoped_refptr<base::MessageLoopProxy>& main_loop)
    : uid_(uid),
      main_loop_(main_loop) {
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

  return new UserPolicyService(
      new PolicyStore(policy_dir.Append(kPolicyDataFile)),
      new OwnerKey(policy_dir.Append(kPolicyKeyFile)),
      main_loop_);
}

}  // namespace login_manager
