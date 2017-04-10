// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
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

#include <memory>
#include <string>
#include <vector>

#include <base/callback.h>
#include <base/files/file_path.h>
#include <base/macros.h>
#include <base/memory/ref_counted.h>
#include <base/time/time.h>
#include <base/values.h>
#include <brillo/secure_blob.h>
#include <chromeos/dbus/service_constants.h>
#include <gtest/gtest_prod.h>
#include <policy/device_policy.h>
#include <policy/libpolicy.h>

#include "cryptohome/credentials.h"
#include "cryptohome/crypto.h"
#include "cryptohome/dircrypto_data_migrator/migration_helper.h"
#include "cryptohome/homedirs.h"
#include "cryptohome/mount_stack.h"
#include "cryptohome/platform.h"
#include "cryptohome/user_oldest_activity_timestamp_cache.h"
#include "cryptohome/user_session.h"
#include "cryptohome/vault_keyset.h"

#include "vault_keyset.pb.h"  // NOLINT(build/include)

namespace cryptohome {

// Directories that we intend to track (make pass-through in cryptohome vault)
extern const base::FilePath::CharType kCacheDir[];
extern const base::FilePath::CharType kDownloadsDir[];
extern const base::FilePath::CharType kGCacheDir[];
// subdir of kGCacheDir
extern const base::FilePath::CharType kGCacheVersionDir[];
// subdir of kGCacheVersionDir
extern const base::FilePath::CharType kGCacheTmpDir[];
// Name of the vault directory.
extern const base::FilePath::CharType kVaultDir[];
extern const char kUserHomeSuffix[];
extern const char kRootHomeSuffix[];
// Name of the mount directory.
extern const base::FilePath::CharType kMountDir[];
// Name of the temporary mount directory.
extern const base::FilePath::CharType kTemporaryMountDir[];
// Name of the key file.
extern const base::FilePath::CharType kKeyFile[];
// Automatic label prefix of a legacy key ("%s%d")
extern const char kKeyLegacyPrefix[];
// Maximum number of key files.
extern const int kKeyFileMax;
// File system type for ephemeral mounts.
extern const char kEphemeralMountType[];
extern const base::FilePath::CharType kEphemeralDir[];
extern const base::FilePath::CharType kGuestMountPath[];

class BootLockbox;
class ChapsClientFactory;
class UserOldestActivityTimestampCache;

// The Mount class handles mounting/unmounting of the user's cryptohome
// directory as well as offline verification of the user's credentials against
// the directory's crypto key.
class Mount : public base::RefCountedThreadSafe<Mount> {
 public:
  enum class MountType {
    NONE,  // Not mounted.
    ECRYPTFS,  // Encrypted with ecryptfs.
    DIR_CRYPTO,  // Encrypted with dircrypto.
    EPHEMERAL,  // Ephemeral mount.
  };

  struct MountArgs {
    bool create_if_missing;
    bool ensure_ephemeral;
    // When creating a new cryptohome from scratch, use ecryptfs.
    bool create_as_ecryptfs;
    // Forces dircrypto, i.e., makes it an error to mount ecryptfs.
    bool force_dircrypto;
    // Mount the existing ecryptfs vault to a temporary location while setting
    // up a new dircrypto directory.
    bool to_migrate_from_ecryptfs;

    MountArgs() : create_if_missing(false),
                  ensure_ephemeral(false),
                  create_as_ecryptfs(false),
                  force_dircrypto(false),
                  to_migrate_from_ecryptfs(false) {
    }

    void CopyFrom(const MountArgs& rhs) {
      this->create_if_missing = rhs.create_if_missing;
      this->ensure_ephemeral = rhs.ensure_ephemeral;
      this->create_as_ecryptfs = rhs.create_as_ecryptfs;
      this->force_dircrypto = rhs.force_dircrypto;
      this->to_migrate_from_ecryptfs = rhs.to_migrate_from_ecryptfs;
    }
  };

  // Sets up Mount with the default locations, username, etc., as defined above.
  Mount();

  virtual ~Mount();

  // Gets the uid/gid of the default user and loads the system salt
  virtual bool Init(Platform* platform, Crypto* crypto,
                    UserOldestActivityTimestampCache *cache);

  // Attempts to mount the cryptohome for the given credentials
  //
  // Parameters
  //   credentials - The Credentials representing the user
  //   mount_args - The options for the call to mount:
  //                * Whether to create the cryptohome if it doesn't exist.
  //                * Whether to ensure that the mount is ephemeral.
  //   error - The specific error condition on failure
  virtual bool MountCryptohome(const Credentials& credentials,
                               const MountArgs& mount_args,
                               MountError* error);

  // Unmounts any mount at the cryptohome mount point
  virtual bool UnmountCryptohome();

  // Checks whether the mount point currently has a cryptohome mounted for the
  // current user.
  //
  // Parameters
  //   credentials - The credentials for which to test the mount point.
  virtual bool IsMounted() const;

  // Checks whether the mount point currently has a cryptohome mounted for the
  // current user that is not ephemeral.
  //
  virtual bool IsNonEphemeralMounted() const;

  // Checks if the cryptohome vault exists for the given credentials and creates
  // it if not (calls CreateCryptohome and sets mount_type_).
  //
  // Parameters
  //   credentials - The Credentials representing the user whose cryptohome
  //     should be ensured.
  //   mount_args - The options for the call to mount.
  //   created (OUT) - Whether the cryptohome was created
  virtual bool EnsureCryptohome(const Credentials& credentials,
                                const MountArgs& mount_args,
                                bool* created);

  // Updates current user activity timestamp. This is called daily.
  // So we may not consider current user as old (and delete it soon after she
  // logs off). Returns true if current user is logged in and timestamp was
  // updated.
  // If no user is logged or the mount is ephemeral, nothing is done and false
  // is returned.
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


  //
  // virtual int AddKey(const Credentials& existing, const Credentials& new);
  // virtual bool RemoveKey(const Credentials& existing, int index);

  // Returns the current key index.  If there is no active key, -1 is returned.
  virtual int CurrentKey(void) const { return current_user_->key_index(); }

  // Used to override the default shadow root
  void set_shadow_root(const base::FilePath& value) {
    shadow_root_ = value;
  }

  // Used to override the default skeleton directory
  void set_skel_source(const base::FilePath& value) {
    skel_source_ = value;
  }

  // Used to override the default Crypto handler (does not take ownership)
  void set_crypto(Crypto* value) {
    crypto_ = value;
  }

  // Get the Crypto instance
  virtual Crypto* crypto() {
    return crypto_;
  }

  // Used to override the default HomeDirs handler (does not take ownership)
  void set_homedirs(HomeDirs* value) {
    homedirs_ = value;
  }

  // Get the HomeDirs instance
  virtual HomeDirs* homedirs() {
    return homedirs_;
  }

  virtual Platform* platform() {
    return platform_;
  }

  virtual const base::FilePath& mount_point() const {
    return mount_point_;
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

  // Set/get a flag, that this machine is enterprise owned.
  void set_enterprise_owned(bool value) {
    enterprise_owned_ = value;
    homedirs_->set_enterprise_owned(value);
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
  // keys are: "keysets", "mounted", "owner", "enterprise", and "type".
  virtual std::unique_ptr<base::Value> GetStatus();

  // Inserts the current user's PKCS #11 token.
  bool InsertPkcs11Token();

  // Removes the current user's PKCS #11 token.
  void RemovePkcs11Token();

  // Returns true if this Mount instances owns the mount path.
  virtual bool OwnsMountPoint(const base::FilePath& path) const;

  // Migrates the data from eCryptfs to dircrypto.
  // Call MountCryptohome with to_migrate_from_ecryptfs beforehand.
  bool MigrateToDircrypto(
      const dircrypto_data_migrator::MigrationHelper::ProgressCallback&
      callback);

  // Used to override the policy provider for testing (takes ownership)
  // TODO(wad) move this in line with other testing accessors
  void set_policy_provider(policy::PolicyProvider* provider) {
    policy_provider_.reset(provider);
    homedirs_->set_policy_provider(provider);
  }

  // Returns the temporary user path while we're migrating for
  // http://crbug.com/224291
  static base::FilePath GetNewUserPath(const std::string& username);

  void set_legacy_mount(bool legacy) { legacy_mount_ = legacy; }

  // Does not take ownership.
  void set_chaps_client_factory(ChapsClientFactory* factory) {
    chaps_client_factory_ = factory;
  }

  // Does not take ownership.
  void set_boot_lockbox(BootLockbox* lockbox) {
    boot_lockbox_ = lockbox;
  }

 protected:
  FRIEND_TEST(ServiceInterfaceTest, CheckAsyncTestCredentials);
  friend class MakeTests;
  friend class MountTest;
  friend class EphemeralTest;
  friend class ChapsDirectoryTest;

 private:
  // A class which scopes a mount point.  i.e. The mount point is unmounted on
  // destruction.
  class ScopedMountPoint {
   public:
    ScopedMountPoint(Mount* mount,
                     const base::FilePath& src,
                     const base::FilePath& dest);
    ~ScopedMountPoint();

   private:
    Mount* mount_;
    const base::FilePath src_;
    const base::FilePath dest_;
  };

  // Checks if the cryptohome vault exists for the given credentials
  //
  // Parameters
  //   credentials - The Credentials representing the user whose cryptohome
  //     should be checked.
  virtual bool DoesCryptohomeExist(const Credentials& credentials) const;

  // Specialized version of DoesCryptohomeExist only deals with Ecryptfs.
  //
  // Parameters
  //   credentials - The Credentials representing the user whose cryptohome
  //     should be checked.
  virtual bool DoesEcryptfsCryptohomeExist(
      const Credentials& credentials) const;

  // Specialized version of DoesCryptohomeExist only deals with Dircrypto.
  //
  // Parameters
  //   credentials - The Credentials representing the user whose cryptohome
  //     should be checked.
  virtual bool DoesDircryptoCryptohomeExist(
      const Credentials& credentials) const;

  // Returns the names of all tracked subdirectories.
  static std::vector<base::FilePath> GetTrackedSubdirectories();

  // Creates the tracked subdirectories in a user's cryptohome
  // If the cryptohome did not have tracked directories, but had them untracked,
  // migrate their contents.
  //
  // Parameters
  //   credentials - The Credentials representing the user
  //   is_new - True, if the cryptohome is being created and there is
  //            no need in migration
  virtual bool CreateTrackedSubdirectories(const Credentials& credentials,
                                           bool is_new) const;

  // Creates the cryptohome salt, key, (and vault if necessary) for the
  // specified credentials.
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
  virtual void MigrateToUserHome(const base::FilePath& dir) const;

  // Changes the group ownership and permissions on those directories inside
  // the cryptohome that need to be accessible by other system daemons
  virtual bool SetupGroupAccess(const base::FilePath& home_dir) const;


  virtual bool LoadVaultKeyset(const Credentials& credentials,
                               int index,
                               SerializedVaultKeyset* encrypted_keyset) const;

  virtual bool AddEcryptfsAuthToken(const VaultKeyset& vault_keyset,
                       std::string* key_signature,
                       std::string* filename_key_signature) const;

  virtual bool LoadVaultKeysetForUser(
      const std::string& obfuscated_username,
      int index,
      SerializedVaultKeyset* encrypted_keyset) const;

  virtual bool StoreVaultKeysetForUser(
      const std::string& obfuscated_username,
      int index,
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
                            int index,
                            SerializedVaultKeyset* serialized) const;

  // Attempt to decrypt the keyset for the specified user.  The method both
  // deserializes the SerializedVaultKeyset from disk and decrypts the
  // encrypted vault keyset, returning it in vault_keyset.
  //
  // Parameters
  //   credentials - The user credentials to use
  //   vault_keyset (OUT) - The unencrypted vault keyset on success
  //   serialized (OUT) - The keyset container as deserialized from disk
  //   index (OUT) - The keyset index from disk
  //   error (OUT) - The specific error when decrypting
  bool DecryptVaultKeyset(const Credentials& credentials,
                          bool migrate_if_needed,
                          VaultKeyset* vault_keyset,
                          SerializedVaultKeyset* serialized,
                          int* index,
                          MountError* error) const;

  // Cache the old key file and salt file during migration
  //
  // Parameters
  //   files - The file names to cache
  bool CacheOldFiles(const std::vector<base::FilePath>& files) const;

  // Move the cached files back to the original files
  //
  // Parameters
  //   files - The file names to un-cache
  bool RevertCacheFiles(const std::vector<base::FilePath>& files) const;

  // Remove the cached files for the user
  //
  // Parameters
  //   files - The file names to remove
  bool DeleteCacheFiles(const std::vector<base::FilePath>& files) const;

  // Gets the user's salt file name
  //
  // Parameters
  //   obfuscated_username - Obfuscated username field of the Credentials
  //   index - key index the salt is associated with
  base::FilePath GetUserSaltFileForUser(const std::string& obfuscated_username,
                                        int index) const;


  // Gets the user's key file name by index
  //
  // Parameters
  //   obfuscated_username - Obfuscated username field of the Credentials
  //   index - which key file to load
  base::FilePath GetUserLegacyKeyFileForUser(
      const std::string& obfuscated_username,
      int index) const;

  // Gets the user's key file name by label
  //
  // Parameters
  //   obfuscated_username - Obfuscated username field of the Credentials
  //   label - which key file to load by KeyData::label()
  base::FilePath GetUserKeyFileForUser(
      const std::string& obfuscated_username,
      const std::string& label) const;

  // Gets the directory in the shadow root where the user's salt, key, and vault
  // are stored.
  //
  // Parameters
  //   credentials - The Credentials representing the user
  base::FilePath GetUserDirectory(const Credentials& credentials) const;

  // Gets the directory in the shadow root where the user's salt, key, and vault
  // are stored.
  //
  // Parameters
  //   obfuscated_username - Obfuscated username field of the Credentials
  base::FilePath GetUserDirectoryForUser(
      const std::string& obfuscated_username) const;

  // Gets the directory representing the user's ephemeral cryptohome.
  //
  // Parameters
  //   obfuscated_username - Obfuscated username field of the credentials.
  base::FilePath GetUserEphemeralPath(
      const std::string& obfuscated_username) const;

  // Gets the user's vault directory
  //
  // Parameters
  //   obfuscated_username - Obfuscated username field of the credentials.
  base::FilePath GetUserVaultPath(const std::string& obfuscated_username) const;

  // Gets the directory to mount the user's cryptohome at
  //
  // Parameters
  //   credentials - The credentials representing the user
  base::FilePath GetUserMountDirectory(
      const std::string& obfuscated_username) const;

  // Gets the directory to temporarily mount the user's cryptohome at.
  //
  // Parameters
  //   obfuscated_username - Obfuscated username field of the credentials.
  base::FilePath GetUserTemporaryMountDirectory(
      const std::string& obfuscated_username) const;

  // Returns the path of a user passthrough inside a vault
  //
  // Parameters
  //   vault - vault path
  base::FilePath VaultPathToUserPath(const base::FilePath& vault) const;

  // Returns the path of a root passthrough inside a vault
  //
  // Parameters
  //   vault - vault path
  base::FilePath VaultPathToRootPath(const base::FilePath& vault) const;

  // Returns the mounted userhome path (e.g. /home/.shadow/.../mount/user)
  //
  // Parameters
  //   credentials - The Credentials representing the user
  base::FilePath GetMountedUserHomePath(
      const std::string& obfuscated_username) const;

  // Returns the mounted roothome path (e.g. /home/.shadow/.../mount/root)
  //
  // Parameters
  //   credentials - The Credentials representing the user
  base::FilePath GetMountedRootHomePath(
      const std::string& obfuscated_username) const;

  // Returns a path suitable for building a skeleton for an ephemeral home
  // directory.
  base::FilePath GetEphemeralSkeletonPath() const;

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
  // incorrect, the correct values are set. If the directory
  // does not exist, it is created and initialzed with the correct
  // values. If the directory or its attributes cannot be checked,
  // set or created, a fatal error has occured and the function
  // returns false.  If the directory does not exist and a legacy directory
  // exists, the legacy directory will be moved to the new location.
  //
  // Parameters
  //   dir - directory to check
  //   legacy_dir - legacy directory location
  bool CheckChapsDirectory(const base::FilePath& dir,
                           const base::FilePath& legacy_dir);

  // Ensures that the device policy is loaded.
  //
  // Parameters
  //   force_reload - Whether to force a reload to pick up any policy changes.
  void EnsureDevicePolicyLoaded(bool force_reload);

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
  //   source_path - A name for the mount point which will appear in /etc/mtab.
  //   home_dir - The path at which the user's cryptohome has been mounted.
  bool SetUpEphemeralCryptohome(const base::FilePath& source_path,
                                const base::FilePath& home_dir);

  // Recursively copies directory contents to the destination if the destination
  // file does not exist.  Sets ownership to the default_user_
  //
  // Parameters
  //   destination - Where to copy files to
  //   source - Where to copy files from
  void RecursiveCopy(const base::FilePath& destination,
                     const base::FilePath& source) const;

  // Copies the skeleton directory to the user's cryptohome.
  void CopySkeleton(const base::FilePath& destination) const;

  // Returns the user's salt
  //
  // Parameters
  //   credentials - The Credentials representing the user
  //   force - Whether to force creation of a new salt
  //   key_index - key index the salt is associated with
  //   salt (OUT) - The user's salt
  void GetUserSalt(const Credentials& credentials, bool force_new,
                   int key_index, brillo::SecureBlob* salt) const;

  // Ensures that the numth component of path is owned by uid/gid and is a
  // directory.
  bool EnsurePathComponent(const base::FilePath& fp, size_t num, uid_t uid,
                           gid_t gid) const;


  // Ensures that the permissions on every parent of NewUserDir are correct and
  // that they are all directories. Since we're going to bind-mount over
  // NewUserDir itself, we don't care what the permissions on it are, just that
  // it exists.
  // NewUserDir looks like: /home/chronos/u-$hash
  // /home needs to be root:root, /home/chronos needs to be uid:gid.
  bool EnsureNewUserDirExists(const base::FilePath& fp, uid_t uid,
                              gid_t gid) const;

  // Ensures that a specified directory exists, with all path components but the
  // last one owned by kMountOwnerUid:kMountOwnerGid and the last component
  // owned by final_uid:final_gid.
  //
  // Parameters
  //   fp - Directory to check
  //   final_uid - uid that must own the directory
  //   final_gid - gid that muts own the directory
  bool EnsureDirHasOwner(const base::FilePath& fp, uid_t final_uid,
                         gid_t final_gid) const;

  // Ensures that root and user mountpoints for the specified user are present.
  // Returns false if the mountpoints were not present and could not be created.
  //
  // Parameters
  //   credentials - The Credentials representing the user
  bool EnsureUserMountPoints(const Credentials& credentials) const;

  // Mount a mount point, remembering it for later unmounting
  // Returns true if the mount succeeds, false otherwise
  //
  // Parameters
  //   src - Directory to mount from
  //   dest - Directory to mount to
  //   type - Filesystem type to mount with
  //   options - Filesystem options to supply
  bool RememberMount(const base::FilePath& src,
                     const base::FilePath& dest,
                     const std::string& type,
                     const std::string& options);

  // Bind a mount point, remembering it for later unmounting
  // Returns true if the bind succeeds, false otherwise
  //
  // Parameters
  //   src - Directory to bind from
  //   dest - Directory to bind to
  bool RememberBind(const base::FilePath& src,
                    const base::FilePath& dest);

  // Unmounts all mount points
  // Relies on ForceUnmount() internally; see the caveat listed for it
  //
  void UnmountAll();

  // Forcibly unmounts a mountpoint, killing processes with open handles to it
  // if necessary. Note that this approach is not bulletproof - if a process can
  // avoid being killed by racing against us, then grab a handle to the
  // mountpoint, it can prevent the lazy unmount from ever completing.
  //
  // Parameters
  //   src - Path mounted at |dest|
  //   dest - Mount point to unmount
  void ForceUnmount(const base::FilePath& src, const base::FilePath& dest);

  // Derives PKCS #11 token authorization data from a passkey. This may take up
  // to ~100ms (dependant on CPU / memory performance). Returns true on success.
  bool DeriveTokenAuthData(const brillo::SecureBlob& passkey,
                           std::string* auth_data);

  // Mount the legacy home directory.
  // The legacy home directory is from before multiprofile and is mounted at
  // /home/chronos/user.
  bool MountLegacyHome(const base::FilePath& from, MountError* mount_error);

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
  base::FilePath mount_point_;

  // Where to store the system salt and user salt/key/vault.  Defaults to
  // /home/.shadow
  base::FilePath shadow_root_;

  // Where the skeleton for the user's cryptohome is copied from
  base::FilePath skel_source_;

  // Stores the global system salt
  brillo::SecureBlob system_salt_;

  // The platform-specific calls
  std::unique_ptr<Platform> default_platform_;
  Platform *platform_;

  // The crypto implementation
  Crypto *crypto_;

  // TODO(wad,ellyjones) Require HomeDirs at Init().
  // HomeDirs encapsulates operations on Cryptohomes at rest.
  std::unique_ptr<HomeDirs> default_homedirs_;
  HomeDirs *homedirs_;

  // Whether to use the TPM for added security
  bool use_tpm_;

  // Used to keep track of the current logged-in user
  std::unique_ptr<UserSession> default_current_user_;
  UserSession* current_user_;

  // Cache of last access timestamp for existing users.
  UserOldestActivityTimestampCache* user_timestamp_cache_;

  // Used to retrieve the owner user.
  std::unique_ptr<policy::PolicyProvider> policy_provider_;

  // True if the machine is enterprise owned, false if not or we have
  // not discovered it in this session.
  bool enterprise_owned_;

  Pkcs11State pkcs11_state_;

  // Used to track the user's passkey. PKCS #11 initialization consumes and
  // clears this value.
  brillo::SecureBlob pkcs11_token_auth_data_;

  // Used to track the user's old passkey during passkey migration. PKCS #11
  // initialization consumes and clears this value. This value is valid only if
  // is_pkcs11_passkey_migration_required_ is set to true.
  brillo::SecureBlob legacy_pkcs11_passkey_;

  // Used to track whether passkey migration has occurred and PKCS #11 migration
  // of authorization data based on the passkey needs to be performed also.
  bool is_pkcs11_passkey_migration_required_;

  // Stack of mounts (in the mount(2) sense) that we've made.
  MountStack mounts_;

  // Dircrypto key ID.
  key_serial_t dircrypto_key_id_;

  // Whether to mount the legacy homedir or not (see MountLegacyHome)
  bool legacy_mount_;

  // Indicates the type of the current mount.
  // This is only valid when IsMounted() is true.
  MountType mount_type_;

  std::unique_ptr<ChapsClientFactory> default_chaps_client_factory_;
  ChapsClientFactory* chaps_client_factory_;

  BootLockbox* boot_lockbox_;
  std::unique_ptr<BootLockbox> default_boot_lockbox_;

  FRIEND_TEST(MountTest, RememberMountOrderingTest);
  FRIEND_TEST(MountTest, MountCryptohomeChapsKey);
  FRIEND_TEST(MountTest, MountCryptohomeNoChapsKey);
  FRIEND_TEST(MountTest, UserActivityTimestampUpdated);
  FRIEND_TEST(MountTest, GoodReDecryptTest);
  FRIEND_TEST(MountTest, MountCryptohomeNoChange);
  FRIEND_TEST(MountTest, CheckChapsDirectoryMigration);
  FRIEND_TEST(MountTest, TwoWayKeysetMigrationTest);
  FRIEND_TEST(MountTest, BothFlagsMigrationTest);
  FRIEND_TEST(MountTest, CreateTrackedSubdirectories);

  DISALLOW_COPY_AND_ASSIGN(Mount);
};

}  // namespace cryptohome

#endif  // CRYPTOHOME_MOUNT_H_
