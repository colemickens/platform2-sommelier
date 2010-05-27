// Copyright (c) 2009-2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Mount - class for managing cryptohome user keys and mounts.  In Chrome OS,
// users are managed on top of a shared unix user, chronos.  When a user logs
// in, cryptohome mounts their encrypted home directory to /home/chronos/user,
// and Chrome does a profile switch to that directory.  All user data in their
// home directory is transparently encrypted, providing protection against
// offline theft.  On logout, the mount point is removed.
//
// Cryptohome manages directories as follows:
//
// /home/.shadow                            : Location for the system salt and
//                                            individual users' salt/key/vault
// /home/.shadow/<salted_hash_of_username>  : Each Chrome OS user gets a
//                                            directory in the shadow root where
//                                            their salts, keys, and vault are
//                                            stored.
// /home/.shadow/<s_h_o_u>/vault            : The user's vault (the encrypted
//                                            version of their home directory)
// /home/.shadow/<s_h_o_u>/master.X         : Vault key at index X.  Key indexes
//                                            may be used during password
//                                            change.  The vault key is
//                                            encrypted using a key generated
//                                            from the user's passkey and the
//                                            key-specific salt.
// /home/.shadow/<s_h_o_u>/master.X.salt    : Salt used in converting the user's
//                                            passkey to a passkey wrapper
//                                            (what encrypts the vault key) at
//                                            index X.
// /home/chronos/user                       : On successful login, the user's
//                                            vault directory is mounted here
//                                            using the symmetric key decrypted
//                                            from master.X by the user's
//                                            passkey.
//
// Offline login and screen unlock is processed through cryptohome using a test
// decryption of any of the user's master keys using the passkey provided.
//
// A user's cryptohome is automatically created when the vault directory for the
// user does not exist and the cryptohome service gets a call to mount the
// user's home directory.
//
// Passkey change: <TBD>

#ifndef MOUNT_H_
#define MOUNT_H_

#include "base/basictypes.h"
#include "base/file_path.h"
#include "cryptohome/credentials.h"
#include "cryptohome/secure_blob.h"
#include "cryptohome/vault_keyset.h"

namespace cryptohome {

// Default entropy source is used to seed openssl's random number generator
extern const std::string kDefaultEntropySource;
// The directory to mount the user's cryptohome at
extern const std::string kDefaultHomeDir;
// The directory containing the system salt and the user vaults
extern const std::string kDefaultShadowRoot;
// The default shared user (chronos)
extern const std::string kDefaultSharedUser;
// The default skeleton source (/etc/skel)
extern const std::string kDefaultSkeletonSource;
// The incognito user
extern const std::string kIncognitoUser;
// Where to find mtab
extern const std::string kMtab;
// Openssl-encrypted files start with "Salted__" and an 8-byte salt
extern const std::string kOpenSSLMagic;

// The Mount class handles mounting/unmounting of the user's cryptohome
// directory as well as offline verification of the user's credentials against
// the directory's crypto key.
class Mount : public EntropySource {
 public:
  enum MountError {
    MOUNT_ERROR_NONE = 0,
    MOUNT_ERROR_FATAL = 1 << 0,
    MOUNT_ERROR_KEY_FAILURE = 1 << 1,
  };

  // Sets up Mount with the default locations, username, etc., as defined above.
  Mount();

  // Sets up Mount with non-default locations
  explicit Mount(const std::string& username, const std::string& entropy_source,
                 const std::string& home_dir, const std::string& shadow_root,
                 const std::string& skel_source);

  virtual ~Mount();

  // Gets the uid/gid of the default user and loads the system salt
  virtual bool Init();

  // Attempts to mount the cryptohome for the given credentials
  //
  // Parameters
  //   credentials - The Credentials representing the user
  //   index - The key index to try
  //   error - The specific error condition on failure
  virtual bool MountCryptohome(const Credentials& credentials, int index,
                               MountError* error = NULL);

  // Unmounts any mount at the cryptohome mount point
  virtual bool UnmountCryptohome();

  // Remove a user's cryptohome
  virtual bool RemoveCryptohome(const Credentials& credentials);

  // Checks if the mount point currently has a mount
  virtual bool IsCryptohomeMounted();

  // Checks if the mount point currently has a mount, and if that mount is for
  // the specified credentials
  //
  // Parameters
  //   credentials - The Credentials for which to test the mount point
  virtual bool IsCryptohomeMountedForUser(const Credentials& credentials);

  // Creates the cryptohome salt, key, and vault for the specified credentials
  //
  // Parameters
  //   credentials - The Credentials representing the user whose cryptohome
  //     should be created.
  //   index - The key index to generate
  virtual bool CreateCryptohome(const Credentials& credentials, int index);

  // Tests if the given credentials would unwrap the user's cryptohome key
  //
  // Parameters
  //   credentials - The Credentials to attempt to unwrap the key with
  virtual bool TestCredentials(const Credentials& credentials);

  // Migrages a user's vault key from one passkey to another
  //
  // Parameters
  //   credentials - The new Credentials for the user
  //   from_key - The old Credentials
  virtual bool MigratePasskey(const Credentials& credentials,
                              const char* old_key);

  // Mounts an incognito home directory to the cryptohome mount point
  virtual bool MountIncognitoCryptohome();

  // Returns the system salt
  virtual SecureBlob GetSystemSalt();

  // Used to disable setting vault ownership
  void set_set_vault_ownership(bool value) {
    set_vault_ownership_ = value;
  }

 private:
  // Checks if the cryptohome vault exists for the given credentials and creates
  // it if not (calls CreateCryptohome).
  //
  // Parameters
  //   credentials - The Credentials representing the user whose cryptohome
  //     should be ensured.
  //   created (OUT) - Whether the cryptohome was created
  virtual bool EnsureCryptohome(const Credentials& credentials,
                                bool* created);

  // Saves the VaultKeyset for the user at the given index
  //
  // Parameters
  //   credentials - The Credentials for the user
  //   vault_keyset - The VaultKeyset to save
  //   index - The index to save the VaultKeyset to
  bool SaveVaultKeyset(const Credentials& credentials,
                       const VaultKeyset& vault_keyset,
                       int index);

  // Gets the directory in the shadow root where the user's salt, key, and vault
  // are stored.
  //
  // Parameters
  //   credentials - The Credentials representing the user
  std::string GetUserDirectory(const Credentials& credentials);

  // Gets the user's key file name at the given index
  //
  // Parameters
  //   credentials - The Credentials representing the user
  //   index - The key index to return
  std::string GetUserKeyFile(const Credentials& credentials, int index);

  // Gets the user's salt file name at the given index
  //
  // Parameters
  //   credentials - The Credentials representing the user
  //   index - The salt index to return
  std::string GetUserSaltFile(const Credentials& credentials, int index);

  // Gets the user's vault directory
  //
  // Parameters
  //   credentials - The Credentials representing the user
  std::string GetUserVaultPath(const Credentials& credentials);

  // Recursively copies directory contents to the destination if the destination
  // file does not exist.  Sets ownership to the default_user_
  //
  // Parameters
  //   destination - Where to copy files to
  //   source - Where to copy files from
  void RecursiveCopy(const FilePath& destination, const FilePath& source);

  // Copies the skeleton directory to the user's cryptohome if that user is
  // currently mounted
  //
  // Parameters
  //   credentials - The Credentials representing the user
  void CopySkeletonForUser(const Credentials& credentials);

  // Copies the skeleton directory to the cryptohome mount point
  //
  void CopySkeleton();

  // Returns the specified number of random bytes
  //
  // Parameters
  //   rand (IN/OUT) - Where to store the bytes, must be a least length bytes
  //   length - The number of random bytes to return
  void GetSecureRandom(unsigned char *rand, int length) const;

  // Creates a new master key and stores it in the master key file for a user
  //
  // Parameters
  //   credentials - The Credentials representing the user
  //   index - the key index to generate
  bool CreateMasterKey(const Credentials& credentials, int index);

  // Converts the passkey to a symmetric key used to decrypt the user's
  // cryptohome key.
  //
  // Parameters
  //   passkey - The passkey (hash, currently) to create the key from
  //   salt - The salt used in creating the key
  //   iters - The hash iterations to use in generating the key
  SecureBlob PasskeyToWrapper(const chromeos::Blob& passkey,
                         const chromeos::Blob& salt, int iters);

  // Returns the user's salt at the given index
  //
  // Parameters
  //   credentials - The Credentials representing the user
  //   index - The salt index to return
  //   force - Whether to force creation of a new salt
  SecureBlob GetUserSalt(const Credentials& credentials, int index,
                         bool force_new = false);

  // Gets the salt in the specified file, creating it if it does not exist
  //
  // Parameters
  //   path - The path of the salt file
  //   length - The length of the salt to create if it doesn't exist
  //   force - Whether to force creation of a new salt
  SecureBlob GetOrCreateSalt(const FilePath& path, int length,
                             bool force_new);

  // Loads the contents of the specified file as a blob
  //
  // Parameters
  //   path - The file path to read from
  //   blob (OUT) - Where to store the loaded file bytes
  bool LoadFileBytes(const FilePath& path, SecureBlob& blob);

  // Unmount a mount point
  //
  // Parameters
  //   path - The path to unmount
  //   lazy - Whether to perform a lazy unmount
  //   was_busy (OUT) - Whether the mount point was busy
  bool Unmount(const std::string& path, bool lazy, bool* was_busy);

  // Attempt to unwrap the key at the specified path
  //
  // Parameters
  //   path - The file path for the master key
  //   passkey - The passkey to use (converted to a passkey wrapper by this
  //     method)
  //   key (OUT) - Where to store the cryptohome key on success
  bool UnwrapMasterKey(const FilePath& path,
                       const chromeos::Blob& passkey,
                       VaultKeyset* key);

  // Adds the specified key to the ecryptfs keyring so that the cryptohome can
  // be mounted.  Clears the user keyring first.
  //
  // Parameters
  //   vault_keyset - The keyset to add
  //   key_signature (OUT) - The signature of the cryptohome key that should be
  //     used in subsequent calls to mount(2)
  //   fnek_signature (OUT) - The signature of the cryptohome filename
  //     encryption key that should be used in subsequent calls to mount(2)
  bool AddKeyToEcryptfsKeyring(const VaultKeyset& vault_keyset,
                               std::string* key_signature,
                               std::string* fnek_signature);

  // Adds the specified key to the user keyring
  //
  // Parameters
  //   key - The key to add
  //   key_sig - The key's (ascii) signature
  //   salt - The salt
  bool PushVaultKey(const SecureBlob& key, const std::string& key_sig,
                    const SecureBlob& salt);

  // Encodes a binary blob to hex-ascii
  //
  // Parameters
  //   blob - The binary blob to convert
  //   buffer (IN/OUT) - Where to store the converted blob
  //   buffer_length - The size of the buffer
  void AsciiEncodeToBuffer(const chromeos::Blob& blob, char *buffer,
                           int buffer_length);

  // Terminates or kills processes (except the current) that have files open on
  // the specified path.  Returns true if it tried to kill any processes.
  //
  // Parameters
  //   path - The path to check if the process has open files on
  //   hard - If true, send a SIGKILL instead of SIGTERM
  bool TerminatePidsWithOpenFiles(const std::string& path, bool hard);

  // Returns a vector of PIDs that have files open on the given path
  //
  // Parameters
  //   path - The path to check if the process has open files on
  std::vector<pid_t> LookForOpenFiles(const std::string& path);

  // Terminates or kills processes (except the current) that have the user ID
  // specified.  Returns true if it tried to kill any processes.
  //
  // Parameters
  //   path - The path to check if the process has open files on
  //   hard - If true, send a SIGKILL instead of SIGTERM
  bool TerminatePidsForUser(const uid_t uid, bool hard);

  // Returns a vector of PIDs whose Real, Effective, Saved, or File UID is equal
  // to that requested
  //
  // Parameters
  //   uid - the user ID to search for
  std::vector<pid_t> GetPidsForUser(uid_t uid);

  // The uid of the shared user.  Ownership of the user's vault is set to this
  // uid.
  uid_t default_user_;

  // The gid of the shared user.  Ownership of the user's vault is set to this
  // gid.
  gid_t default_group_;

  // The shared user name.  This user's uid/gid is used for vault ownership.
  const std::string default_username_;

  // The file path to load entropy from.  Defaults to /dev/urandom
  const std::string entropy_source_;

  // The file path to mount cryptohome at.  Defaults to /home/chronos/user
  const std::string home_dir_;

  // Where to store the system salt and user salt/key/vault.  Defaults to
  // /home/chronos/shadow
  const std::string shadow_root_;

  // Where the skeleton for the user's cryptohome is copied from
  const std::string skel_source_;

  // Stores the global system salt
  cryptohome::SecureBlob system_salt_;

  // Whether to change ownership of the vault file
  bool set_vault_ownership_;

  DISALLOW_COPY_AND_ASSIGN(Mount);
};

}

#endif  // MOUNT_H_
