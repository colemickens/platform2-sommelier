// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTOHOME_ATTESTATION_H_
#define CRYPTOHOME_ATTESTATION_H_

#include <map>
#include <string>

#include <base/file_path.h>
#include <base/scoped_observer.h>
#include <base/synchronization/lock.h>
#include <base/threading/platform_thread.h>
#include <chromeos/secure_blob.h>
#include <metrics/metrics_library.h>
#include <openssl/evp.h>

#include "attestation.pb.h"
#include "crypto.h"
#include "install_attributes.h"

namespace cryptohome {

class KeyStore;
class Pkcs11KeyStore;
class Platform;
class Tpm;

// This class performs tasks which enable attestation enrollment.  These tasks
// include creating an AIK and recording all information about the AIK and EK
// that an attestation server will need to issue credentials for this system. If
// a platform does not have a TPM, this class does nothing.  This class is
// thread-safe.
class Attestation : public base::PlatformThread::Delegate,
                    public InstallAttributes::Observer {
 public:
  enum PCAType {
    kDefaultPCA,    // The Google-operated Privacy CA.
    kAlternatePCA,  // An alternate Privacy CA specified by enterprise policy.
    kMaxPCAType
  };
  // The caller retains ownership of all pointers.
  Attestation(Tpm* tpm,
              Platform* platform,
              Crypto* crypto,
              InstallAttributes* install_attributes);
  virtual ~Attestation();

  // Must be called before any other method.
  virtual void Initialize();

  // Returns true if the attestation enrollment blobs already exist.
  virtual bool IsPreparedForEnrollment();

  // Returns true if an AIK certificate exists.
  virtual bool IsEnrolled();

  // Creates attestation enrollment blobs if they do not already exist. This
  // includes extracting the EK information from the TPM and generating an AIK
  // to be used later during enrollment.
  virtual void PrepareForEnrollment();

  // Like PrepareForEnrollment(), but asynchronous.
  virtual void PrepareForEnrollmentAsync();

  // Verifies all attestation data as an attestation server would. Returns true
  // if all data is valid.
  virtual bool Verify();

  // Verifies the EK certificate.
  virtual bool VerifyEK();

  // Creates an enrollment request to be sent to the Privacy CA. This request
  // is a serialized AttestationEnrollmentRequest protobuf. Attestation
  // enrollment is a process by which the Privacy CA verifies the EK certificate
  // of a device and issues a certificate for an AIK. The enrollment process can
  // be finished by calling Enroll() with the response from the Privacy CA. This
  // method can only succeed if IsPreparedForEnrollment() returns true.
  //
  // Parameters
  //   pca_type - Specifies to which Privacy CA the request will be sent.
  //   pca_request - The request to be sent to the Privacy CA.
  //
  // Returns true on success.
  virtual bool CreateEnrollRequest(PCAType pca_type,
                                   chromeos::SecureBlob* pca_request);

  // Finishes the enrollment process. On success, IsEnrolled() will return true.
  // The response from the Privacy CA is a serialized
  // AttestationEnrollmentResponse protobuf. This method recovers the AIK
  // certificate by calling TPM_ActivateIdentity and stores this certificate to
  // be used later during a certificate request.
  //
  // Parameters
  //   pca_type - Specifies which Privacy CA created the response.
  //   pca_response - The Privacy CA's response to an enrollment request as
  //                  returned by CreateEnrollRequest().
  //
  // Returns true on success.
  virtual bool Enroll(PCAType pca_type,
                      const chromeos::SecureBlob& pca_response);

  // Creates an attestation certificate request to be sent to the Privacy CA.
  // The request is a serialized AttestationCertificateRequest protobuf. The
  // certificate request process generates and certifies a key in the TPM and
  // sends the AIK certificate along with information about the certified key to
  // the Privacy CA. The PCA verifies the information and issues a certificate
  // for the certified key. The certificate request process can be finished by
  // calling FinishCertRequest() with the response from the Privacy CA. This
  // method can only succeed if IsEnrolled() returns true.
  //
  // Parameters
  //   pca_type - Specifies to which Privacy CA the request will be sent.
  //   profile - Specifies the type of certificate to be requested.
  //   username - The user requesting the certificate.
  //   origin - Some certificate requests require information about the origin
  //            of the request.  If no origin is needed, this can be empty.
  //   pca_request - The request to be sent to the Privacy CA.
  //
  // Returns true on success.
  virtual bool CreateCertRequest(PCAType pca_type,
                                 CertificateProfile profile,
                                 const std::string& username,
                                 const std::string& origin,
                                 chromeos::SecureBlob* pca_request);

  // Finishes the certificate request process.  The |pca_response| from the PCA
  // is a serialized AttestationCertificateResponse protobuf. This final step
  // verifies the PCA operation succeeded and extracts the certificate for the
  // certified key.  The certificate is stored with the key in association with
  // the |key_name|.  If the key is a user-specific key it is stored in
  // association with the currently signed-in user.
  //
  // Parameters
  //   pca_response - The Privacy CA's response to a certificate request as
  //                  returned by CreateCertRequest().
  //   is_user_specific - Whether the key is associated with the current user.
  //   username - The canonical username of the active user profile.  Ignored if
  //              |is_user_specific| is false.
  //   key_name - A name for the key.  If a key already exists with this name it
  //              will be destroyed and replaced with this one.  This name may
  //              subsequently be used to query the certificate for the key
  //              using GetCertificateChain.
  //   certificate_chain - The PCA issued certificate chain in PEM format.  By
  //                       convention the certified key certificate is listed
  //                       first followed by intermediate CA certificate(s).
  //                       The PCA root certificate is not included.
  virtual bool FinishCertRequest(const chromeos::SecureBlob& pca_response,
                                 bool is_user_specific,
                                 const std::string& username,
                                 const std::string& key_name,
                                 chromeos::SecureBlob* certificate_chain);

  // Gets the PCA issued certificate chain for an existing certified key.
  // Returns true on success.
  //
  // Parameters
  //
  //   is_user_specific - Whether the key is associated with the current user.
  //   username - The canonical username of the active user profile.  Ignored if
  //              |is_user_specific| is false.
  //   key_name - The key name; this is the same name previously passed to
  //              FinishCertRequest.
  //   certificate_chain - The PCA issued certificate chain in the same format
  //                       used by FinishCertRequest.
  virtual bool GetCertificateChain(bool is_user_specific,
                                   const std::string& username,
                                   const std::string& key_name,
                                   chromeos::SecureBlob* certificate_chain);

  // Gets the public key for an existing certified key.  Returns true on
  // success.  This method is provided for convenience, the same public key can
  // also be extracted from the certificate chain.
  //
  // Parameters
  //   is_user_specific - Whether the key is associated with the current user.
  //   username - The canonical username of the active user profile.  Ignored if
  //              |is_user_specific| is false.
  //   key_name - The key name; this is the same name previously passed to
  //              FinishCertRequest.
  //   public_key - The public key in DER form, according to the RSAPublicKey
  //                ASN.1 type defined in PKCS #1 A.1.1.
  virtual bool GetPublicKey(bool is_user_specific,
                            const std::string& username,
                            const std::string& key_name,
                            chromeos::SecureBlob* public_key);

  // Returns true iff a given key exists.
  virtual bool DoesKeyExist(bool is_user_specific,
                            const std::string& username,
                            const std::string& key_name);

  // Signs a challenge for an enterprise device / user.  This challenge is
  // typically generated by and the response verified by DMServer.  Returns true
  // on success.
  //
  // Parameters
  //
  //   is_user_specific - Whether the key is associated with the current user.
  //   username - The canonical username of the active user profile.  Ignored if
  //              |is_user_specific| is false.
  //   key_name - The key name; this is the same name previously passed to
  //              FinishCertRequest.
  //   domain - A domain value to be included in the challenge response.  The
  //            value is opaque to this class.
  //   device_id - A device identifier to be included in the challenge response.
  //               This value is opaque to this class.
  //   include_signed_public_key - Whether the challenge response should include
  //                               a SignedPublicKeyAndChallenge.
  //   challenge - The challenge to be signed.
  //   response - On success is populated with the challenge response.
  virtual bool SignEnterpriseChallenge(
      bool is_user_specific,
      const std::string& username,
      const std::string& key_name,
      const std::string& domain,
      const chromeos::SecureBlob& device_id,
      bool include_signed_public_key,
      const chromeos::SecureBlob& challenge,
      chromeos::SecureBlob* response);

  // Signs a challenge outside of an enterprise context.  This challenge is
  // typically generated by some module running within Chrome.  Returns true
  // on success.
  //
  // Parameters
  //
  //   is_user_specific - Whether the key is associated with the current user.
  //   username - The canonical username of the active user profile.  Ignored if
  //              |is_user_specific| is false.
  //   key_name - The key name; this is the same name previously passed to
  //              FinishCertRequest.
  //   challenge - The challenge to be signed.
  //   response - On success is populated with the challenge response.
  virtual bool SignSimpleChallenge(bool is_user_specific,
                                   const std::string& username,
                                   const std::string& key_name,
                                   const chromeos::SecureBlob& challenge,
                                   chromeos::SecureBlob* response);

  // Registers a key with the user's PKCS #11 token.  On success, the key is
  // removed from its old location and will no longer be available via this
  // interface.  Returns true on success.
  //
  // Parameters
  //
  //   is_user_specific - Whether the key is associated with the current user.
  //   username - The canonical username of the active user profile.  Ignored if
  //              |is_user_specific| is false.
  //   key_name - The key name; this is the same name previously passed to
  //              FinishCertRequest.
  virtual bool RegisterKey(bool is_user_specific,
                           const std::string& username,
                           const std::string& key_name);

  // Gets a payload previously set for a key.  If the key exists but no payload
  // has been set, the payload will be empty.  Returns true on success.
  //
  // Parameters
  //
  //   is_user_specific - Whether the key is associated with the current user.
  //   username - The canonical username of the active user profile.  Ignored if
  //              |is_user_specific| is false.
  //   key_name - The key name; this is the same name previously passed to
  //              FinishCertRequest.
  //   payload - Receives the payload data.
  virtual bool GetKeyPayload(bool is_user_specific,
                             const std::string& key_name,
                             const std::string& username,
                             chromeos::SecureBlob* payload);

  // Sets a payload for a key; any previous payload will be overwritten.
  // Returns true on success.
  //
  // Parameters
  //
  //   is_user_specific - Whether the key is associated with the current user.
  //   username - The canonical username of the active user profile.  Ignored if
  //              |is_user_specific| is false.
  //   key_name - The key name; this is the same name previously passed to
  //              FinishCertRequest.
  //   payload - The payload data.
  virtual bool SetKeyPayload(bool is_user_specific,
                             const std::string& key_name,
                             const std::string& username,
                             const chromeos::SecureBlob& payload);

  // Deletes all keys where the key name has the given |key_prefix|.
  // Returns true on success.
  //
  // Parameters
  //
  //   is_user_specific - Whether the key is associated with the current user.
  //   username - The current user canonical email address.
  //   key_prefix - The key name prefix.
  virtual bool DeleteKeysByPrefix(bool is_user_specific,
                                  const std::string& username,
                                  const std::string& key_prefix);

  // Gets the TPM Endorsement Key (EK) certificate and returns it in PEM format
  // in addition to a hex SHA256 hash of the raw DER encoded certificate.  The
  // result is intended to be human readable and is always valid ASCII.  Returns
  // true on success.  This method requires the TPM owner password to be
  // available.
  virtual bool GetEKInfo(std::string* ek_info);

  // Sets an alternative attestation database location. Useful in testing.
  virtual void set_database_path(const char* path) {
    database_path_ = FilePath(path);
  }

  // Sets an alternate key store.
  virtual void set_user_key_store(KeyStore* user_key_store) {
    user_key_store_ = user_key_store;
  }

  virtual void set_enterprise_test_key(RSA* enterprise_test_key) {
    enterprise_test_key_ = enterprise_test_key;
  }

  // PlatformThread::Delegate interface.
  virtual void ThreadMain() { PrepareForEnrollment(); }

  // InstallAttributes::Observer interface.
  virtual void OnFinalized() { PrepareForEnrollmentAsync(); }

 private:
  typedef std::map<std::string, chromeos::SecureBlob> CertRequestMap;
  enum FirmwareType {
    kUnknown,
    kVerified,
    kDeveloper
  };
  // So we can use scoped_ptr with openssl types.
  struct RSADeleter {
    inline void operator()(void* ptr) const;
  };
  struct X509Deleter {
    inline void operator()(void* ptr) const;
  };
  struct EVP_PKEYDeleter {
    inline void operator()(void* ptr) const;
  };
  struct NETSCAPE_SPKIDeleter {
    inline void operator()(void* ptr) const;
  };
  static const size_t kQuoteExternalDataSize;
  static const size_t kCipherKeySize;
  static const size_t kNonceSize;
  static const size_t kDigestSize;
  static const char kDefaultDatabasePath[];
  static const char kDefaultPCAPublicKey[];
  static const char kDefaultPCAPublicKeyID[];
  static const char kEnterpriseSigningPublicKey[];
  static const char kEnterpriseEncryptionPublicKey[];
  static const char kEnterpriseEncryptionPublicKeyID[];
  static const struct CertificateAuthority {
    const char* issuer;
    const char* modulus;  // In hex format.
  } kKnownEndorsementCA[];
  static const struct PCRValue {
    bool developer_mode_enabled;
    bool recovery_mode_enabled;
    FirmwareType firmware_type;
  } kKnownPCRValues[];
  // ASN.1 DigestInfo header for SHA-256 (see PKCS #1 v2.1 section 9.2).
  static const unsigned char kSha256DigestInfo[];
  static const int kNumTemporalValues;
  // Install attribute names for alternate PCA attributes.
  static const char kAlternatePCAKeyAttributeName[];
  static const char kAlternatePCAKeyIDAttributeName[];

  Tpm* tpm_;
  Platform* platform_;
  Crypto* crypto_;
  // A lock to protect all data members except |tpm_| and |platform_| which are
  // not owned by this class.
  base::Lock lock_;
  chromeos::SecureBlob database_key_;
  chromeos::SecureBlob sealed_database_key_;
  FilePath database_path_;
  AttestationDatabase database_pb_;
  base::PlatformThreadHandle thread_;
  CertRequestMap pending_cert_requests_;
  scoped_ptr<Pkcs11KeyStore> pkcs11_key_store_;
  KeyStore* user_key_store_;
  // If set, this will be used to sign / encrypt enterprise challenge-response
  // data instead of using kEnterprise*PublicKey.
  RSA* enterprise_test_key_;
  MetricsLibrary metrics_;
  InstallAttributes* install_attributes_;
  ScopedObserver<InstallAttributes, InstallAttributes::Observer>
      install_attributes_observer_;
  // Don't use directly, use IsTPMReady() instead.
  bool is_tpm_ready_;
  bool is_prepare_in_progress_;

  // Moves data from a std::string container to a SecureBlob container.
  chromeos::SecureBlob ConvertStringToBlob(const std::string& s);

  // Moves data from a chromeos::Blob container to a std::string container.
  std::string ConvertBlobToString(const chromeos::Blob& blob);

  // Concatenates two SecureBlobs.
  chromeos::SecureBlob SecureCat(const chromeos::SecureBlob& blob1,
                                 const chromeos::SecureBlob& blob2);

  // Serializes and encrypts an attestation database.
  bool EncryptDatabase(const AttestationDatabase& db,
                       std::string* serial_encrypted_db);

  // Decrypts and parses an attestation database.
  bool DecryptDatabase(const std::string& serial_encrypted_db,
                       AttestationDatabase* db);

  // Writes an encrypted database to a persistent storage location.
  bool StoreDatabase(const std::string& serial_encrypted_db);

  // Reads a database from a persistent storage location.
  bool LoadDatabase(std::string* serial_encrypted_db);

  // Persists any changes made to database_pb_.
  bool PersistDatabaseChanges();

  // Ensures permissions of the database file are correct.
  void CheckDatabasePermissions();

  // Verifies an endorsement credential against known Chrome OS issuers.
  bool VerifyEndorsementCredential(const chromeos::SecureBlob& credential,
                                   const chromeos::SecureBlob& public_key);

  // Verifies identity key binding data.
  bool VerifyIdentityBinding(const IdentityBinding& binding);

  // Verifies a quote of PCR0.
  bool VerifyQuote(const chromeos::SecureBlob& aik_public_key,
                   const Quote& quote);

  // Verifies a certified key.
  bool VerifyCertifiedKey(const chromeos::SecureBlob& aik_public_key,
                          const chromeos::SecureBlob& certified_public_key,
                          const chromeos::SecureBlob& certified_key_info,
                          const chromeos::SecureBlob& proof);

  // Creates a public key based on a known credential issuer.
  scoped_ptr<EVP_PKEY, EVP_PKEYDeleter> GetAuthorityPublicKey(
      const char* issuer_name);

  // Verifies an RSA-PKCS1-SHA1 digital signature.
  bool VerifySignature(const chromeos::SecureBlob& public_key,
                       const chromeos::SecureBlob& signed_data,
                       const chromeos::SecureBlob& signature);

  // Clears the memory of the database protobuf.
  void ClearDatabase();

  // Clears the memory of a std::string.
  void ClearString(std::string* s);

  // Performs AIK activation with a fake credential.
  bool VerifyActivateIdentity(const chromeos::SecureBlob& delegate_blob,
                              const chromeos::SecureBlob& delegate_secret,
                              const chromeos::SecureBlob& identity_key_blob,
                              const chromeos::SecureBlob& identity_public_key,
                              const chromeos::SecureBlob& ek_public_key);

  // Encrypts the endorsement credential with the Privacy CA public key.
  bool EncryptEndorsementCredential(PCAType pca_type,
                                    const chromeos::SecureBlob& credential,
                                    EncryptedData* encrypted_credential);

  // Adds named device-wide key to the attestation database.
  bool AddDeviceKey(const std::string& key_name, const CertifiedKey& key);

  // Finds a key by name.
  bool FindKeyByName(bool is_user_specific,
                     const std::string& username,
                     const std::string& key_name,
                     CertifiedKey* key);

  // Saves a key either in the device or user key store.
  bool SaveKey(bool is_user_specific,
               const std::string& username,
               const std::string& key_name,
               const CertifiedKey& key);

  // Assembles a certificate chain in PEM format given a leaf certificate and an
  // intermediate CA certificate.  By convention, the leaf certificate will be
  // first.
  bool CreatePEMCertificateChain(const std::string& leaf_certificate,
                                 const std::string& intermediate_ca_cert,
                                 chromeos::SecureBlob* certificate_chain);

  // Creates a certificate in PEM format from a DER encoded X.509 certificate.
  std::string CreatePEMCertificate(const std::string& certificate);

  // Signs data with a certified key using the scheme specified for challenges
  // and outputs a serialized SignedData protobuf.
  bool SignChallengeData(const CertifiedKey& key,
                         const chromeos::SecureBlob& challenge,
                         chromeos::SecureBlob* response);

  // Validates incoming enterprise challenge data.
  bool ValidateEnterpriseChallenge(const SignedData& signed_challenge);

  // Encrypts a KeyInfo protobuf as required for an enterprise challenge
  // response.
  bool EncryptEnterpriseKeyInfo(const KeyInfo& key_info,
                                EncryptedData* encrypted_data);

  // Encrypts data into an EncryptedData protobuf, wrapping the encryption key
  // with |wrapping_key|.
  bool EncryptData(const chromeos::SecureBlob& input,
                   RSA* wrapping_key,
                   const std::string& wrapping_key_id,
                   EncryptedData* output);

  // Creates an RSA* given a modulus in hex format.  The exponent is always set
  // to 65537.  If an error occurs, NULL is returned.
  scoped_ptr<RSA, RSADeleter> CreateRSAFromHexModulus(
      const std::string& hex_modulus);

  // Creates a SignedPublicKeyAndChallenge with a random challenge.
  bool CreateSignedPublicKey(const CertifiedKey& key,
                             chromeos::SecureBlob* signed_public_key);

  // Standard AES-256-CBC padded encryption.
  bool AesEncrypt(const chromeos::SecureBlob& plaintext,
                  const chromeos::SecureBlob& key,
                  const chromeos::SecureBlob& iv,
                  chromeos::SecureBlob* ciphertext);

  // Standard AES-256-CBC padded decryption.
  bool AesDecrypt(const chromeos::SecureBlob& ciphertext,
                  const chromeos::SecureBlob& key,
                  const chromeos::SecureBlob& iv,
                  chromeos::SecureBlob* plaintext);

  // Chooses a temporal index which will be used by the PCA to create a
  // certificate.  This decision factors in the currently signed-in |user| and
  // the |origin| of the certificate request.  The strategy is to find an index
  // which has not already been used by another user for the same origin.
  int ChooseTemporalIndex(const std::string& user, const std::string& origin);

  // Ensures all endorsement data which uniquely identifies the device no longer
  // exists in the attestation database unencrypted.  Encrypted endorsement data
  // cannot be decrypted locally.
  void FinalizeEndorsementData();

  // Returns true if the TPM is ready.
  bool IsTPMReady();

  friend class AttestationTest;

  DISALLOW_COPY_AND_ASSIGN(Attestation);
};

}  // namespace cryptohome

#endif  // CRYPTOHOME__ATTESTATION_H_
