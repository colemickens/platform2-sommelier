// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATTESTATION_SERVER_ATTESTATION_SERVICE_H_
#define ATTESTATION_SERVER_ATTESTATION_SERVICE_H_

#include "attestation/common/attestation_interface.h"

#include <memory>
#include <string>

#include <base/callback.h>
#include <base/macros.h>
#include <base/memory/weak_ptr.h>
#include <base/threading/thread.h>
#include <chromeos/bind_lambda.h>
#include <chromeos/http/http_transport.h>

#include "attestation/server/crypto_utility.h"
#include "attestation/server/crypto_utility_impl.h"
#include "attestation/server/database.h"
#include "attestation/server/database_impl.h"
#include "attestation/server/key_store.h"
#include "attestation/server/pkcs11_key_store.h"
#include "attestation/server/tpm_utility.h"
#include "attestation/server/tpm_utility_v1.h"

namespace attestation {

// An implementation of AttestationInterface for the core attestation service.
// Access to TPM, network and local file-system resources occurs asynchronously
// with the exception of Initialize(). All methods must be called on the same
// thread that originally called Initialize().
// Usage:
//   std::unique_ptr<AttestationInterface> attestation =
//       new AttestationService();
//   CHECK(attestation->Initialize());
//   attestation->CreateGoogleAttestedKey(...);
//
// THREADING NOTES:
// This class runs a worker thread and delegates all calls to it. This keeps the
// public methods non-blocking while allowing complex implementation details
// with dependencies on the TPM, network, and filesystem to be coded in a more
// readable way. It also serves to serialize method execution which reduces
// complexity with TPM state.
//
// Tasks that run on the worker thread are bound with base::Unretained which is
// safe because the thread is owned by this class (so it is guaranteed not to
// process a task after destruction). Weak pointers are used to post replies
// back to the main thread.
class AttestationService : public AttestationInterface {
 public:
  AttestationService();
  ~AttestationService() override = default;

  // AttestationInterface methods.
  bool Initialize() override;
  void CreateGoogleAttestedKey(
      const std::string& key_label,
      KeyType key_type,
      KeyUsage key_usage,
      CertificateProfile certificate_profile,
      const std::string& username,
      const std::string& origin,
      const CreateGoogleAttestedKeyCallback& callback) override;

  // Mutators useful for testing.
  void set_crypto_utility(CryptoUtility* crypto_utility) {
    crypto_utility_ = crypto_utility;
  }

  void set_database(Database* database) {
    database_ = database;
  }

  void set_http_transport(
      const std::shared_ptr<chromeos::http::Transport>& transport) {
    http_transport_ = transport;
  }

  void set_key_store(KeyStore* key_store) {
    key_store_ = key_store;
  }

  void set_tpm_utility(TpmUtility* tpm_utility) {
    tpm_utility_ = tpm_utility;
  }

  // So tests don't need to duplicate URL decisions.
  const std::string& attestation_ca_origin() {
    return attestation_ca_origin_;
  }

 private:
  enum ACARequestType {
    kEnroll,          // Enrolls a device, certifying an identity key.
    kGetCertificate,  // Issues a certificate for a TPM-backed key.
  };

  struct CreateGoogleAttestedKeyTaskResult {
      AttestationStatus status{STATUS_SUCCESS};
      std::string certificate_chain;
      std::string server_error_details;
  };

  // A synchronous implementation of CreateGoogleAttestedKey appropriate to run
  // on the worker thread.
  void CreateGoogleAttestedKeyTask(
      const std::string& key_label,
      KeyType key_type,
      KeyUsage key_usage,
      CertificateProfile certificate_profile,
      const std::string& username,
      const std::string& origin,
      const std::shared_ptr<CreateGoogleAttestedKeyTaskResult>& result);

  // A callback for CreateGoogleAttestedKeyTask that invokes the original
  // |callback| with the given |result|. Having this relay allows us to use weak
  // pointer semantics to cancel callbacks.
  void CreateGoogleAttestedKeyTaskCallback(
      const CreateGoogleAttestedKeyCallback& callback,
      const std::shared_ptr<CreateGoogleAttestedKeyTaskResult>& result);

  // Returns true iff all information required for enrollment with the Google
  // Attestation CA is available.
  bool IsPreparedForEnrollment();

  // Returns true iff enrollment with the Google Attestation CA has been
  // completed.
  bool IsEnrolled();

  // Creates an enrollment request compatible with the Google Attestation CA.
  // Returns true on success.
  bool CreateEnrollRequest(std::string* enroll_request);

  // Finishes enrollment given an |enroll_response| from the Google Attestation
  // CA. Returns true on success. On failure, returns false and sets
  // |server_error| to the error string from the CA.
  bool FinishEnroll(const std::string& enroll_response,
                    std::string* server_error);

  // Creates a |certificate_request| compatible with the Google Attestation CA
  // for the key identified by |username| and |key_label|, according to the
  // given |profile| and |origin.
  bool CreateCertificateRequest(const std::string& username,
                                const std::string& key_label,
                                CertificateProfile profile,
                                const std::string& origin,
                                std::string* certificate_request,
                                std::string* message_id);

  // Finishes a certificate request by decoding the |certificate_response| to
  // recover the |certificate_chain| and storing it in association with the key
  // identified by |username| and |key_label|. Returns true on success. On
  // failure, returns false and sets |server_error| to the error string from the
  // CA.
  bool FinishCertificateRequest(const std::string& certificate_response,
                                const std::string& username,
                                const std::string& key_label,
                                const std::string& message_id,
                                std::string* certficate_chain,
                                std::string* server_error);

  // Sends a |request_type| |request| to the Google Attestation CA and waits for
  // the |reply|. Returns true on success.
  bool SendACARequestAndBlock(ACARequestType request_type,
                              const std::string& request,
                              std::string* reply);

  // Creates, certifies, and saves a new key for |username| with the given
  // |key_label|, |key_type|, and |key_usage|.
  bool CreateKey(const std::string& username,
                 const std::string& key_label,
                 KeyType key_type,
                 KeyUsage key_usage);

  // Finds the |key| associated with |username| and |key_label|. Returns false
  // if such a key does not exist.
  bool FindKeyByLabel(const std::string& username,
                      const std::string& key_label,
                      CertifiedKey* key);

  // Saves the |key| associated with |username| and |key_label|. Returns true on
  // success.
  bool SaveKey(const std::string& username,
               const std::string& key_label,
               const CertifiedKey& key);

  // Deletes the key associated with |username| and |key_label|.
  void DeleteKey(const std::string& username,
                 const std::string& key_label);

  // Adds named device-wide key to the attestation database.
  bool AddDeviceKey(const std::string& key_label, const CertifiedKey& key);

  // Removes a device-wide key from the attestation database.
  void RemoveDeviceKey(const std::string& key_label);

  // Creates a PEM certificate chain from the credential fields of a |key|.
  std::string CreatePEMCertificateChain(const CertifiedKey& key);

  // Creates a certificate in PEM format from a DER encoded X.509 certificate.
  std::string CreatePEMCertificate(const std::string& certificate);

  // Chooses a temporal index which will be used by the ACA to create a
  // certificate.  This decision factors in the currently signed-in |user| and
  // the |origin| of the certificate request.  The strategy is to find an index
  // which has not already been used by another user for the same origin.
  int ChooseTemporalIndex(const std::string& user, const std::string& origin);

  // Creates a Google Attestation CA URL for the given |request_type|.
  std::string GetACAURL(ACARequestType request_type) const;

  base::WeakPtr<AttestationService> GetWeakPtr();


  const std::string attestation_ca_origin_;

  // Other than initialization and destruction, these are used only by the
  // worker thread.
  CryptoUtility* crypto_utility_{nullptr};
  Database* database_{nullptr};
  std::shared_ptr<chromeos::http::Transport> http_transport_;
  KeyStore* key_store_{nullptr};
  TpmUtility* tpm_utility_{nullptr};

  // Default implementations for the above interfaces. These will be setup
  // during Initialize() if the corresponding interface has not been set with a
  // mutator.
  std::unique_ptr<CryptoUtilityImpl> default_crypto_utility_;
  std::unique_ptr<DatabaseImpl> default_database_;
  std::unique_ptr<Pkcs11KeyStore> default_key_store_;
  std::unique_ptr<chaps::TokenManagerClient> pkcs11_token_manager_;
  std::unique_ptr<TpmUtilityV1> default_tpm_utility_;

  // All work is done in the background. This serves to serialize requests and
  // allow synchronous implementation of complex methods. This is intentionally
  // declared after the thread-owned members.
  std::unique_ptr<base::Thread> worker_thread_;

  // Declared last so any weak pointers are destroyed first.
  base::WeakPtrFactory<AttestationService> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(AttestationService);
};

}  // namespace attestation

#endif  // ATTESTATION_SERVER_ATTESTATION_SERVICE_H_
