// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#ifndef CHAPS_CHAPS_PROXY_H
#define CHAPS_CHAPS_PROXY_H

#include <base/memory/scoped_ptr.h>

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
  virtual void FireLoginEvent(const std::string& path,
                              const std::vector<uint8_t>& auth_data);
  virtual void FireLogoutEvent(const std::string& path);
  virtual void FireChangeAuthDataEvent(
      const std::string& path,
      const std::vector<uint8_t>& old_auth_data,
      const std::vector<uint8_t>& new_auth_data);
  virtual void SetLogLevel(const int32_t& level);

  // ChapsInterface methods.
  virtual uint32_t GetSlotList(bool token_present,
                               std::vector<uint64_t>* slot_list);
  virtual uint32_t GetSlotInfo(uint64_t slot_id,
                               std::vector<uint8_t>* slot_description,
                               std::vector<uint8_t>* manufacturer_id,
                               uint64_t* flags,
                               uint8_t* hardware_version_major,
                               uint8_t* hardware_version_minor,
                               uint8_t* firmware_version_major,
                               uint8_t* firmware_version_minor);
  virtual uint32_t GetTokenInfo(uint64_t slot_id,
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
  virtual uint32_t GetMechanismList(uint64_t slot_id,
                                    std::vector<uint64_t>* mechanism_list);
  virtual uint32_t GetMechanismInfo(uint64_t slot_id,
                                    uint64_t mechanism_type,
                                    uint64_t* min_key_size,
                                    uint64_t* max_key_size,
                                    uint64_t* flags);
  virtual uint32_t InitToken(uint64_t slot_id,
                             const std::string* so_pin,
                             const std::vector<uint8_t>& label);
  virtual uint32_t InitPIN(uint64_t session_id, const std::string* pin);
  virtual uint32_t SetPIN(uint64_t session_id,
                          const std::string* old_pin,
                          const std::string* new_pin);
  virtual uint32_t OpenSession(uint64_t slot_id, uint64_t flags,
                               uint64_t* session_id);
  virtual uint32_t CloseSession(uint64_t session_id);
  virtual uint32_t CloseAllSessions(uint64_t slot_id);
  virtual uint32_t GetSessionInfo(uint64_t session_id,
                                  uint64_t* slot_id,
                                  uint64_t* state,
                                  uint64_t* flags,
                                  uint64_t* device_error);
  virtual uint32_t GetOperationState(uint64_t session_id,
                                     std::vector<uint8_t>* operation_state);
  virtual uint32_t SetOperationState(
      uint64_t session_id,
      const std::vector<uint8_t>& operation_state,
      uint64_t encryption_key_handle,
      uint64_t authentication_key_handle);
  virtual uint32_t Login(uint64_t session_id,
                         uint64_t user_type,
                         const std::string* pin);
  virtual uint32_t Logout(uint64_t session_id);
  virtual uint32_t CreateObject(uint64_t session_id,
                                const std::vector<uint8_t>& attributes,
                                uint64_t* new_object_handle);
  virtual uint32_t CopyObject(uint64_t session_id,
                              uint64_t object_handle,
                              const std::vector<uint8_t>& attributes,
                              uint64_t* new_object_handle);
  virtual uint32_t DestroyObject(uint64_t session_id,
                                 uint64_t object_handle);
  virtual uint32_t GetObjectSize(uint64_t session_id,
                                 uint64_t object_handle,
                                 uint64_t* object_size);
  virtual uint32_t GetAttributeValue(uint64_t session_id,
                                     uint64_t object_handle,
                                     const std::vector<uint8_t>& attributes_in,
                                     std::vector<uint8_t>* attributes_out);
  virtual uint32_t SetAttributeValue(uint64_t session_id,
                                     uint64_t object_handle,
                                     const std::vector<uint8_t>& attributes);
  virtual uint32_t FindObjectsInit(uint64_t session_id,
                                   const std::vector<uint8_t>& attributes);
  virtual uint32_t FindObjects(uint64_t session_id,
                               uint64_t max_object_count,
                               std::vector<uint64_t>* object_list);
  virtual uint32_t FindObjectsFinal(uint64_t session_id);
  virtual uint32_t EncryptInit(uint64_t session_id,
                               uint64_t mechanism_type,
                               const std::vector<uint8_t>& mechanism_parameter,
                               uint64_t key_handle);
  virtual uint32_t Encrypt(uint64_t session_id,
                           const std::vector<uint8_t>& data_in,
                           uint64_t max_out_length,
                           uint64_t* actual_out_length,
                           std::vector<uint8_t>* data_out);
  virtual uint32_t EncryptUpdate(uint64_t session_id,
                                 const std::vector<uint8_t>& data_in,
                                 uint64_t max_out_length,
                                 uint64_t* actual_out_length,
                                 std::vector<uint8_t>* data_out);
  virtual uint32_t EncryptFinal(uint64_t session_id,
                                uint64_t max_out_length,
                                uint64_t* actual_out_length,
                                std::vector<uint8_t>* data_out);
  virtual uint32_t DecryptInit(uint64_t session_id,
                               uint64_t mechanism_type,
                               const std::vector<uint8_t>& mechanism_parameter,
                               uint64_t key_handle);
  virtual uint32_t Decrypt(uint64_t session_id,
                           const std::vector<uint8_t>& data_in,
                           uint64_t max_out_length,
                           uint64_t* actual_out_length,
                           std::vector<uint8_t>* data_out);
  virtual uint32_t DecryptUpdate(uint64_t session_id,
                                 const std::vector<uint8_t>& data_in,
                                 uint64_t max_out_length,
                                 uint64_t* actual_out_length,
                                 std::vector<uint8_t>* data_out);
  virtual uint32_t DecryptFinal(uint64_t session_id,
                                uint64_t max_out_length,
                                uint64_t* actual_out_length,
                                std::vector<uint8_t>* data_out);
  virtual uint32_t DigestInit(uint64_t session_id,
                              uint64_t mechanism_type,
                              const std::vector<uint8_t>& mechanism_parameter);
  virtual uint32_t Digest(uint64_t session_id,
                          const std::vector<uint8_t>& data_in,
                          uint64_t max_out_length,
                          uint64_t* actual_out_length,
                          std::vector<uint8_t>* digest);
  virtual uint32_t DigestUpdate(uint64_t session_id,
                                const std::vector<uint8_t>& data_in);
  virtual uint32_t DigestKey(uint64_t session_id,
                             uint64_t key_handle);
  virtual uint32_t DigestFinal(uint64_t session_id,
                               uint64_t max_out_length,
                               uint64_t* actual_out_length,
                               std::vector<uint8_t>* digest);
  virtual uint32_t SignInit(uint64_t session_id,
                            uint64_t mechanism_type,
                            const std::vector<uint8_t>& mechanism_parameter,
                            uint64_t key_handle);
  virtual uint32_t Sign(uint64_t session_id,
                        const std::vector<uint8_t>& data,
                        uint64_t max_out_length,
                        uint64_t* actual_out_length,
                        std::vector<uint8_t>* signature);
  virtual uint32_t SignUpdate(uint64_t session_id,
                              const std::vector<uint8_t>& data_part);
  virtual uint32_t SignFinal(uint64_t session_id,
                             uint64_t max_out_length,
                             uint64_t* actual_out_length,
                             std::vector<uint8_t>* signature);
  virtual uint32_t SignRecoverInit(
      uint64_t session_id,
      uint64_t mechanism_type,
      const std::vector<uint8_t>& mechanism_parameter,
      uint64_t key_handle);
  virtual uint32_t SignRecover(uint64_t session_id,
                               const std::vector<uint8_t>& data,
                               uint64_t max_out_length,
                               uint64_t* actual_out_length,
                               std::vector<uint8_t>* signature);
  virtual uint32_t VerifyInit(uint64_t session_id,
                              uint64_t mechanism_type,
                              const std::vector<uint8_t>& mechanism_parameter,
                              uint64_t key_handle);
  virtual uint32_t Verify(uint64_t session_id,
                          const std::vector<uint8_t>& data,
                          const std::vector<uint8_t>& signature);
  virtual uint32_t VerifyUpdate(uint64_t session_id,
                                const std::vector<uint8_t>& data_part);
  virtual uint32_t VerifyFinal(uint64_t session_id,
                               const std::vector<uint8_t>& signature);
  virtual uint32_t VerifyRecoverInit(
      uint64_t session_id,
      uint64_t mechanism_type,
      const std::vector<uint8_t>& mechanism_parameter,
      uint64_t key_handle);
  virtual uint32_t VerifyRecover(uint64_t session_id,
                                 const std::vector<uint8_t>& signature,
                                 uint64_t max_out_length,
                                 uint64_t* actual_out_length,
                                 std::vector<uint8_t>* data);
  virtual uint32_t DigestEncryptUpdate(uint64_t session_id,
                                       const std::vector<uint8_t>& data_in,
                                       uint64_t max_out_length,
                                       uint64_t* actual_out_length,
                                       std::vector<uint8_t>* data_out);
  virtual uint32_t DecryptDigestUpdate(uint64_t session_id,
                                       const std::vector<uint8_t>& data_in,
                                       uint64_t max_out_length,
                                       uint64_t* actual_out_length,
                                       std::vector<uint8_t>* data_out);
  virtual uint32_t SignEncryptUpdate(uint64_t session_id,
                                     const std::vector<uint8_t>& data_in,
                                     uint64_t max_out_length,
                                     uint64_t* actual_out_length,
                                     std::vector<uint8_t>* data_out);
  virtual uint32_t DecryptVerifyUpdate(uint64_t session_id,
                                       const std::vector<uint8_t>& data_in,
                                       uint64_t max_out_length,
                                       uint64_t* actual_out_length,
                                       std::vector<uint8_t>* data_out);
  virtual uint32_t GenerateKey(uint64_t session_id,
                               uint64_t mechanism_type,
                               const std::vector<uint8_t>& mechanism_parameter,
                               const std::vector<uint8_t>& attributes,
                               uint64_t* key_handle);
  virtual uint32_t GenerateKeyPair(
      uint64_t session_id,
      uint64_t mechanism_type,
      const std::vector<uint8_t>& mechanism_parameter,
      const std::vector<uint8_t>& public_attributes,
      const std::vector<uint8_t>& private_attributes,
      uint64_t* public_key_handle,
      uint64_t* private_key_handle);
  virtual uint32_t WrapKey(uint64_t session_id,
                           uint64_t mechanism_type,
                           const std::vector<uint8_t>& mechanism_parameter,
                           uint64_t wrapping_key_handle,
                           uint64_t key_handle,
                           uint64_t max_out_length,
                           uint64_t* actual_out_length,
                           std::vector<uint8_t>* wrapped_key);
  virtual uint32_t UnwrapKey(uint64_t session_id,
                             uint64_t mechanism_type,
                             const std::vector<uint8_t>& mechanism_parameter,
                             uint64_t wrapping_key_handle,
                             const std::vector<uint8_t>& wrapped_key,
                             const std::vector<uint8_t>& attributes,
                             uint64_t* key_handle);
  virtual uint32_t DeriveKey(uint64_t session_id,
                             uint64_t mechanism_type,
                             const std::vector<uint8_t>& mechanism_parameter,
                             uint64_t base_key_handle,
                             const std::vector<uint8_t>& attributes,
                             uint64_t* key_handle);
  virtual uint32_t SeedRandom(uint64_t session_id,
                              const std::vector<uint8_t>& seed);
  virtual uint32_t GenerateRandom(uint64_t session_id,
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

  DISALLOW_COPY_AND_ASSIGN(ChapsProxyImpl);
};

}  // namespace

#endif  // CHAPS_CHAPS_PROXY_H
