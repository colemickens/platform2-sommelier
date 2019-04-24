// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTOHOME_USERDATAAUTH_H_
#define CRYPTOHOME_USERDATAAUTH_H_

#include <map>
#include <memory>
#include <string>
#include <utility>

#include <base/threading/thread.h>
#include <brillo/secure_blob.h>

#include "cryptohome/crypto.h"
#include "cryptohome/homedirs.h"
#include "cryptohome/mount.h"
#include "cryptohome/platform.h"

namespace cryptohome {

class UserDataAuth {
 public:
  UserDataAuth();
  ~UserDataAuth();

  bool Initialize();

 private:
  // Note: In Service class (the class that this class is refactored from),
  // there is a use_tpm_ member variable, but it is almost unused and always set
  // to true there, so in this class, if we are migrating any code from Service
  // class and use_tpm_ is used there, then we'll just assume it's true and not
  // have a use_tpm_ variable here.
  // The same is true for initialize_tpm_ variable, it is assumed to be true.

  // =============== Threading Related Variables ===============

  // The task runner that belongs to the thread that created this UserDataAuth
  // object. Currently, this is required to be the same as the dbus thread's
  // task runner.
  scoped_refptr<base::SingleThreadTaskRunner> origin_task_runner_;

  // The thread ID of the thread that created this UserDataAuth object.
  // Currently, this is required to be th esame as the dbus thread's task
  // runner.
  base::PlatformThreadId origin_thread_id_;

  // The thread for performing long running, or mount related operations
  base::Thread mount_thread_;

  // =============== Basic Utilities Related Variables ===============
  // The system salt that is used for obfuscating the username
  brillo::SecureBlob system_salt_;

  // The object for accessing the TPM
  // Note that TPM doesn't use the unique_ptr for default pattern, since the tpm
  // is a singleton - we don't want it getting destroyed when we are.
  Tpm* tpm_;

  // The default TPM init object.
  std::unique_ptr<TpmInit> default_tpm_init_;

  // The TPM init object. Note that |tpm_init_| and |default_tpm_init_| will be
  // removed at the end of the refactoring that's happening in cryptohome
  // (b/123679223).
  TpmInit* tpm_init_;

  // The default platform object for accessing platform related functionalities
  std::unique_ptr<cryptohome::Platform> default_platform_;

  // The actual platform object used by this class, usually set to
  // default_platform_, but can be overridden for testing
  cryptohome::Platform* platform_;

  // The default crypto object for performing cryptographic operations
  std::unique_ptr<cryptohome::Crypto> default_crypto_;

  // The actual crypto object used by this class, usually set to
  // default_crypto_, but can be overridden for testing
  cryptohome::Crypto* crypto_;

  // =============== Mount Related Variables ===============

  // Defines a type for tracking Mount objects for each user by username.
  typedef std::map<const std::string, scoped_refptr<cryptohome::Mount>>
      MountMap;

  // Records the Mount objects associated with each username.
  // This and its content should only be accessed from the mount thread.
  MountMap mounts_;

  // Note: In Service class (the class that this class is refactored from),
  // there is a mounts_lock_ lock for inserting/removal of mounts_ map. However,
  // in this class, all accesses to mounts_ should happen on the mount thread,
  // so no lock is needed.

  // The homedirs_ object in normal operation
  std::unique_ptr<HomeDirs> default_homedirs_;

  // This holds the object that records informations about the homedirs.
  // This is usually set to default_homedirs_, but can be overridden for
  // testing
  HomeDirs* homedirs_;

  // This holds a timestamp for each user that is the time that the user was
  // active.
  std::unique_ptr<UserOldestActivityTimestampCache> user_timestamp_cache_;
};

}  // namespace cryptohome

#endif  // CRYPTOHOME_USERDATAAUTH_H_
