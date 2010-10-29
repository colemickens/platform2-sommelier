// Copyright (c) 2009-2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Crypto - class for handling the keyset key management functions relating to
// cryptohome.  This includes wrapping/unwrapping the vault keyset (and
// supporting functions) and setting/clearing the user keyring for use with
// ecryptfs.

#ifndef CRYPTOHOME_CRYPTO_H_
#define CRYPTOHOME_CRYPTO_H_

#include <base/basictypes.h>
#include <base/file_path.h>

#include "secure_blob.h"
#include "tpm.h"
#include "vault_keyset.h"
#include "vault_keyset.pb.h"

namespace cryptohome {

// Default entropy source is used to seed openssl's random number generator
extern const std::string kDefaultEntropySource;
// Default number of hash rounds to use when generating  key from a password
extern const unsigned int kDefaultPasswordRounds;

class Crypto : public EntropySource {
 public:

  enum PaddingScheme {
    PADDING_NONE = 0,
    PADDING_LIBRARY_DEFAULT = 1,
    PADDING_CRYPTOHOME_DEFAULT = 2,
  };

  enum BlockMode {
    ECB = 1,
    CBC = 2,
  };

  enum CryptoError {
    CE_NONE = 0,
    CE_TPM_FATAL,
    CE_TPM_COMM_ERROR,
    CE_TPM_DEFEND_LOCK,
    CE_TPM_CRYPTO,
    CE_SCRYPT_CRYPTO,
    CE_OTHER_FATAL,
    CE_OTHER_CRYPTO,
    CE_NO_PUBLIC_KEY_HASH,
  };

  // Default constructor, using the default entropy source
  Crypto();

  virtual ~Crypto();

  // Initializes Crypto
  bool Init();

  // Seeds the random number generator
  void SeedRng() const;

  // Returns random bytes of the given length
  //
  // Parameters
  //   rand (OUT) - Where to store the random bytes
  //   length - The number of random bytes to store in rand
  void GetSecureRandom(unsigned char *rand, unsigned int length) const;

  // Gets the AES block size
  unsigned int GetAesBlockSize() const;

  // AES decrypts the wrapped blob
  //
  // Parameters
  //   wrapped - The blob containing the encrypted data
  //   start - The position in the blob to start with
  //   count - The count of bytes to decrypt
  //   key - The AES key to use in decryption
  //   padding - The padding method to use
  //   unwrapped (OUT) - The unwrapped (decrypted) data
  bool UnwrapAes(const chromeos::Blob& wrapped, unsigned int start,
                 unsigned int count, const SecureBlob& key,
                 const SecureBlob& iv, PaddingScheme padding,
                 SecureBlob* unwrapped) const;

  // AES encrypts the plain text data using the specified key
  //
  // Parameters
  //   unwrapped - The plain text data to encrypt
  //   start - The position in the blob to start with
  //   count - The count of bytes to encrypt
  //   key - The AES key to use
  //   padding - The padding method to use
  //   wrapped - On success, the encrypted data
  bool WrapAes(const chromeos::Blob& unwrapped, unsigned int start,
               unsigned int count, const SecureBlob& key, const SecureBlob& iv,
               PaddingScheme padding, SecureBlob* wrapped) const;

  // Same as UnwrapAes, but allows using either CBC or ECB
  bool UnwrapAesSpecifyBlockMode(const chromeos::Blob& wrapped,
                                 unsigned int start, unsigned int count,
                                 const SecureBlob& key, const SecureBlob& iv,
                                 PaddingScheme padding, BlockMode block_mode,
                                 SecureBlob* unwrapped) const;

  // Same as WrapAes, but allows using either CBC or ECB
  bool WrapAesSpecifyBlockMode(const chromeos::Blob& unwrapped,
                               unsigned int start, unsigned int count,
                               const SecureBlob& key, const SecureBlob& iv,
                               PaddingScheme padding, BlockMode block_mode,
                               SecureBlob* wrapped) const;

  // Creates a new RSA key
  //
  // Parameters
  //   key_bits - The key size to generate
  //   n (OUT) - the modulus
  //   p (OUT) - the private key
  bool CreateRsaKey(unsigned int key_bits, SecureBlob* n, SecureBlob *p) const;

  // Unwraps an encrypted vault keyset.  The vault keyset should be the output
  // of WrapVaultKeyset().
  //
  // Parameters
  //   wrapped_keyset - The blob containing the encrypted keyset
  //   vault_wrapper - The passkey wrapper used to unwrap the keyset
  //   wrap_flags (OUT) - Whether the keyset was wrapped by the TPM or scrypt
  //   error (OUT) - The specific error code on failure
  //   vault_keyset (OUT) - The unwrapped vault keyset on success
  bool UnwrapVaultKeyset(const SerializedVaultKeyset& serialized,
                         const chromeos::Blob& vault_wrapper,
                         unsigned int* wrap_flags, CryptoError* error,
                         VaultKeyset* vault_keyset) const;

  // Wraps (encrypts) the vault keyset with the given wrapper
  //
  // Parameters
  //   vault_keyset - The VaultKeyset to encrypt
  //   vault_wrapper - The passkey wrapper used to wrap the keyset
  //   vault_wrapper_salt - The salt to use for the vault wrapper when wrapping
  //                        the keyset
  //   wrapped_keyset - On success, the encrypted vault keyset
  bool WrapVaultKeyset(const VaultKeyset& vault_keyset,
                       const SecureBlob& vault_wrapper,
                       const SecureBlob& vault_wrapper_salt,
                       SerializedVaultKeyset* serialized) const;

  // Unwraps an encrypted vault keyset in the old method
  //
  // Parameters
  //   wrapped_keyset - The blob containing the encrypted keyset
  //   vault_wrapper - The passkey wrapper used to unwrap the keyset
  //   vault_keyset (OUT) - The unwrapped vault keyset on success
  bool UnwrapVaultKeysetOld(const chromeos::Blob& wrapped_keyset,
                            const chromeos::Blob& vault_wrapper,
                            VaultKeyset* vault_keyset) const;

  // Wraps (encrypts) the vault keyset with the given wrapper in the old method
  //
  // Parameters
  //   vault_keyset - The VaultKeyset to encrypt
  //   vault_wrapper - The passkey wrapper used to wrap the keyset
  //   vault_wrapper_salt - The salt to use for the vault wrapper when wrapping
  //                        the keyset
  //   wrapped_keyset - On success, the encrypted vault keyset
  bool WrapVaultKeysetOld(const VaultKeyset& vault_keyset,
                          const SecureBlob& vault_wrapper,
                          const SecureBlob& vault_wrapper_salt,
                          SecureBlob* wrapped_keyset) const;

  // Converts the passkey directly to an AES key, using the default OpenSSL
  // conversion method.
  //
  // Parameters
  //   passkey - The passkey (hash, currently) to create the key from
  //   salt - The salt used in creating the key
  //   rounds - The number of SHA rounds to perform
  //   key (OUT) - The AES key
  //   iv (OUT) - The initialization vector
  bool PasskeyToAesKey(const chromeos::Blob& passkey,
                       const chromeos::Blob& salt, unsigned int rounds,
                       SecureBlob* key, SecureBlob* iv) const;

  // Converts the passkey to a symmetric key used to decrypt the user's
  // cryptohome key.
  //
  // Parameters
  //   passkey - The passkey (hash, currently) to create the key from
  //   salt - The salt used in creating the key
  //   iters - The hash iterations to use in generating the key
  //   wrapper (OUT) - The wrapper
  void PasskeyToWrapper(const chromeos::Blob& passkey,
                        const chromeos::Blob& salt, unsigned int iters,
                        SecureBlob* wrapper) const;

  // Gets an existing salt, or creates one if it doesn't exist
  //
  // Parameters
  //   path - The path to the salt file
  //   length - The length of the new salt if it needs to be created
  //   force - If true, forces creation of a new salt even if the file exists
  //   salt (OUT) - The salt
  bool GetOrCreateSalt(const FilePath& path, unsigned int length, bool force,
                       SecureBlob* salt) const;

  // Adds the specified key to the ecryptfs keyring so that the cryptohome can
  // be mounted.  Clears the user keyring first.
  //
  // Parameters
  //   vault_keyset - The keyset to add
  //   key_signature (OUT) - The signature of the cryptohome key that should be
  //     used in subsequent calls to mount(2)
  //   fnek_signature (OUT) - The signature of the cryptohome filename
  //     encryption key that should be used in subsequent calls to mount(2)
  bool AddKeyset(const VaultKeyset& vault_keyset,
                 std::string* key_signature,
                 std::string* fnek_signature) const;

  // Clears the user's kernel keyring
  void ClearKeyset() const;

  // Gets the SHA1 hash of the data provided
  void GetSha1(const chromeos::Blob& data, unsigned int start,
               unsigned int count, SecureBlob* hash) const;

  // Encodes a binary blob to hex-ascii
  //
  // Parameters
  //   blob - The binary blob to convert
  //   buffer (IN/OUT) - Where to store the converted blob
  //   buffer_length - The size of the buffer
  static void AsciiEncodeToBuffer(const chromeos::Blob& blob, char* buffer,
                                  unsigned int buffer_length);

  // Converts a null-terminated password to a passkey (ascii-encoded first half
  // of the salted SHA1 hash of the password).
  //
  // Parameters
  //   password - The password to convert
  //   salt - The salt used during hashing
  //   passkey (OUT) - The passkey
  static void PasswordToPasskey(const char *password,
                                const chromeos::Blob& salt,
                                SecureBlob* passkey);

  // Ensures that the TPM is connected
  CryptoError EnsureTpm(bool disconnect_first) const;

  // Overrides the default the entropy source
  void set_entropy_source(const std::string& entropy_source) {
    entropy_source_ = entropy_source;
  }

  // Sets whether or not to use scrypt to add a layer of protection to the vault
  // keyset when the TPM is not used
  void set_fallback_to_scrypt(bool value) {
    fallback_to_scrypt_ = value;
  }

  // Sets whether or not to use the TPM (must be called before init, depends
  // on the presence of a functioning, initialized TPM).  The TPM is merely used
  // to add a layer of difficulty in a brute-force attack against the user's
  // credentials.
  void set_use_tpm(bool value) {
    use_tpm_ = value;
  }

  // Sets whether to always load the TPM, even if it isn't used
  void set_load_tpm(bool value) {
    load_tpm_ = value;
  }

  // Sets the TPM implementation
  void set_tpm(Tpm* value) {
    tpm_ = value;
  }

  // Gets whether the TPM is set
  bool has_tpm() {
    return (tpm_ != NULL);
  }

  // Gets the TPM implementation
  const Tpm* get_tpm() {
    return tpm_;
  }

  // Checks if the TPM is connected
  bool is_tpm_connected() {
    if (tpm_ == NULL) {
      return false;
    }
    return tpm_->IsConnected();
  }

 private:
  // Converts a TPM error to a Crypto error
  CryptoError TpmErrorToCrypto(Tpm::TpmRetryAction retry_action) const;

  // Adds the specified key to the user keyring
  //
  // Parameters
  //   key - The key to add
  //   key_sig - The key's (ascii) signature
  //   salt - The salt
  bool PushVaultKey(const SecureBlob& key, const std::string& key_sig,
                           const SecureBlob& salt) const;

  std::string entropy_source_;

  // If set, the TPM will be used during the encryption of the vault keyset
  bool use_tpm_;

  // TODO(fes): Remove this when TPM becomes default or not, it is only used in
  // keyset migration.
  // If set, Crypto can process TPM-encrypted keysets, but won't default to save
  // keysets as TPM-encrypted.
  bool load_tpm_;

  // The TPM implementation
  Tpm *tpm_;

  // If set, Crypto will use scrypt to protect the vault keyset when the TPM is
  // not available for use (or turned off)
  bool fallback_to_scrypt_;

  DISALLOW_COPY_AND_ASSIGN(Crypto);
};

}  // namespace cryptohome

#endif  // CRYPTOHOME_CRYPTO_H_
