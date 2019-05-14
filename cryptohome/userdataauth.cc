// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/bind.h>
#include <base/strings/string_util.h>
#include <base/threading/thread_task_runner_handle.h>
#include <chaps/isolate.h>
#include <chaps/token_manager_client.h>
#include <vector>

#include "cryptohome/cryptohome_metrics.h"
#include "cryptohome/tpm.h"
#include "cryptohome/user_oldest_activity_timestamp_cache.h"
#include "cryptohome/userdataauth.h"

using base::FilePath;
using brillo::SecureBlob;

namespace cryptohome {

const char kMountThreadName[] = "MountThread";

UserDataAuth::UserDataAuth()
    : origin_thread_id_(base::PlatformThread::CurrentId()),
      mount_thread_(kMountThreadName),
      disable_threading_(false),
      system_salt_(),
      default_platform_(new Platform()),
      platform_(default_platform_.get()),
      default_crypto_(new Crypto(platform_)),
      crypto_(default_crypto_.get()),
      default_chaps_client_(new chaps::TokenManagerClient()),
      chaps_client_(default_chaps_client_.get()),
      reported_pkcs11_init_fail_(false),
      default_homedirs_(new cryptohome::HomeDirs()),
      homedirs_(default_homedirs_.get()),
      user_timestamp_cache_(new UserOldestActivityTimestampCache()),
      force_ecryptfs_(true),
      legacy_mount_(true) {}

UserDataAuth::~UserDataAuth() {
  mount_thread_.Stop();
}

bool UserDataAuth::Initialize() {
  AssertOnOriginThread();

  if (!disable_threading_) {
    // Note that |origin_task_runner_| is initialized here because in some cases
    // such as unit testing, the current thread Task Runner might not be
    // available, so we should not attempt to retrieve the current thread task
    // runner during the creation of this class.
    origin_task_runner_ = base::ThreadTaskRunnerHandle::Get();
  }

  // Note that we check to see if |tpm_| is available here because it may have
  // been set to an overridden value during unit testing before Initialize() is
  // called.
  if (!tpm_) {
    tpm_ = Tpm::GetSingleton();
  }

  // Note that we check to see if |tpm_init_| is available here because it may
  // have been set to an overridden value during unit testing before
  // Initialize() is called.
  if (!tpm_init_) {
    default_tpm_init_.reset(new TpmInit(tpm_, platform_));
    tpm_init_ = default_tpm_init_.get();
  }

  crypto_->set_use_tpm(true);
  if (!crypto_->Init(tpm_init_)) {
    return false;
  }

  if (!homedirs_->Init(platform_, crypto_, user_timestamp_cache_.get())) {
    return false;
  }

  if (!homedirs_->GetSystemSalt(&system_salt_)) {
    return false;
  }

  if (!disable_threading_) {
    base::Thread::Options options;
    options.message_loop_type = base::MessageLoop::TYPE_IO;
    mount_thread_.StartWithOptions(options);
  }

  // Clean up any unreferenced mountpoints at startup.
  PostTaskToMountThread(FROM_HERE,
                        base::BindOnce(
                            [](UserDataAuth* userdataauth) {
                              userdataauth->CleanUpStaleMounts(false);
                            },
                            base::Unretained(this)));

  // We expect |tpm_| and |tpm_init_| to be available by this point.
  DCHECK(tpm_ && tpm_init_);

  tpm_init_->Init(
      base::Bind(&UserDataAuth::OwnershipCallback, base::Unretained(this)));

  return true;
}

bool UserDataAuth::PostTaskToOriginThread(
    const base::Location& from_here, base::OnceClosure task) {
  if (disable_threading_) {
    std::move(task).Run();
    return true;
  }
  return origin_task_runner_->PostTask(from_here, std::move(task));
}

bool UserDataAuth::PostTaskToMountThread(
    const base::Location& from_here, base::OnceClosure task) {
  if (disable_threading_) {
    std::move(task).Run();
    return true;
  }
  return mount_thread_.task_runner()->PostTask(from_here, std::move(task));
}

bool UserDataAuth::IsMounted(const std::string& username,
                             bool* is_ephemeral_out) {
  // Note: This can only run in mount_thread_
  AssertOnMountThread();

  bool is_mounted = false;
  bool is_ephemeral = false;
  if (username.empty()) {
    // No username is specified, so we consider "the cryptohome" to be mounted
    // if any existing cryptohome is mounted.
    for (const auto& mount_pair : mounts_) {
      if (mount_pair.second->IsMounted()) {
        is_mounted = true;
        is_ephemeral |= !mount_pair.second->IsNonEphemeralMounted();
      }
    }
  } else {
    // A username is specified, check the associated mount object.
    scoped_refptr<cryptohome::Mount> mount = GetMountForUser(username);

    if (mount.get()) {
      is_mounted = mount->IsMounted();
      is_ephemeral = is_mounted && !mount->IsNonEphemeralMounted();
    }
  }

  if (is_ephemeral_out) {
    *is_ephemeral_out = is_ephemeral;
  }

  return is_mounted;
}

scoped_refptr<cryptohome::Mount> UserDataAuth::GetMountForUser(
    const std::string& username) {
  // Note: This can only run in mount_thread_
  AssertOnMountThread();

  scoped_refptr<cryptohome::Mount> mount = nullptr;
  if (mounts_.count(username) == 1) {
    mount = mounts_[username];
  }
  return mount;
}

bool UserDataAuth::RemoveAllMounts(bool unmount) {
  AssertOnMountThread();

  bool success = true;
  for (auto it = mounts_.begin(); it != mounts_.end();) {
    scoped_refptr<cryptohome::Mount> mount = it->second;
    if (unmount && mount->IsMounted()) {
      if (mount->pkcs11_state() == cryptohome::Mount::kIsBeingInitialized) {
        // Reset the state.
        mount->set_pkcs11_state(cryptohome::Mount::kUninitialized);
        // And also reset the global failure reported state.
        reported_pkcs11_init_fail_ = false;
      }
      success = success && mount->UnmountCryptohome();
    }
    mounts_.erase(it++);
  }
  return success;
}

bool UserDataAuth::FilterActiveMounts(
    std::multimap<const FilePath, const FilePath>* mounts,
    std::multimap<const FilePath, const FilePath>* active_mounts,
    bool force) {
  // Note: This can only run in mount_thread_
  AssertOnMountThread();

  bool skipped = false;
  for (auto match = mounts->begin(); match != mounts->end();) {
    // curr->first is the source device of the group that we are processing in
    // this outer loop.
    auto curr = match;
    bool keep = false;

    // Note that we organize the set of mounts with the same source, then
    // process them together. That is, say there's /dev/mmcblk0p1 mounted on
    // /home/user/xxx and /home/chronos/u-xxx/MyFiles/Downloads. They are both
    // from the same source (/dev/mmcblk0p1, or match->first). In this case,
    // we'll decide the fate of all mounts with the same source together. For
    // each such group, the outer loop will run once. The inner loop will
    // iterate through every mount in the group with |match| variable, looking
    // to see if it's owned by any active mounts. If it is, the entire group is
    // kept. Otherwise, (and assuming no open files), the entire group is
    // discarded, as in, not moved into the active_mounts multimap.

    // Walk each set of sources as one group since multimaps are key ordered.
    for (; match != mounts->end() && match->first == curr->first; ++match) {
      // Ignore known mounts.
      for (const auto& mount_pair : mounts_) {
        if (mount_pair.second->OwnsMountPoint(match->second)) {
          keep = true;
          break;
        }
      }

      // Optionally, ignore mounts with open files.
      if (!force) {
        std::vector<ProcessInformation> processes;
        platform_->GetProcessesWithOpenFiles(match->second, &processes);
        if (processes.size()) {
          LOG(WARNING) << "Stale mount " << match->second.value() << " from "
                       << match->first.value() << " has active holders.";
          keep = true;
          skipped = true;
        }
      }
    }
    // Delete anything that shouldn't be unmounted.
    if (keep) {
      active_mounts->insert(curr, match);
      mounts->erase(curr, match);
    }
  }
  return skipped;
}

void UserDataAuth::GetEphemeralLoopDevicesMounts(
    std::multimap<const FilePath, const FilePath>* mounts) {
  std::multimap<const FilePath, const FilePath> loop_mounts;
  platform_->GetLoopDeviceMounts(&loop_mounts);

  const FilePath sparse_path =
      FilePath(kEphemeralCryptohomeDir).Append(kSparseFileDir);
  for (const auto& device : platform_->GetAttachedLoopDevices()) {
    // Ephemeral mounts are mounts from a loop device with ephemeral sparse
    // backing file.
    if (sparse_path.IsParent(device.backing_file)) {
      auto range = loop_mounts.equal_range(device.device);
      mounts->insert(range.first, range.second);
    }
  }
}

bool UserDataAuth::UnloadPkcs11Tokens(const std::vector<FilePath>& exclude) {
  SecureBlob isolate =
      chaps::IsolateCredentialManager::GetDefaultIsolateCredential();
  std::vector<std::string> tokens;
  if (!chaps_client_->GetTokenList(isolate, &tokens))
    return false;
  for (size_t i = 0; i < tokens.size(); ++i) {
    if (tokens[i] != chaps::kSystemTokenPath &&
        !PrefixPresent(exclude, tokens[i])) {
      // It's not a system token and is not under one of the excluded path.
      LOG(INFO) << "Unloading up PKCS #11 token: " << tokens[i];
      chaps_client_->UnloadToken(isolate, FilePath(tokens[i]));
    }
  }
  return true;
}

bool UserDataAuth::CleanUpStaleMounts(bool force) {
  // This function is meant to aid in a clean recovery from a crashed or
  // manually restarted cryptohomed.  Cryptohomed may restart:
  // 1. Before any mounts occur
  // 2. While mounts are active
  // 3. During an unmount
  // In case #1, there should be no special work to be done.
  // The best way to disambiguate #2 and #3 is to determine if there are
  // any active open files on any stale mounts.  If there are open files,
  // then we've likely(*) resumed an active session. If there are not,
  // the last cryptohome should have been unmounted.
  // It's worth noting that a restart during active use doesn't impair
  // other user session behavior, like CheckKey, because it doesn't rely
  // exclusively on mount state.
  //
  // In the future, it may make sense to attempt to keep the MountMap
  // persisted to disk which would make resumption much easier.
  //
  // (*) Relies on the expectation that all processes have been killed off.

  // Stale shadow and ephemeral mounts.
  std::multimap<const FilePath, const FilePath> shadow_mounts;
  std::multimap<const FilePath, const FilePath> ephemeral_mounts;

  // Active mounts that we don't intend to unmount.
  std::multimap<const FilePath, const FilePath> active_mounts;

  // Retrieve all the mounts that's currently mounted by the kernel and concerns
  // us
  platform_->GetMountsBySourcePrefix(homedirs_->shadow_root(), &shadow_mounts);
  GetEphemeralLoopDevicesMounts(&ephemeral_mounts);

  // Remove mounts that we've a record of or have open files on them
  bool skipped = FilterActiveMounts(&shadow_mounts, &active_mounts, force) ||
                 FilterActiveMounts(&ephemeral_mounts, &active_mounts, force);

  // Unload PKCS#11 tokens on any mount that we're going to unmount.
  std::vector<FilePath> excluded_mount_points;
  for (const auto& mount : active_mounts) {
    excluded_mount_points.push_back(mount.second);
  }
  UnloadPkcs11Tokens(excluded_mount_points);

  // Unmount anything left.
  for (const auto& match : shadow_mounts) {
    LOG(WARNING) << "Lazily unmounting stale shadow mount: "
                 << match.second.value() << " from " << match.first.value();
    // true for lazy unmount, nullptr for us not needing to know if it's really
    // unmounted.
    platform_->Unmount(match.second, true, nullptr);
  }
  for (const auto& match : ephemeral_mounts) {
    LOG(WARNING) << "Lazily unmounting stale ephemeral mount: "
                 << match.second.value() << " from " << match.first.value();
    // true for lazy unmount, nullptr for us not needing to know if it's really
    // unmounted.
    platform_->Unmount(match.second, true, nullptr);
    // Clean up destination directory for ephemeral mounts under ephemeral
    // cryptohome dir.
    if (base::StartsWith(match.first.value(), kLoopPrefix,
                         base::CompareCase::SENSITIVE) &&
        FilePath(kEphemeralCryptohomeDir).IsParent(match.second)) {
      platform_->DeleteFile(match.second, true /* recursive */);
    }
  }

  // Clean up all stale sparse files, this is comprised of two stages:
  // 1. Clean up stale loop devices.
  // 2. Clean up stale sparse files.
  // Note that some mounts are backed by loop devices, and loop devices are
  // backed by sparse files.

  std::vector<Platform::LoopDevice> loop_devices =
      platform_->GetAttachedLoopDevices();
  const FilePath sparse_dir =
      FilePath(kEphemeralCryptohomeDir).Append(kSparseFileDir);
  std::vector<FilePath> stale_sparse_files;
  platform_->EnumerateDirectoryEntries(sparse_dir, false /* is_recursive */,
                                       &stale_sparse_files);

  // We'll go through all loop devices, and for every of them, we'll see if we
  // can remove it. Also in the process, we'll get to keep track of which sparse
  // files are actually used by active loop devices.
  for (const auto& device : loop_devices) {
    // Check whether the loop device is created from an ephemeral sparse file.
    if (!sparse_dir.IsParent(device.backing_file)) {
      // Nah, it's this loop device is not backed by an ephemeral sparse file
      // created by cryptohome, so we'll leave it alone.
      continue;
    }

    // Check if any of our active mounts are backed by this loop device.
    if (active_mounts.count(device.device) == 0) {
      // Nope, this loop device have nothing to do with our active mounts.
      LOG(WARNING) << "Detaching stale loop device: " << device.device.value();
      if (!platform_->DetachLoop(device.device)) {
        ReportCryptohomeError(kEphemeralCleanUpFailed);
        PLOG(ERROR) << "Can't detach stale loop: " << device.device.value();
      }
    } else {
      // This loop device backs one of our active_mounts, so we can't count it
      // as stale. Thus removing from the stale_sparse_files list.
      stale_sparse_files.erase(
          std::remove(stale_sparse_files.begin(), stale_sparse_files.end(),
                      device.backing_file),
          stale_sparse_files.end());
    }
  }

  // Now we clean up the stale sparse files.
  for (const auto& file : stale_sparse_files) {
    LOG(WARNING) << "Deleting stale ephemeral backing sparse file: "
                 << file.value();
    if (!platform_->DeleteFile(file, false /* recursive */)) {
      ReportCryptohomeError(kEphemeralCleanUpFailed);
      PLOG(ERROR) << "Failed to clean up ephemeral sparse file: "
                  << file.value();
    }
  }

  // |force| and |skipped| cannot be true at the same time. If |force| is true,
  // then we'll not skip over any stale mount because there are open files, so
  // |skipped| must be false.
  DCHECK(!(force && skipped));

  return skipped;
}

bool UserDataAuth::PrefixPresent(const std::vector<FilePath>& prefixes,
                                 const std::string path) {
  return std::any_of(
      prefixes.begin(), prefixes.end(), [&path](const FilePath& prefix) {
        return base::StartsWith(path, prefix.value(),
                                base::CompareCase::INSENSITIVE_ASCII);
      });
}

bool UserDataAuth::Unmount() {
  bool unmount_ok = RemoveAllMounts(true);

  // If there are any unexpected mounts lingering from a crash/restart,
  // clean them up now.
  // Note that we do not care about the return value of CleanUpStaleMounts()
  // because it doesn't matter if any mount is skipped due to open files, and
  // additionally, since we've specified force=true, it'll not skip over mounts
  // with open files.
  CleanUpStaleMounts(true);

  return unmount_ok;
}

void UserDataAuth::InitializePkcs11(cryptohome::Mount* mount) {
  // We should not pass nullptr to this method.
  DCHECK(mount);

  if (!IsOnMountThread()) {
    // We are not on mount thread, but to be safe, we'll only access Mount
    // objects on mount thread, so let's post ourself there.
    PostTaskToMountThread(
        FROM_HERE,
        base::BindOnce(&UserDataAuth::InitializePkcs11, base::Unretained(this),
                       base::Unretained(mount)));
    return;
  }

  AssertOnMountThread();

  // Wait for ownership if there is a working TPM.
  if (tpm_ && tpm_->IsEnabled() && !tpm_->IsOwned()) {
    LOG(WARNING) << "TPM was not owned. TPM initialization call back will"
                 << " handle PKCS#11 initialization.";
    mount->set_pkcs11_state(cryptohome::Mount::kIsWaitingOnTPM);
    return;
  }

  bool still_mounted = false;

  // The mount have be mounted, that is, still tracked by cryptohome. Otherwise
  // there's no point in initializing PKCS#11 for it. The reason for this check
  // is because it might be possible for Unmount() to be called after mounting
  // and before getting here.
  for (const auto& mount_pair : mounts_) {
    if (mount_pair.second.get() == mount && mount->IsMounted()) {
      still_mounted = true;
      break;
    }
  }

  if (!still_mounted) {
    LOG(WARNING)
        << "PKCS#11 initialization requested but cryptohome is not mounted.";
    return;
  }

  mount->set_pkcs11_state(cryptohome::Mount::kIsBeingInitialized);

  // Note that the timer stops in the Mount class' method.
  ReportTimerStart(kPkcs11InitTimer);

  mount->InsertPkcs11Token();

  LOG(INFO) << "PKCS#11 initialization succeeded.";

  mount->set_pkcs11_state(cryptohome::Mount::kIsInitialized);
}

void UserDataAuth::ResumeAllPkcs11Initialization() {
  if (!IsOnMountThread()) {
    // We are not on mount thread, but to be safe, we'll only access Mount
    // objects on mount thread, so let's post ourself there.
    PostTaskToMountThread(
        FROM_HERE, base::BindOnce(&UserDataAuth::ResumeAllPkcs11Initialization,
                                  base::Unretained(this)));
    return;
  }

  for (const auto& mount_pair : mounts_) {
    cryptohome::Mount* mount = mount_pair.second.get();
    if (mount->pkcs11_state() == cryptohome::Mount::kIsWaitingOnTPM) {
      InitializePkcs11(mount);
    }
  }
}

void UserDataAuth::ResetAllTPMContext() {
  if (!IsOnMountThread()) {
    // We are not on mount thread, but to be safe, we'll only access Mount
    // objects on mount thread, so let's post ourself there.
    PostTaskToMountThread(FROM_HERE,
                          base::BindOnce(&UserDataAuth::ResetAllTPMContext,
                                         base::Unretained(this)));
    return;
  }

  for (const auto& mount_pair : mounts_) {
    if (mount_pair.second) {
      Crypto* crypto = mount_pair.second->crypto();
      if (crypto) {
        crypto->EnsureTpm(true);
      }
    }
  }
}

void UserDataAuth::OwnershipCallback(bool status, bool took_ownership) {
  if (took_ownership) {
    // Reset the TPM context of all mounts, that is, force a reload of
    // cryptohome keys, and make sure it is loaded and ready for every mount.
    PostTaskToMountThread(FROM_HERE,
                          base::BindOnce(&UserDataAuth::ResetAllTPMContext,
                                         base::Unretained(this)));

    // There might be some mounts that is half way through the PKCS#11
    // initialization, let's resume them.
    PostTaskToMountThread(
        FROM_HERE, base::BindOnce(&UserDataAuth::ResumeAllPkcs11Initialization,
                                  base::Unretained(this)));
  }
}

}  // namespace cryptohome
