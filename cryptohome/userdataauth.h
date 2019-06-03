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

  // Note that this function must be called from the thread that created this
  // object, so that |origin_task_runner_| is initialized correctly.
  bool Initialize();

  // =============== Mount Related Public API ===============

  // If username is empty, returns true if any mount is mounted, otherwise,
  // returns true if the mount associated with the given |username| is mounted.
  // For |is_ephemeral_out|, if no username is given, then is_ephemeral_out is
  // set to true when any mount is ephemeral. Otherwise, is_ephemeral_out is set
  // to true when the mount associated with the given |username| is mounted in
  // an ephemeral manner. If nullptr is passed in for is_ephemeral_out, then it
  // won't be touched. Ephemeral mount means that the content of the mount is
  // cleared once the user logs out.
  bool IsMounted(const std::string& username = "",
                 bool* is_ephemeral_out = nullptr);

  // Returns true if the mount that corresponds to the username is mounted,
  // false otherwise. If that mount is ephemeral, then is_ephemeral_out is set
  // to true, false otherwise. If nullptr is passed in for is_ephemeral_out,
  // then it won't be touched. Ephemeral mount means that the content of the
  // mount is cleared once the user logs out.
  bool IsMountedForUser(const std::string& username,
                        bool* is_ephemeral_out = nullptr);

  // ================= Threading Utilities ==================

  // Returns true if we are currently running on the origin thread
  bool IsOnOriginThread() {
    // Note that this function should not rely on |origin_task_runner_| because
    // it may be unavailable when this function is first called by
    // UserDataAuth::Initialize()
    return disable_threading_ ||
           base::PlatformThread::CurrentId() == origin_thread_id_;
  }

  // Returns true if we are currently running on the mount thread
  bool IsOnMountThread() {
    // GetThreadId blocks if the thread is not started yet.
    return disable_threading_ ||
           (mount_thread_.IsRunning() &&
            (base::PlatformThread::CurrentId() == mount_thread_.GetThreadId()));
  }

  // DCHECK if we are running on the origin thread. Will have no effect
  // in production.
  void AssertOnOriginThread() { DCHECK(IsOnOriginThread()); }

  // DCHECK if we are running on the mount thread. Will have no effect
  // in production.
  void AssertOnMountThread() { DCHECK(IsOnMountThread()); }

  // Post Task to origin thread. For the caller, from_here is usually FROM_HERE
  // macro, while task is a callback function to be posted. Will return true if
  // the task may be run sometime in the future, false if it will definitely not
  // run.
  bool PostTaskToOriginThread(const tracked_objects::Location& from_here,
                              base::OnceClosure task);

  // Post Task to mount thread. For the caller, from_here is usually FROM_HERE
  // macro, while task is a callback function to be posted. Will return true if
  // the task may be run sometime in the future, false if it will definitely not
  // run.
  bool PostTaskToMountThread(const tracked_objects::Location& from_here,
                             base::OnceClosure task);

  // ================= Testing Utilities ==================
  // Note that all functions below in this section should only be used for unit
  // testing purpose only.

  // Override |crypto_| for testing purpose
  void set_crypto(cryptohome::Crypto* crypto) { crypto_ = crypto; }

  // Override |homedirs_| for testing purpose
  void set_homedirs(cryptohome::HomeDirs* homedirs) { homedirs_ = homedirs; }

  // Override |tpm_| for testing purpose
  void set_tpm(Tpm* tpm) { tpm_ = tpm; }

  // Override |tpm_init_| for testing purpose
  void set_tpm_init(TpmInit* tpm_init) { tpm_init_ = tpm_init; }

  // Override |platform_| for testing purpose
  void set_platform(cryptohome::Platform* platform) { platform_ = platform; }

  // Associate a particular mount object |mount| with the username |username|
  // for testing purpose
  void set_mount_for_user(const std::string& username,
                          cryptohome::Mount* mount) {
    mounts_[username] = mount;
  }

  // Set |disable_threading_|, so that we can disable threading in this class
  // for testing purpose. See comment of |disable_threading_| for more
  // information.
  void set_disable_threading(bool disable_threading) {
    disable_threading_ = disable_threading;
  }

 private:
  // Note: In Service class (the class that this class is refactored from),
  // there is a use_tpm_ member variable, but it is almost unused and always set
  // to true there, so in this class, if we are migrating any code from Service
  // class and use_tpm_ is used there, then we'll just assume it's true and not
  // have a use_tpm_ variable here.
  // The same is true for initialize_tpm_ variable, it is assumed to be true.

  // =============== Mount Related Utilities ===============
  // Returns the mount object associated with the given username
  scoped_refptr<cryptohome::Mount> GetMountForUser(const std::string& username);

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

  // This variable is used only for unit testing purpose. If set to true, it'll
  // disable the the threading mechanism in this class so that testing doesn't
  // fail. When threading is disabled, posting to origin or mount thread will
  // execute immediately, and all checks for whether we are on mount or origin
  // thread will result in true.
  bool disable_threading_;

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
