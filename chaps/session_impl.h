// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHAPS_SESSION_IMPL_H
#define CHAPS_SESSION_IMPL_H

#include "chaps/session.h"

#include <map>
#include <string>
#include <vector>

#include <base/memory/scoped_ptr.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>

#include "chaps/chaps_factory.h"
#include "chaps/object.h"
#include "chaps/object_pool.h"
#include "chaps/tpm_utility.h"
#include "pkcs11/cryptoki.h"

namespace chaps {

class ChapsFactory;
class ObjectPool;
class TPMUtility;

// SessionImpl is the interface for a PKCS #11 session.  This component is
// responsible for maintaining session state including the state of any multi-
// part operations and any session objects.  It is also responsible for
// executing all session-specific operations.
class SessionImpl : public Session {
 public:
  // The ownership and management of the pointers provided here are outside the
  // scope of this class. Typically, the object pool will be managed by the slot
  // manager and will be shared by all sessions associated with the same slot.
  // The tpm and factory objects are typically singletons and shared across all
  // sessions and slots.
  SessionImpl(int slot_id,
              ObjectPool* token_object_pool,
              TPMUtility* tpm_utility,
              ChapsFactory* factory,
              HandleGenerator* handle_generator,
              bool is_read_only);
  virtual ~SessionImpl();

  // General state management.
  virtual int GetSlot() const;
  virtual CK_STATE GetState() const;
  virtual bool IsReadOnly() const;
  virtual bool IsOperationActive(OperationType type) const;
  // Object management.
  virtual CK_RV CreateObject(const CK_ATTRIBUTE_PTR attributes,
                             int num_attributes,
                             int* new_object_handle);
  virtual CK_RV CopyObject(const CK_ATTRIBUTE_PTR attributes,
                           int num_attributes,
                           int object_handle,
                           int* new_object_handle);
  virtual CK_RV DestroyObject(int object_handle);
  virtual bool GetObject(int object_handle, const Object** object);
  virtual bool GetModifiableObject(int object_handle, Object** object);
  virtual bool FlushModifiableObject(Object* object);
  virtual CK_RV FindObjectsInit(const CK_ATTRIBUTE_PTR attributes,
                                int num_attributes);
  virtual CK_RV FindObjects(int max_object_count,
                            std::vector<int>* object_handles);
  virtual CK_RV FindObjectsFinal();
  // Cryptographic operations (encrypt, decrypt, digest, sign, verify).
  virtual CK_RV OperationInit(OperationType operation,
                              CK_MECHANISM_TYPE mechanism,
                              const std::string& mechanism_parameter,
                              const Object* key);
  virtual CK_RV OperationUpdate(OperationType operation,
                                const std::string& data_in,
                                int* required_out_length,
                                std::string* data_out);
  virtual CK_RV OperationFinal(OperationType operation,
                               int* required_out_length,
                               std::string* data_out);
  virtual CK_RV VerifyFinal(const std::string& signature);
  virtual CK_RV OperationSinglePart(OperationType operation,
                                    const std::string& data_in,
                                    int* required_out_length,
                                    std::string* data_out);
  // Key generation.
  virtual CK_RV GenerateKey(CK_MECHANISM_TYPE mechanism,
                            const std::string& mechanism_parameter,
                            const CK_ATTRIBUTE_PTR attributes,
                            int num_attributes,
                            int* new_key_handle);
  virtual CK_RV GenerateKeyPair(CK_MECHANISM_TYPE mechanism,
                                const std::string& mechanism_parameter,
                                const CK_ATTRIBUTE_PTR public_attributes,
                                int num_public_attributes,
                                const CK_ATTRIBUTE_PTR private_attributes,
                                int num_private_attributes,
                                int* new_public_key_handle,
                                int* new_private_key_handle);
  // Random number generation.
  virtual CK_RV SeedRandom(const std::string& seed);
  virtual CK_RV GenerateRandom(int num_bytes, std::string* random_data);
  // Waits for private objects to be loaded before returning.
  virtual void WaitForPrivateObjects();

 private:
  struct OperationContext {
    bool is_valid_;  // Whether the contents of this structure are valid.
    bool is_cipher_;  // Set to true when cipher_context_ is valid.
    bool is_digest_;  // Set to true when digest_context_ is valid.
    bool is_hmac_;  // Set to true when hmac_context_ is valid.
    bool is_finished_;  // Set to true when the operation completes.
    union {
      EVP_CIPHER_CTX cipher_context_;
      EVP_MD_CTX digest_context_;
      HMAC_CTX hmac_context_;
    };
    std::string data_;  // This can be used to queue input or output.
    const Object* key_;
    CK_MECHANISM_TYPE mechanism_;
    std::string parameter_;  // The mechanism parameter (if any).

    OperationContext();
    void Clear();
  };

  bool IsValidKeyType(OperationType operation,
                      CK_MECHANISM_TYPE mechanism,
                      CK_OBJECT_CLASS object_class,
                      CK_KEY_TYPE key_type);
  bool IsValidMechanism(OperationType operation, CK_MECHANISM_TYPE mechanism);
  CK_RV CipherInit(bool is_encrypt,
                   CK_MECHANISM_TYPE mechanism,
                   const std::string& mechanism_parameter,
                   const Object* key);
  CK_RV CipherUpdate(OperationContext* context,
                     const std::string& data_in,
                     int* required_out_length,
                     std::string* data_out);
  CK_RV CipherFinal(OperationContext* context);
  CK_RV CreateObjectInternal(const CK_ATTRIBUTE_PTR attributes,
                             int num_attributes,
                             const Object* copy_from_object,
                             int* new_object_handle);
  bool GenerateDESKey(std::string* key_material);
  bool GenerateKeyPairSoftware(int modulus_bits,
                               const std::string& public_exponent,
                               Object* public_object,
                               Object* private_object);
  std::string GenerateRandomSoftware(int num_bytes);
  std::string GetDERDigestInfo(CK_MECHANISM_TYPE mechanism);
  // Provides operation output and handles the buffer-too-small case.
  // The output data must be in context->data_.
  // required_out_length - In: The maximum number of bytes that can be received.
  //                       Out: The actual number of bytes to be received.
  // data_out - Receives the output data if maximum >= actual.
  CK_RV GetOperationOutput(OperationContext* context,
                           int* required_out_length,
                           std::string* data_out);
  // Returns the key usage flag that must be set in order to perform the given
  // operation (e.g. kEncrypt requires CKA_ENCRYPT to be TRUE).
  CK_ATTRIBUTE_TYPE GetRequiredKeyUsage(OperationType operation);
  bool GetTPMKeyHandle(const Object* key, int* key_handle);
  bool LoadLegacyRootKeys();
  bool IsHMAC(CK_MECHANISM_TYPE mechanism);
  // Returns true if the given cipher mechanism uses padding.
  bool IsPaddingEnabled(CK_MECHANISM_TYPE mechanism);
  bool IsRSA(CK_MECHANISM_TYPE mechanism);
  bool RSAEncrypt(OperationContext* context);
  bool RSADecrypt(OperationContext* context);
  bool RSASign(OperationContext* context);
  CK_RV RSAVerify(OperationContext* context,
                  const std::string& digest,
                  const std::string& signature);
  // Wraps the given private key using the TPM and deletes all sensitive
  // attributes. This is called when a private key is imported. On success,
  // the private key can only be accessed by the TPM.
  CK_RV WrapPrivateKey(Object* object);

  // Helpers to map PKCS #11 <--> OpenSSL.
  std::string ConvertFromBIGNUM(const BIGNUM* bignum);
  // Returns NULL if big_integer is empty.
  BIGNUM* ConvertToBIGNUM(const std::string& big_integer);
  // Always returns a non-NULL value.
  RSA* CreateKeyFromObject(const Object* key_object);
  const EVP_CIPHER* GetOpenSSLCipher(CK_MECHANISM_TYPE mechanism,
                                     size_t key_size);
  const EVP_MD* GetOpenSSLDigest(CK_MECHANISM_TYPE mechanism);

  ChapsFactory* factory_;
  std::vector<int> find_results_;
  size_t find_results_offset_;
  bool find_results_valid_;
  bool is_read_only_;
  std::map<const Object*, int> object_tpm_handle_map_;
  OperationContext operation_context_[kNumOperationTypes];
  int slot_id_;
  scoped_ptr<ObjectPool> session_object_pool_;
  ObjectPool* token_object_pool_;
  TPMUtility* tpm_utility_;
  bool is_legacy_loaded_;  // Tracks whether the legacy root keys are loaded.
  int private_root_key_;  // The legacy private root key.
  int public_root_key_;  // The legacy public root key.

  DISALLOW_COPY_AND_ASSIGN(SessionImpl);
};

}  // namespace

#endif  // CHAPS_SESSION_IMPL_H
