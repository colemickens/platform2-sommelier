// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Mount - class for managing cryptohome user keys and mounts.  In Chrome OS,
// users are managed on top of a shared unix user, chronos.  When a user logs
// in, cryptohome mounts their encrypted home directory to /home/chronos/user,
// and Chrome does a profile switch to that directory.  All user data in their
// home directory is transparently encrypted, providing protection against
// offline theft.  On logout, the mount point is removed.

#ifndef CRYPTOHOME_MOUNT_H_
#define CRYPTOHOME_MOUNT_H_

#include <base/basictypes.h>
#include <base/callback.h>
#include <base/file_path.h>
#include <base/memory/scoped_ptr.h>
#include <base/time.h>
#include <base/values.h>
#include <chaps/login_event_client.h>
#include <chromeos/secure_blob.h>
#include <chromeos/dbus/service_constants.h>
#include <gtest/gtest.h>
#include <policy/device_policy.h>
#include <policy/libpolicy.h>

#include "credentials.h"
#include "crypto.h"
#include "homedirs.h"
#include "mount_stack.h"
#include "platform.h"
#include "user_session.h"
#include "user_oldest_activity_timestamp_cache.h"
#include "vault_keyset.h"
#include "vault_keyset.pb.h"

namespace cryptohome {

// The directory in which to create the user's PKCS #11 token database.
extern const char kChapsTokenDir[];
// The path to the PKCS #11 token salt file.
extern const char kTokenSaltFile[];
// Directories that we intend to track (make pass-through in cryptohome vault)
extern const char kCacheDir[];
extern const char kDownloadsDir[];
extern const char kGCacheDir[];
extern const char kGCacheVersionDir[];  // subdir of kGCacheDir
extern const char kGCacheTmpDir[];      // subdir of kGCacheVersionDir
// Name of the vault directory.
extern const char kVaultDir[];
extern const char kUserHomeSuffix[];
extern const char kRootHomeSuffix[];
// Name of the mount directory.
extern const char kMountDir[];
// Name of the key file.
extern const char kKeyFile[];
// File system type for ephemeral mounts.
extern const char kEphemeralMountType[];
// Time delta of last user's activity to be considered as old.
extern const base::TimeDelta kOldUserLastActivityTime;

// The Mount class handles mounting/unmounting of the user's cryptohome
// directory as well as offline verification of the user's credentials against
// the directory's crypto key.
class Mount {
 public:
  struct MountArgs {
    bool create_if_missing;

    MountArgs() : create_if_missing(false) {
    }

    void CopyFrom(const MountArgs& rhs) {
      this->create_if_missing = rhs.create_if_missing;
    }
  };

  // Sets up Mount with the default locations, username, etc., as defined above.
  Mount();

  virtual ~Mount();

  // Gets the uid/gid of the default user and loads the system salt
  virtual bool Init();

  // Attempts to mount the cryptohome for the given credentials
  //
  // Parameters
  //   credentials - The Credentials representing the user
  //   mount_args - The options for the call to mount: whether to create the
  //                cryptohome if it doesn't exist and any tracked directories
  //                to create
  //   error - The specific error condition on failure
  virtual bool MountCryptohome(const Credentials& credentials,
                               const MountArgs& mount_args,
                               MountError* error);

  // Unmounts any mount at the cryptohome mount point
  virtual bool UnmountCryptohome();

  // Checks if the mount point currently has a mount
  virtual bool IsCryptohomeMounted() const;

  // Checks whether the mount point currently has a cryptohome mounted for the
  // specified credentials.
  //
  // Parameters
  //   credentials - The credentials for which to test the mount point.
  virtual bool IsCryptohomeMountedForUser(const Credentials& credentials) const;

  // Checks whether the mount point currently has a cryptohome mounted for the
  // specified credentials that is backed by a vault.
  //
  // Parameters
  //   credentials - The credentials for which to test the mount point.
  // TODO(ellyjones): factor this into IsCryptohomeMountedForUser() with an
  // |*ephemeral| arg or something.
  virtual bool IsVaultMountedForUser(const Credentials& credentials) const;

  // Checks if the cryptohome vault exists for the given credentials and creates
  // it if not (calls CreateCryptohome).
  //
  // Parameters
  //   credentials - The Credentials representing the user whose cryptohome
  //     should be ensured.
  //   created (OUT) - Whether the cryptohome was created
  virtual bool EnsureCryptohome(const Credentials& credentials,
                                bool* created) const;

  // Checks free disk space and if it falls below minimum
  // (kMinFreeSpace), performs cleanup. Returns true if cleanup
  // started (but maybe could not do anything), false if disk space
  // was enough.
  // TODO(ellyjones): delete this in favor of HomeDirs
  virtual bool DoAutomaticFreeDiskSpaceControl();

  // Updates current user activity timestamp. This is called daily.
  // So we may not consider current user as old (and delete it soon after she
  // logs off). Returns true if current user is in and updated.
  // Nothing is done if no user is logged in and false is returned.
  //
  // Parameters
  //   time_shift_sec - normally must be 0. Shifts the updated time backwards
  //                    by specified number of seconds. Used in manual tests.
  virtual bool UpdateCurrentUserActivityTimestamp(int time_shift_sec);

  // Returns whether the supplied Credentials claims to be for the same user
  // this Mount is for.
  //
  // Parameters
  //   credentials - The credentials to check
  virtual bool AreSameUser(const Credentials& credentials);

  // Tests if the given credentials would decrypt the user's cryptohome key
  //
  // Parameters
  //   credentials - The Credentials to attempt to decrypt the key with
  virtual bool AreValid(const Credentials& credentials);

  // Mounts a guest home directory to the cryptohome mount point
  virtual bool MountGuestCryptohome();

  // Used to override the default shadow root
  void set_shadow_root(const std::string& value) {
    shadow_root_ = value;
  }

  // Used to override the default skeleton directory
  void set_skel_source(const std::string& value) {
    skel_source_ = value;
  }

  // Used to override the default Crypto handler (does not take ownership)
  void set_crypto(Crypto* value) {
    crypto_ = value;
  }

  // Get the Crypto instance
  virtual Crypto* get_crypto() {
    return crypto_;
  }

  // Used to override the default Platform handler (does not take ownership)
  void set_platform(Platform* value) {
    platform_ = value;
  }

  // Override whether to use the TPM for added security
  void set_use_tpm(bool value) {
    use_tpm_ = value;
  }

  // Manually set the logged in user
  void set_current_user(UserSession* value) {
    current_user_ = value;
  }


  // Set/get last access timestamp cache instance for test purposes.
  UserOldestActivityTimestampCache* user_timestamp_cache() const {
    return user_timestamp_;
  }

  // Set/get a flag, that this machine is enterprise owned.
  void set_enterprise_owned(bool value) {
    enterprise_owned_ = value;
    homedirs_.set_enterprise_owned(value);
  }

  // Set/get time delta of last user's activity to be considered as old.
  void set_old_user_last_activity_time(base::TimeDelta value) {
    old_user_last_activity_time_ = value;
    homedirs_.set_old_user_last_activity_time(value);
  }

  // Flag indicating if PKCS#11 is ready.
  typedef enum {
    kUninitialized = 0,  // PKCS#11 initialization hasn't been attempted.
    kIsWaitingOnTPM,  // PKCS#11 initialization is waiting on TPM ownership,
    kIsBeingInitialized,  // PKCS#11 is being attempted asynchronously.
    kIsInitialized,  // PKCS#11 was attempted and succeeded.
    kIsFailed,  // PKCS#11 was attempted and failed.
    kInvalidState,  // We should never be in this state.
  } Pkcs11State;

  void set_pkcs11_state(Pkcs11State value) {
    pkcs11_state_ = value;
  }

  Pkcs11State pkcs11_state() {
    return pkcs11_state_;
  }

  // Returns the status of this mount as a Value
  //
  // The returned object is a dictionary whose keys describe the mount. Current
  // keys are: "keysets", "mounted", "owner" and "enterprise".
  virtual Value* GetStatus();

  // Inserts the current user's PKCS #11 token.
  bool InsertPkcs11Token();

  // Removes the current user's PKCS #11 token.
  void RemovePkcs11Token();

 protected:
  FRIEND_TEST(ServiceInterfaceTest, CheckAsyncTestCredentials);
  friend class MakeTests;
  friend class MountTest;
  friend class EphemeralTest;

  // Used to override the policy provider for testing (takes ownership)
  void set_policy_provider(policy::PolicyProvider* provider) {
    policy_provider_.reset(provider);
    homedirs_.set_policy_provider(provider);
  }

 private:
  // Checks if the cryptohome vault exists for the given credentials
  //
  // Parameters
  //   credentials - The Credentials representing the user whose cryptohome
  //     should be checked.
  virtual bool DoesCryptohomeExist(const Credentials& credentials) const;

  // Creates the the tracked subdirectories in a user's cryptohome
  // If the cryptohome did not have tracked directories, but had them untracked,
  // migrate their contents.
  //
  // Parameters
  //   credentials - The Credentials representing the user
  //   is_new - True, if the cryptohome is being created and there is
  //            no need in migration
  virtual bool CreateTrackedSubdirectories(const Credentials& credentials,
                                           bool is_new) const;

  // Creates the cryptohome salt, key, and vault for the specified credentials
  //
  // Parameters
  //   credentials - The Credentials representing the user whose cryptohome
  //     should be created.
  virtual bool CreateCryptohome(const Credentials& credentials) const;

  // Migrates from the home-in-encfs setup to the home-in-subdir setup. Instead
  // of storing all the user's files in the root of the encfs, we store them in
  // a subdirectory of it to make room for a root-owned, user-encrypted volume.
  //
  // Parameters
  //   dir - directory to migrate
  virtual void MigrateToUserHome(const std::string& dir) const;

  // Changes the group ownership and permissions on those directories inside
  // the cryptohome that need to be accessible by other system daemons
  virtual bool SetupGroupAccess() const;

  virtual bool LoadVaultKeyset(const Credentials& credentials,
                               SerializedVaultKeyset* encrypted_keyset) const;

  virtual bool LoadVaultKeysetForUser(
      const std::string& obfuscated_username,
      SerializedVaultKeyset* encrypted_keyset) const;

  virtual bool StoreVaultKeyset(
      const Credentials& credentials,
      const SerializedVaultKeyset& encrypted_keyset) const;

  virtual bool StoreVaultKeysetForUser(
      const std::string& obfuscated_username,
      const SerializedVaultKeyset& encrypted_keyset) const;

  // Encrypts and adds the VaultKeyset to the serialized store
  //
  // Parameters
  //   credentials - The Credentials for the user
  //   vault_keyset - The VaultKeyset to save
  //   serialized (IN/OUT) - The SerializedVaultKeyset to add the encrypted
  //                         VaultKeyset to
  bool AddVaultKeyset(const Credentials& credentials,
                      const VaultKeyset& vault_keyset,
                      SerializedVaultKeyset* serialized) const;

  // Resaves the vault keyset, restoring on failure.  The vault_keyset supplied
  // is encrypted and stored in the wrapped_keyset parameter of serialized,
  // which is then saved to disk.
  //
  // Parameters
  //   credentials - The Credentials for the user
  //   vault_keyset - The VaultKeyset to save
  //   serialized (IN/OUT) - The serialized container to be saved
  bool ReEncryptVaultKeyset(const Credentials& credentials,
                            const VaultKeyset& vault_keyset,
                            SerializedVaultKeyset* serialized) const;

  // Attempt to decrypt the keyset for the specified user.  The method both
  // deserializes the SerializedVaultKeyset from disk and decrypts the
  // encrypted vault keyset, returning it in vault_keyset.
  //
  // Parameters
  //   credentials - The user credentials to use
  //   vault_keyset (OUT) - The unencrypted vault keyset on success
  //   serialized (OUT) - The keyset container as deserialized from disk
  //   error (OUT) - The specific error when decrypting
  bool DecryptVaultKeyset(const Credentials& credentials,
                          bool migrate_if_needed,
                          VaultKeyset* vault_keyset,
                          SerializedVaultKeyset* serialized,
                          MountError* error) const;

  // Cache the old key file and salt file during migration
  //
  // Parameters
  //   files - The file names to cache
  bool CacheOldFiles(const std::vector<std::string>& files) const;

  // Move the cached files back to the original files
  //
  // Parameters
  //   files - The file names to un-cache
  bool RevertCacheFiles(const std::vector<std::string>& files) const;

  // Remove the cached files for the user
  //
  // Parameters
  //   files - The file names to remove
  bool DeleteCacheFiles(const std::vector<std::string>& files) const;

  // Gets the user's salt file name
  //
  // Parameters
  //   credentials - The Credentials representing the user
  std::string GetUserSaltFile(const Credentials& credentials) const;

  // Gets the user's key file name
  //
  // Parameters
  //   credentials - The Credentials representing the user
  std::string GetUserKeyFile(const Credentials& credentials) const;

  // Gets the user's key file name
  //
  // Parameters
  //   obfuscated_username - Obfuscated username field of the Credentials
  std::string GetUserKeyFileForUser(
      const std::string& obfuscated_username) const;

  // Gets the directory in the shadow root where the user's salt, key, and vault
  // are stored.
  //
  // Parameters
  //   credentials - The Credentials representing the user
  std::string GetUserDirectory(const Credentials& credentials) const;

  // Gets the directory in the shadow root where the user's salt, key, and vault
  // are stored.
  //
  // Parameters
  //   obfuscated_username - Obfuscated username field of the Credentials
  std::string GetUserDirectoryForUser(
      const std::string& obfuscated_username) const;

  // Gets the directory representing the user's ephemeral cryptohome.
  //
  // Parameters
  //   obfuscated_username - Obfuscated username field of the credentials.
  std::string GetUserEphemeralPath(
      const std::string& obfuscated_username) const;

  // Gets the user's vault directory
  //
  // Parameters
  //   obfuscated_username - Obfuscated username field of the credentials.
  std::string GetUserVaultPath(const std::string& obfuscated_username) const;

  // Gets the directory to mount the user's cryptohome at
  //
  // Parameters
  //   credentials - The credentials representing the user
  std::string GetUserMountDirectory(const Credentials& credentials) const;

  // Returns the path of a user passthrough inside a vault
  //
  // Parameters
  //   vault - vault path
  std::string VaultPathToUserPath(const std::string& vault) const;

  // Returns the path of a root passthrough inside a vault
  //
  // Parameters
  //   vault - vault path
  std::string VaultPathToRootPath(const std::string& vault) const;

  // Returns the mounted userhome path (e.g. /home/.shadow/.../mount/user)
  //
  // Parameters
  //   credentials - The Credentials representing the user
  std::string GetMountedUserHomePath(const Credentials& credentials) const;

  // Returns the mounted roothome path (e.g. /home/.shadow/.../mount/root)
  //
  // Parameters
  //   credentials - The Credentials representing the user
  std::string GetMountedRootHomePath(const Credentials& credentials) const;

  // Get the owner user's obfuscated hash. This is empty if the owner has not
  // been set yet or the device is enterprise owned.
  // The value is cached. It is the caller's responsibility to invoke
  // ReloadDevicePolicy() whenever a refresh of the cache is desired.
  std::string GetObfuscatedOwner();

  // Returns the state of the ephemeral users policy. Defaults to non-ephemeral
  // users by returning false if the policy cannot be loaded.
  // The value is cached. It is the caller's responsibility to invoke
  // ReloadDevicePolicy() whenever a refresh of the cache is desired.
  bool AreEphemeralUsersEnabled();

  // Reloads the device policy.
  void ReloadDevicePolicy();

  // Checks Chaps Directory and makes sure that it has the correct
  // permissions, owner uid and gid. If any of these values are
  // incorrect, permissions_check is set to false. If the directory
  // does not exist, it is created and initialzed with the correct
  // values. If the directory or its attributes cannot be checked,
  // set or created, a fatal error has occured and the function
  // returns false.
  //
  // Parameters
  //   permissions_check - set to false if permissions, uid or gid is incorrect
  bool CheckChapsDirectory(bool* permissions_check);

  // Ensures that the device policy is loaded.
  //
  // Parameters
  //   force_reload - Whether to force a reload to pick up any policy changes.
  void EnsureDevicePolicyLoaded(bool force_reload);

  // Invokes given callback for every unmounted cryptohome
  //
  // Parameters
  //   callback - Routine to invoke.
  typedef base::Callback<void(const FilePath&)> CryptohomeCallback;
  void DoForEveryUnmountedCryptohome(const CryptohomeCallback& cryptohome_cb);

  // Same as MountCryptohome but specifies if the cryptohome directory should be
  // recreated on a fatal error
  //
  // Parameters
  //   credentials - The Credentials representing the user
  //   mount_args - The options for the call to mount: whether to create the
  //                cryptohome if it doesn't exist and any tracked directories
  //                to create
  //   recreate_decrypt_fatal - Attempt to recreate the cryptohome directory on
  //                            a fatal error (for example, TPM was cleared)
  //   error - The specific error condition on failure
  virtual bool MountCryptohomeInner(const Credentials& credentials,
                                    const MountArgs& mount_args,
                                    bool recreate_decrypt_fatal,
                                    MountError* error);

  // Mounts and populates an ephemeral cryptohome backed by tmpfs for the given
  // user.
  //
  // Parameters
  //   credentials - The credentials representing the user.
  bool MountEphemeralCryptohome(const Credentials& credentials);

  // Sets up a freshly mounted ephemeral cryptohome by adjusting its permissions
  // and populating it with a skeleton directory and file structure.
  //
  // Parameters
  //   home_dir - The path at which the user's cryptohome has been mounted.
  bool SetUpEphemeralCryptohome(const std::string& home_dir);

  // Recursively copies directory contents to the destination if the destination
  // file does not exist.  Sets ownership to the default_user_
  //
  // Parameters
  //   destination - Where to copy files to
  //   source - Where to copy files from
  void RecursiveCopy(const FilePath& destination, const FilePath& source) const;

  // Copies the skeleton directory to the user's cryptohome if that user is
  // currently mounted
  //
  // Parameters
  //   credentials - The Credentials representing the user
  void CopySkeletonForUser(const Credentials& credentials) const;

  // Copies the skeleton directory to the cryptohome mount point
  //
  void CopySkeleton() const;

  // Returns the user's salt
  //
  // Parameters
  //   credentials - The Credentials representing the user
  //   force - Whether to force creation of a new salt
  //   salt (OUT) - The user's salt
  void GetUserSalt(const Credentials& credentials, bool force_new,
                   chromeos::SecureBlob* salt) const;

  // Ensures that a specified directory exists, with all path components but the
  // last one owned by kMountOwnerUid:kMountOwnerGid and the last component
  // owned by final_uid:final_gid.
  //
  // Parameters
  //   fp - Directory to check
  //   final_uid - uid that must own the directory
  //   final_gid - gid that muts own the directory
  bool EnsureDirHasOwner(const FilePath& fp, uid_t final_uid,
                         gid_t final_gid) const;

  // Ensures that root and user mountpoints for the specified user are present.
  // Returns false if the mountpoints were not present and could not be created.
  //
  // Parameters
  //   credentials - The Credentials representing the user
  bool EnsureUserMountPoints(const Credentials& credentials) const;

  // Mount a mount point for a user, remembering it for later unmounting
  // Returns true if the mount succeeds, false otherwise
  //
  // Parameters
  //   user - User to mount for
  //   src - Directory to mount from
  //   dest - Directory to mount to
  //   type - Filesystem type to mount with
  //   options - Filesystem options to supply
  bool MountForUser(UserSession* user,
                    const std::string& src,
                    const std::string& dest,
                    const std::string& type,
                    const std::string& options);

  // Bind a mount point for a user, remembering it for later unmounting
  // Returns true if the bind succeeds, false otherwise
  //
  // Parameters
  //   user - User to mount for
  //   src - Directory to bind from
  //   dest - Directory to bind to
  bool BindForUser(UserSession* user,
                   const std::string& src,
                   const std::string& dest);

  // Pops a mount point from user's stack and unmounts it
  // Returns true if there was a mount to unmount, false otherwise
  // Relies on ForceUnmount internally; see the caveat listed for it
  //
  // Parameters
  //   user - User for whom to unmount
  bool UnmountForUser(UserSession* user);

  // Unmounts all mount points for a user
  // Relies on ForceUnmount() internally; see the caveat listed for it
  //
  // Parameters
  //   user - User for whom to unmount
  void UnmountAllForUser(UserSession* user);

  // Forcibly unmounts a mountpoint, killing processes with open handles to it
  // if necessary. Note that this approach is not bulletproof - if a process can
  // avoid being killed by racing against us, then grab a handle to the
  // mountpoint, it can prevent the lazy unmount from ever completing.
  //
  // Parameters
  //   mount_point - Mount point to unmount
  void ForceUnmount(const std::string& mount_point);

  // Deletes a given cryptohome unless it is currently mounted or belongs to the
  // owner.
  //
  // Parameters
  //   vault - The path to the vault inside the cryptohome to delete.
  void RemoveNonOwnerCryptohomesCallback(const FilePath& vault);

  // Deletes all directories under |prefix| whose names are obfuscated usernames
  // except those currently mounted or matching the owner's obfuscated username.
  //
  // Parameters
  //   prefix - The prefix under which to delete.
  void RemoveNonOwnerDirectories(const FilePath& prefix);

  // Removes the cryptohome directories and mount points except those currently
  // mounted or belonging to the owner.
  void RemoveNonOwnerCryptohomes();

  // Derives PKCS #11 token authorization data from a passkey. This may take up
  // to ~100ms (dependant on CPU / memory performance). Returns true on success.
  bool DeriveTokenAuthData(const chromeos::SecureBlob& passkey,
                           std::string* auth_data);

  // The uid of the shared user.  Ownership of the user's vault is set to this
  // uid.
  uid_t default_user_;

  // The uid of the chaps user. Ownership of the user's PKCS #11 token directory
  // is set to this uid.
  uid_t chaps_user_;

  // The gid of the shared user.  Ownership of the user's vault is set to this
  // gid.
  gid_t default_group_;

  // The gid of the shared access group.  Ownership of the user's home and
  // Downloads directory to this gid.
  gid_t default_access_group_;

  // The file path to mount cryptohome at.  Defaults to /home/chronos/user
  std::string mount_point_;

  // Where to store the system salt and user salt/key/vault.  Defaults to
  // /home/.shadow
  std::string shadow_root_;

  // Where the skeleton for the user's cryptohome is copied from
  std::string skel_source_;

  // Stores the global system salt
  chromeos::SecureBlob system_salt_;

  // The crypto implementation
  scoped_ptr<Crypto> default_crypto_;
  Crypto *crypto_;

  // The platform-specific calls
  scoped_ptr<Platform> default_platform_;
  Platform *platform_;

  // Whether to use the TPM for added security
  bool use_tpm_;

  // Used to keep track of the current logged-in user
  scoped_ptr<UserSession> default_current_user_;
  UserSession* current_user_;

  // Cache of last access timestamp for existing users.
  scoped_ptr<UserOldestActivityTimestampCache> default_user_timestamp_;
  UserOldestActivityTimestampCache* user_timestamp_;

  // Used to retrieve the owner user.
  scoped_ptr<policy::PolicyProvider> policy_provider_;

  // True if the machine is enterprise owned, false if not or we have
  // not discovered it in this session.
  bool enterprise_owned_;

  // Time delta of last user's activity to be considered as old.
  base::TimeDelta old_user_last_activity_time_;

  Pkcs11State pkcs11_state_;

  // Used to send login/logout events to Chaps.
  chaps::LoginEventClient chaps_event_client_;

  // Used to track the user's passkey. PKCS #11 initialization consumes and
  // clears this value.
  chromeos::SecureBlob pkcs11_passkey_;

  // Used to track the user's old passkey during passkey migration. PKCS #11
  // initialization consumes and clears this value. This value is valid only if
  // is_pkcs11_passkey_migration_required_ is set to true.
  chromeos::SecureBlob pkcs11_old_passkey_;

  // Used to track whether passkey migration has occurred and PKCS #11 migration
  // of authorization data based on the passkey needs to be performed also.
  bool is_pkcs11_passkey_migration_required_;

  // Temporary; some of the methods on Mount (currently,
  // DoAutomaticFreeDiskSpaceControl) are shims that call into this object
  // instead. Please do not use this; it will go away once the shims are gone.
  HomeDirs homedirs_;

  // Stack of mounts (in the mount(2) sense) that we've made.
  MountStack mounts_;

  FRIEND_TEST(MountTest, MountForUserOrderingTest);
  FRIEND_TEST(MountTest, UserActivityTimestampUpdated);
  FRIEND_TEST(MountTest, GoodReDecryptTest);
  FRIEND_TEST(MountTest, MountCryptohomeNoChange);
  FRIEND_TEST(MountTest, CheckChapsDirectoryCalledWithExistingDirWithBadPerms);
  FRIEND_TEST(MountTest, CheckChapsDirectoryCalledWithExistingDirWithBadUID);
  FRIEND_TEST(MountTest, CheckChapsDirectoryCalledWithExistingDirWithBadGID);
  FRIEND_TEST(MountTest,
              CheckChapsDirectoryCalledWithNonexistingDirWithFatalError);
  FRIEND_TEST(MountTest,
              CheckChapsDirectoryCalledWithExistingDirWithFatalError);
  FRIEND_TEST(MountTest, CheckChapsDirectoryCalledWithExistingDir);
  friend class DoAutomaticFreeDiskSpaceControlTest;

  DISALLOW_COPY_AND_ASSIGN(Mount);
};

}  // namespace cryptohome

#endif  // CRYPTOHOME_MOUNT_H_
