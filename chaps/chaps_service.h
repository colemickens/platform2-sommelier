// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHAPS_CHAPS_SERVICE_H_
#define CHAPS_CHAPS_SERVICE_H_

#include <string>
#include <vector>

#include <base/memory/scoped_ptr.h>

#include "chaps/chaps_interface.h"
#include "chaps/slot_manager.h"
#include "pkcs11/cryptoki.h"

namespace chaps {

// ChapsServiceImpl implements the Chaps IPC interface.  This class effectively
// serves as the entry point to the Chaps daemon and is called directly by
// ChapsAdaptor.
class ChapsServiceImpl : public ChapsInterface {
 public:
  // ChapsServiceImpl does not take ownership of slot_manager and will not
  // delete it.
  explicit ChapsServiceImpl(SlotManager* slot_manager);
  virtual ~ChapsServiceImpl();
  bool Init();
  void TearDown();
  // ChapsInterface methods
  virtual uint32_t GetSlotList(const chromeos::SecureBlob& isolate_credential,
                               bool token_present,
                               std::vector<uint64_t>* slot_list);
  virtual uint32_t GetSlotInfo(const chromeos::SecureBlob& isolate_credential,
                               uint64_t slot_id,
                               std::vector<uint8_t>* slot_description,
                               std::vector<uint8_t>* manufacturer_id,
                               uint64_t* flags,
                               uint8_t* hardware_version_major,
                               uint8_t* hardware_version_minor,
                               uint8_t* firmware_version_major,
                               uint8_t* firmware_version_minor);
  virtual uint32_t GetTokenInfo(const chromeos::SecureBlob& isolate_credential,
                                uint64_t slot_id,
                                std::vector<uint8_t>* label,
                                std::vector<uint8_t>* manufacturer_id,
                                std::vector<uint8_t>* model,
                                std::vector<uint8_t>* serial_number,
                                uint64_t* flags,
                                uint64_t* max_session_count,
                                uint64_t* session_count,
                                uint64_t* max_session_count_rw,
                                uint64_t* session_count_rw,
                                uint64_t* max_pin_len,
                                uint64_t* min_pin_len,
                                uint64_t* total_public_memory,
                                uint64_t* free_public_memory,
                                uint64_t* total_private_memory,
                                uint64_t* free_private_memory,
                                uint8_t* hardware_version_major,
                                uint8_t* hardware_version_minor,
                                uint8_t* firmware_version_major,
                                uint8_t* firmware_version_minor);
  virtual uint32_t GetMechanismList(
      const chromeos::SecureBlob& isolate_credential,
      uint64_t slot_id,
      std::vector<uint64_t>* mechanism_list);
  virtual uint32_t GetMechanismInfo(
      const chromeos::SecureBlob& isolate_credential,
      uint64_t slot_id,
      uint64_t mechanism_type,
      uint64_t* min_key_size,
      uint64_t* max_key_size,
      uint64_t* flags);
  virtual uint32_t InitToken(const chromeos::SecureBlob& isolate_credential,
                             uint64_t slot_id,
                             const std::string* so_pin,
                             const std::vector<uint8_t>& label);
  virtual uint32_t InitPIN(const chromeos::SecureBlob& isolate_credential,
                           uint64_t session_id, const std::string* pin);
  virtual uint32_t SetPIN(const chromeos::SecureBlob& isolate_credential,
                          uint64_t session_id,
                          const std::string* old_pin,
                          const std::string* new_pin);
  virtual uint32_t OpenSession(const chromeos::SecureBlob& isolate_credential,
                               uint64_t slot_id, uint64_t flags,
                               uint64_t* session_id);
  virtual uint32_t CloseSession(const chromeos::SecureBlob& isolate_credential,
                                uint64_t session_id);
  virtual uint32_t CloseAllSessions(
      const chromeos::SecureBlob& isolate_credential,
      uint64_t slot_id);
  virtual uint32_t GetSessionInfo(
      const chromeos::SecureBlob& isolate_credential,
      uint64_t session_id,
      uint64_t* slot_id,
      uint64_t* state,
      uint64_t* flags,
      uint64_t* device_error);
  virtual uint32_t GetOperationState(
      const chromeos::SecureBlob& isolate_credential,
      uint64_t session_id,
      std::vector<uint8_t>* operation_state);
  virtual uint32_t SetOperationState(
      const chromeos::SecureBlob& isolate_credential,
      uint64_t session_id,
      const std::vector<uint8_t>& operation_state,
      uint64_t encryption_key_handle,
      uint64_t authentication_key_handle);
  virtual uint32_t Login(const chromeos::SecureBlob& isolate_credential,
                         uint64_t session_id,
                         uint64_t user_type,
                         const std::string* pin);
  virtual uint32_t Logout(const chromeos::SecureBlob& isolate_credential,
                          uint64_t session_id);
  virtual uint32_t CreateObject(const chromeos::SecureBlob& isolate_credential,
                                uint64_t session_id,
                                const std::vector<uint8_t>& attributes,
                                uint64_t* new_object_handle);
  virtual uint32_t CopyObject(const chromeos::SecureBlob& isolate_credential,
                              uint64_t session_id,
                              uint64_t object_handle,
                              const std::vector<uint8_t>& attributes,
                              uint64_t* new_object_handle);
  virtual uint32_t DestroyObject(const chromeos::SecureBlob& isolate_credential,
                                 uint64_t session_id,
                                 uint64_t object_handle);
  virtual uint32_t GetObjectSize(const chromeos::SecureBlob& isolate_credential,
                                 uint64_t session_id,
                                 uint64_t object_handle,
                                 uint64_t* object_size);
  virtual uint32_t GetAttributeValue(
      const chromeos::SecureBlob& isolate_credential,
      uint64_t session_id,
      uint64_t object_handle,
      const std::vector<uint8_t>& attributes_in,
      std::vector<uint8_t>* attributes_out);
  virtual uint32_t SetAttributeValue(
      const chromeos::SecureBlob& isolate_credential,
      uint64_t session_id,
      uint64_t object_handle,
      const std::vector<uint8_t>& attributes);
  virtual uint32_t FindObjectsInit(
      const chromeos::SecureBlob& isolate_credential,
      uint64_t session_id,
      const std::vector<uint8_t>& attributes);
  virtual uint32_t FindObjects(const chromeos::SecureBlob& isolate_credential,
                               uint64_t session_id,
                               uint64_t max_object_count,
                               std::vector<uint64_t>* object_list);
  virtual uint32_t FindObjectsFinal(
      const chromeos::SecureBlob& isolate_credential,
      uint64_t session_id);
  virtual uint32_t EncryptInit(const chromeos::SecureBlob& isolate_credential,
                               uint64_t session_id,
                               uint64_t mechanism_type,
                               const std::vector<uint8_t>& mechanism_parameter,
                               uint64_t key_handle);
  virtual uint32_t Encrypt(const chromeos::SecureBlob& isolate_credential,
                           uint64_t session_id,
                           const std::vector<uint8_t>& data_in,
                           uint64_t max_out_length,
                           uint64_t* actual_out_length,
                           std::vector<uint8_t>* data_out);
  virtual uint32_t EncryptUpdate(const chromeos::SecureBlob& isolate_credential,
                                 uint64_t session_id,
                                 const std::vector<uint8_t>& data_in,
                                 uint64_t max_out_length,
                                 uint64_t* actual_out_length,
                                 std::vector<uint8_t>* data_out);
  virtual uint32_t EncryptFinal(const chromeos::SecureBlob& isolate_credential,
                                uint64_t session_id,
                                uint64_t max_out_length,
                                uint64_t* actual_out_length,
                                std::vector<uint8_t>* data_out);
  virtual void EncryptCancel(const chromeos::SecureBlob& isolate_credential,
                             uint64_t session_id);
  virtual uint32_t DecryptInit(const chromeos::SecureBlob& isolate_credential,
                               uint64_t session_id,
                               uint64_t mechanism_type,
                               const std::vector<uint8_t>& mechanism_parameter,
                               uint64_t key_handle);
  virtual uint32_t Decrypt(const chromeos::SecureBlob& isolate_credential,
                           uint64_t session_id,
                           const std::vector<uint8_t>& data_in,
                           uint64_t max_out_length,
                           uint64_t* actual_out_length,
                           std::vector<uint8_t>* data_out);
  virtual uint32_t DecryptUpdate(const chromeos::SecureBlob& isolate_credential,
                                 uint64_t session_id,
                                 const std::vector<uint8_t>& data_in,
                                 uint64_t max_out_length,
                                 uint64_t* actual_out_length,
                                 std::vector<uint8_t>* data_out);
  virtual uint32_t DecryptFinal(const chromeos::SecureBlob& isolate_credential,
                                uint64_t session_id,
                                uint64_t max_out_length,
                                uint64_t* actual_out_length,
                                std::vector<uint8_t>* data_out);
  virtual void DecryptCancel(const chromeos::SecureBlob& isolate_credential,
                             uint64_t session_id);
  virtual uint32_t DigestInit(const chromeos::SecureBlob& isolate_credential,
                              uint64_t session_id,
                              uint64_t mechanism_type,
                              const std::vector<uint8_t>& mechanism_parameter);
  virtual uint32_t Digest(const chromeos::SecureBlob& isolate_credential,
                          uint64_t session_id,
                          const std::vector<uint8_t>& data_in,
                          uint64_t max_out_length,
                          uint64_t* actual_out_length,
                          std::vector<uint8_t>* digest);
  virtual uint32_t DigestUpdate(const chromeos::SecureBlob& isolate_credential,
                                uint64_t session_id,
                                const std::vector<uint8_t>& data_in);
  virtual uint32_t DigestKey(const chromeos::SecureBlob& isolate_credential,
                             uint64_t session_id,
                             uint64_t key_handle);
  virtual uint32_t DigestFinal(const chromeos::SecureBlob& isolate_credential,
                               uint64_t session_id,
                               uint64_t max_out_length,
                               uint64_t* actual_out_length,
                               std::vector<uint8_t>* digest);
  virtual void DigestCancel(const chromeos::SecureBlob& isolate_credential,
                            uint64_t session_id);
  virtual uint32_t SignInit(const chromeos::SecureBlob& isolate_credential,
                            uint64_t session_id,
                            uint64_t mechanism_type,
                            const std::vector<uint8_t>& mechanism_parameter,
                            uint64_t key_handle);
  virtual uint32_t Sign(const chromeos::SecureBlob& isolate_credential,
                        uint64_t session_id,
                        const std::vector<uint8_t>& data,
                        uint64_t max_out_length,
                        uint64_t* actual_out_length,
                        std::vector<uint8_t>* signature);
  virtual uint32_t SignUpdate(const chromeos::SecureBlob& isolate_credential,
                              uint64_t session_id,
                              const std::vector<uint8_t>& data_part);
  virtual uint32_t SignFinal(const chromeos::SecureBlob& isolate_credential,
                             uint64_t session_id,
                             uint64_t max_out_length,
                             uint64_t* actual_out_length,
                             std::vector<uint8_t>* signature);
  virtual void SignCancel(const chromeos::SecureBlob& isolate_credential,
                          uint64_t session_id);
  virtual uint32_t SignRecoverInit(
      const chromeos::SecureBlob& isolate_credential,
      uint64_t session_id,
      uint64_t mechanism_type,
      const std::vector<uint8_t>& mechanism_parameter,
      uint64_t key_handle);
  virtual uint32_t SignRecover(const chromeos::SecureBlob& isolate_credential,
                               uint64_t session_id,
                               const std::vector<uint8_t>& data,
                               uint64_t max_out_length,
                               uint64_t* actual_out_length,
                               std::vector<uint8_t>* signature);
  virtual uint32_t VerifyInit(const chromeos::SecureBlob& isolate_credential,
                              uint64_t session_id,
                              uint64_t mechanism_type,
                              const std::vector<uint8_t>& mechanism_parameter,
                              uint64_t key_handle);
  virtual uint32_t Verify(const chromeos::SecureBlob& isolate_credential,
                          uint64_t session_id,
                          const std::vector<uint8_t>& data,
                          const std::vector<uint8_t>& signature);
  virtual uint32_t VerifyUpdate(const chromeos::SecureBlob& isolate_credential,
                                uint64_t session_id,
                                const std::vector<uint8_t>& data_part);
  virtual uint32_t VerifyFinal(const chromeos::SecureBlob& isolate_credential,
                               uint64_t session_id,
                               const std::vector<uint8_t>& signature);
  virtual void VerifyCancel(const chromeos::SecureBlob& isolate_credential,
                            uint64_t session_id);
  virtual uint32_t VerifyRecoverInit(
      const chromeos::SecureBlob& isolate_credential,
      uint64_t session_id,
      uint64_t mechanism_type,
      const std::vector<uint8_t>& mechanism_parameter,
      uint64_t key_handle);
  virtual uint32_t VerifyRecover(const chromeos::SecureBlob& isolate_credential,
                                 uint64_t session_id,
                                 const std::vector<uint8_t>& signature,
                                 uint64_t max_out_length,
                                 uint64_t* actual_out_length,
                                 std::vector<uint8_t>* data);
  virtual uint32_t DigestEncryptUpdate(
      const chromeos::SecureBlob& isolate_credential,
      uint64_t session_id,
      const std::vector<uint8_t>& data_in,
      uint64_t max_out_length,
      uint64_t* actual_out_length,
      std::vector<uint8_t>* data_out);
  virtual uint32_t DecryptDigestUpdate(
      const chromeos::SecureBlob& isolate_credential,
      uint64_t session_id,
      const std::vector<uint8_t>& data_in,
      uint64_t max_out_length,
      uint64_t* actual_out_length,
      std::vector<uint8_t>* data_out);
  virtual uint32_t SignEncryptUpdate(
      const chromeos::SecureBlob& isolate_credential,
      uint64_t session_id,
      const std::vector<uint8_t>& data_in,
      uint64_t max_out_length,
      uint64_t* actual_out_length,
      std::vector<uint8_t>* data_out);
  virtual uint32_t DecryptVerifyUpdate(
      const chromeos::SecureBlob& isolate_credential,
      uint64_t session_id,
      const std::vector<uint8_t>& data_in,
      uint64_t max_out_length,
      uint64_t* actual_out_length,
      std::vector<uint8_t>* data_out);
  virtual uint32_t GenerateKey(const chromeos::SecureBlob& isolate_credential,
                               uint64_t session_id,
                               uint64_t mechanism_type,
                               const std::vector<uint8_t>& mechanism_parameter,
                               const std::vector<uint8_t>& attributes,
                               uint64_t* key_handle);
  virtual uint32_t GenerateKeyPair(
      const chromeos::SecureBlob& isolate_credential,
      uint64_t session_id,
      uint64_t mechanism_type,
      const std::vector<uint8_t>& mechanism_parameter,
      const std::vector<uint8_t>& public_attributes,
      const std::vector<uint8_t>& private_attributes,
      uint64_t* public_key_handle,
      uint64_t* private_key_handle);
  virtual uint32_t WrapKey(const chromeos::SecureBlob& isolate_credential,
                           uint64_t session_id,
                           uint64_t mechanism_type,
                           const std::vector<uint8_t>& mechanism_parameter,
                           uint64_t wrapping_key_handle,
                           uint64_t key_handle,
                           uint64_t max_out_length,
                           uint64_t* actual_out_length,
                           std::vector<uint8_t>* wrapped_key);
  virtual uint32_t UnwrapKey(const chromeos::SecureBlob& isolate_credential,
                             uint64_t session_id,
                             uint64_t mechanism_type,
                             const std::vector<uint8_t>& mechanism_parameter,
                             uint64_t wrapping_key_handle,
                             const std::vector<uint8_t>& wrapped_key,
                             const std::vector<uint8_t>& attributes,
                             uint64_t* key_handle);
  virtual uint32_t DeriveKey(const chromeos::SecureBlob& isolate_credential,
                             uint64_t session_id,
                             uint64_t mechanism_type,
                             const std::vector<uint8_t>& mechanism_parameter,
                             uint64_t base_key_handle,
                             const std::vector<uint8_t>& attributes,
                             uint64_t* key_handle);
  virtual uint32_t SeedRandom(const chromeos::SecureBlob& isolate_credential,
                              uint64_t session_id,
                              const std::vector<uint8_t>& seed);
  virtual uint32_t GenerateRandom(
      const chromeos::SecureBlob& isolate_credential,
      uint64_t session_id,
      uint64_t num_bytes,
      std::vector<uint8_t>* random_data);

 private:
  SlotManager* slot_manager_;
  bool init_;

  DISALLOW_COPY_AND_ASSIGN(ChapsServiceImpl);
};

}  // namespace chaps
#endif  // CHAPS_CHAPS_SERVICE_H_
