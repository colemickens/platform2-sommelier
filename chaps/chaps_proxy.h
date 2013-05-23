// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#ifndef CHAPS_CHAPS_PROXY_H
#define CHAPS_CHAPS_PROXY_H

#include <base/memory/scoped_ptr.h>
#include <base/synchronization/lock.h>
#include <chromeos/secure_blob.h>

#include "chaps/chaps_interface.h"
#include "chaps_proxy_generated.h"

namespace chaps {

// ChapsProxyImpl is the default implementation of the chaps proxy interface.
//
// All calls are forwarded to a dbus-c++ generated proxy object. All exceptions
// thrown by the dbus-c++ library are caught and handled in this class.
//
// The Connect() method must be called successfully before calling any other
// methods.
class ChapsProxyImpl : public ChapsInterface {
public:
  ChapsProxyImpl();
  virtual ~ChapsProxyImpl();
  virtual bool Init();

  virtual bool OpenIsolate(chromeos::SecureBlob* isolate_credential,
                           bool* new_isolate_created);
  virtual void CloseIsolate(const chromeos::SecureBlob& isolate_credential);
  virtual bool LoadToken(const chromeos::SecureBlob& isolate_credential,
                         const std::string& path,
                         const std::vector<uint8_t>& auth_data,
                         const std::string& label,
                         uint64_t* slot_id);
  virtual void UnloadToken(const chromeos::SecureBlob& isolate_credential,
                           const std::string& path);
  virtual void ChangeTokenAuthData(const std::string& path,
                                   const std::vector<uint8_t>& old_auth_data,
                                   const std::vector<uint8_t>& new_auth_data);
  virtual bool GetTokenPath(const chromeos::SecureBlob& isolate_credential,
                            uint64_t slot_id,
                            std::string* path);

  virtual void SetLogLevel(const int32_t& level);

  // ChapsInterface methods.
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
  virtual uint32_t FindObjects(
      const chromeos::SecureBlob& isolate_credential,
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
  virtual uint32_t VerifyRecoverInit(
      const chromeos::SecureBlob& isolate_credential,
      uint64_t session_id,
      uint64_t mechanism_type,
      const std::vector<uint8_t>& mechanism_parameter,
      uint64_t key_handle);
  virtual uint32_t VerifyRecover(
      const chromeos::SecureBlob& isolate_credential,
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
  // Waits for the service to be available. Returns false if the service is not
  // available within 5 seconds.
  bool WaitForService();

  // This class provides the link to the dbus-c++ generated proxy.
  class Proxy : public org::chromium::Chaps_proxy,
                public DBus::ObjectProxy {
  public:
    Proxy(DBus::Connection &connection,
          const char* path,
          const char* service) : ObjectProxy(connection, path, service) {}
    virtual ~Proxy() {}

  private:
    DISALLOW_COPY_AND_ASSIGN(Proxy);
  };

  scoped_ptr<Proxy> proxy_;
  // TODO(dkrahn): Once crosbug.com/35421 has been fixed this lock must be
  // removed.  Currently this is needed to avoid flooding the chapsd dbus
  // dispatcher which seems to drop requests under pressure.
  base::Lock lock_;

  DISALLOW_COPY_AND_ASSIGN(ChapsProxyImpl);
};

}  // namespace

#endif  // CHAPS_CHAPS_PROXY_H
