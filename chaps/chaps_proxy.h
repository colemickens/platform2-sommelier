// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#ifndef CHAPS_CHAPS_PROXY_H_
#define CHAPS_CHAPS_PROXY_H_

#include <memory>
#include <string>
#include <vector>

#include <base/synchronization/lock.h>
#include <brillo/secure_blob.h>

#include "chaps/chaps_interface.h"
#include "chaps/dbus_proxies/org.chromium.Chaps.h"

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

  virtual bool OpenIsolate(brillo::SecureBlob* isolate_credential,
                           bool* new_isolate_created);
  virtual void CloseIsolate(const brillo::SecureBlob& isolate_credential);
  virtual bool LoadToken(const brillo::SecureBlob& isolate_credential,
                         const std::string& path,
                         const std::vector<uint8_t>& auth_data,
                         const std::string& label,
                         uint64_t* slot_id);
  virtual void UnloadToken(const brillo::SecureBlob& isolate_credential,
                           const std::string& path);
  virtual void ChangeTokenAuthData(const std::string& path,
                                   const std::vector<uint8_t>& old_auth_data,
                                   const std::vector<uint8_t>& new_auth_data);
  virtual bool GetTokenPath(const brillo::SecureBlob& isolate_credential,
                            uint64_t slot_id,
                            std::string* path);

  virtual void SetLogLevel(const int32_t& level);

  // ChapsInterface methods.
  virtual uint32_t GetSlotList(const brillo::SecureBlob& isolate_credential,
                               bool token_present,
                               std::vector<uint64_t>* slot_list);
  virtual uint32_t GetSlotInfo(const brillo::SecureBlob& isolate_credential,
                               uint64_t slot_id,
                               SlotInfo* slot_info);
  virtual uint32_t GetTokenInfo(const brillo::SecureBlob& isolate_credential,
                                uint64_t slot_id,
                                TokenInfo* token_info);
  virtual uint32_t GetMechanismList(
      const brillo::SecureBlob& isolate_credential,
      uint64_t slot_id,
      std::vector<uint64_t>* mechanism_list);
  virtual uint32_t GetMechanismInfo(
      const brillo::SecureBlob& isolate_credential,
      uint64_t slot_id,
      uint64_t mechanism_type,
      MechanismInfo* mechanism_info);
  virtual uint32_t InitToken(const brillo::SecureBlob& isolate_credential,
                             uint64_t slot_id,
                             const std::string* so_pin,
                             const std::vector<uint8_t>& label);
  virtual uint32_t InitPIN(const brillo::SecureBlob& isolate_credential,
                           uint64_t session_id, const std::string* pin);
  virtual uint32_t SetPIN(const brillo::SecureBlob& isolate_credential,
                          uint64_t session_id,
                          const std::string* old_pin,
                          const std::string* new_pin);
  virtual uint32_t OpenSession(const brillo::SecureBlob& isolate_credential,
                               uint64_t slot_id, uint64_t flags,
                               uint64_t* session_id);
  virtual uint32_t CloseSession(const brillo::SecureBlob& isolate_credential,
                                uint64_t session_id);
  virtual uint32_t CloseAllSessions(
      const brillo::SecureBlob& isolate_credential,
      uint64_t slot_id);
  virtual uint32_t GetSessionInfo(
      const brillo::SecureBlob& isolate_credential,
      uint64_t session_id,
      SessionInfo* session_info);
  virtual uint32_t GetOperationState(
      const brillo::SecureBlob& isolate_credential,
      uint64_t session_id,
      std::vector<uint8_t>* operation_state);
  virtual uint32_t SetOperationState(
      const brillo::SecureBlob& isolate_credential,
      uint64_t session_id,
      const std::vector<uint8_t>& operation_state,
      uint64_t encryption_key_handle,
      uint64_t authentication_key_handle);
  virtual uint32_t Login(const brillo::SecureBlob& isolate_credential,
                         uint64_t session_id,
                         uint64_t user_type,
                         const std::string* pin);
  virtual uint32_t Logout(const brillo::SecureBlob& isolate_credential,
                          uint64_t session_id);
  virtual uint32_t CreateObject(const brillo::SecureBlob& isolate_credential,
                                uint64_t session_id,
                                const std::vector<uint8_t>& attributes,
                                uint64_t* new_object_handle);
  virtual uint32_t CopyObject(const brillo::SecureBlob& isolate_credential,
                              uint64_t session_id,
                              uint64_t object_handle,
                              const std::vector<uint8_t>& attributes,
                              uint64_t* new_object_handle);
  virtual uint32_t DestroyObject(const brillo::SecureBlob& isolate_credential,
                                 uint64_t session_id,
                                 uint64_t object_handle);
  virtual uint32_t GetObjectSize(const brillo::SecureBlob& isolate_credential,
                                 uint64_t session_id,
                                 uint64_t object_handle,
                                 uint64_t* object_size);
  virtual uint32_t GetAttributeValue(
      const brillo::SecureBlob& isolate_credential,
      uint64_t session_id,
      uint64_t object_handle,
      const std::vector<uint8_t>& attributes_in,
      std::vector<uint8_t>* attributes_out);
  virtual uint32_t SetAttributeValue(
      const brillo::SecureBlob& isolate_credential,
      uint64_t session_id,
      uint64_t object_handle,
      const std::vector<uint8_t>& attributes);
  virtual uint32_t FindObjectsInit(
      const brillo::SecureBlob& isolate_credential,
      uint64_t session_id,
      const std::vector<uint8_t>& attributes);
  virtual uint32_t FindObjects(
      const brillo::SecureBlob& isolate_credential,
      uint64_t session_id,
      uint64_t max_object_count,
      std::vector<uint64_t>* object_list);
  virtual uint32_t FindObjectsFinal(
      const brillo::SecureBlob& isolate_credential,
      uint64_t session_id);
  virtual uint32_t EncryptInit(const brillo::SecureBlob& isolate_credential,
                               uint64_t session_id,
                               uint64_t mechanism_type,
                               const std::vector<uint8_t>& mechanism_parameter,
                               uint64_t key_handle);
  virtual uint32_t Encrypt(const brillo::SecureBlob& isolate_credential,
                           uint64_t session_id,
                           const std::vector<uint8_t>& data_in,
                           uint64_t max_out_length,
                           uint64_t* actual_out_length,
                           std::vector<uint8_t>* data_out);
  virtual uint32_t EncryptUpdate(const brillo::SecureBlob& isolate_credential,
                                 uint64_t session_id,
                                 const std::vector<uint8_t>& data_in,
                                 uint64_t max_out_length,
                                 uint64_t* actual_out_length,
                                 std::vector<uint8_t>* data_out);
  virtual uint32_t EncryptFinal(const brillo::SecureBlob& isolate_credential,
                                uint64_t session_id,
                                uint64_t max_out_length,
                                uint64_t* actual_out_length,
                                std::vector<uint8_t>* data_out);
  virtual void EncryptCancel(const brillo::SecureBlob& isolate_credential,
                             uint64_t session_id);
  virtual uint32_t DecryptInit(const brillo::SecureBlob& isolate_credential,
                               uint64_t session_id,
                               uint64_t mechanism_type,
                               const std::vector<uint8_t>& mechanism_parameter,
                               uint64_t key_handle);
  virtual uint32_t Decrypt(const brillo::SecureBlob& isolate_credential,
                           uint64_t session_id,
                           const std::vector<uint8_t>& data_in,
                           uint64_t max_out_length,
                           uint64_t* actual_out_length,
                           std::vector<uint8_t>* data_out);
  virtual uint32_t DecryptUpdate(const brillo::SecureBlob& isolate_credential,
                                 uint64_t session_id,
                                 const std::vector<uint8_t>& data_in,
                                 uint64_t max_out_length,
                                 uint64_t* actual_out_length,
                                 std::vector<uint8_t>* data_out);
  virtual uint32_t DecryptFinal(const brillo::SecureBlob& isolate_credential,
                                uint64_t session_id,
                                uint64_t max_out_length,
                                uint64_t* actual_out_length,
                                std::vector<uint8_t>* data_out);
  virtual void DecryptCancel(const brillo::SecureBlob& isolate_credential,
                             uint64_t session_id);
  virtual uint32_t DigestInit(const brillo::SecureBlob& isolate_credential,
                              uint64_t session_id,
                              uint64_t mechanism_type,
                              const std::vector<uint8_t>& mechanism_parameter);
  virtual uint32_t Digest(const brillo::SecureBlob& isolate_credential,
                          uint64_t session_id,
                          const std::vector<uint8_t>& data_in,
                          uint64_t max_out_length,
                          uint64_t* actual_out_length,
                          std::vector<uint8_t>* digest);
  virtual uint32_t DigestUpdate(const brillo::SecureBlob& isolate_credential,
                                uint64_t session_id,
                                const std::vector<uint8_t>& data_in);
  virtual uint32_t DigestKey(const brillo::SecureBlob& isolate_credential,
                             uint64_t session_id,
                             uint64_t key_handle);
  virtual uint32_t DigestFinal(const brillo::SecureBlob& isolate_credential,
                               uint64_t session_id,
                               uint64_t max_out_length,
                               uint64_t* actual_out_length,
                               std::vector<uint8_t>* digest);
  virtual void DigestCancel(const brillo::SecureBlob& isolate_credential,
                            uint64_t session_id);
  virtual uint32_t SignInit(const brillo::SecureBlob& isolate_credential,
                            uint64_t session_id,
                            uint64_t mechanism_type,
                            const std::vector<uint8_t>& mechanism_parameter,
                            uint64_t key_handle);
  virtual uint32_t Sign(const brillo::SecureBlob& isolate_credential,
                        uint64_t session_id,
                        const std::vector<uint8_t>& data,
                        uint64_t max_out_length,
                        uint64_t* actual_out_length,
                        std::vector<uint8_t>* signature);
  virtual uint32_t SignUpdate(const brillo::SecureBlob& isolate_credential,
                              uint64_t session_id,
                              const std::vector<uint8_t>& data_part);
  virtual uint32_t SignFinal(const brillo::SecureBlob& isolate_credential,
                             uint64_t session_id,
                             uint64_t max_out_length,
                             uint64_t* actual_out_length,
                             std::vector<uint8_t>* signature);
  virtual void SignCancel(const brillo::SecureBlob& isolate_credential,
                          uint64_t session_id);
  virtual uint32_t SignRecoverInit(
      const brillo::SecureBlob& isolate_credential,
      uint64_t session_id,
      uint64_t mechanism_type,
      const std::vector<uint8_t>& mechanism_parameter,
      uint64_t key_handle);
  virtual uint32_t SignRecover(const brillo::SecureBlob& isolate_credential,
                               uint64_t session_id,
                               const std::vector<uint8_t>& data,
                               uint64_t max_out_length,
                               uint64_t* actual_out_length,
                               std::vector<uint8_t>* signature);
  virtual uint32_t VerifyInit(const brillo::SecureBlob& isolate_credential,
                              uint64_t session_id,
                              uint64_t mechanism_type,
                              const std::vector<uint8_t>& mechanism_parameter,
                              uint64_t key_handle);
  virtual uint32_t Verify(const brillo::SecureBlob& isolate_credential,
                          uint64_t session_id,
                          const std::vector<uint8_t>& data,
                          const std::vector<uint8_t>& signature);
  virtual uint32_t VerifyUpdate(const brillo::SecureBlob& isolate_credential,
                                uint64_t session_id,
                                const std::vector<uint8_t>& data_part);
  virtual uint32_t VerifyFinal(const brillo::SecureBlob& isolate_credential,
                               uint64_t session_id,
                               const std::vector<uint8_t>& signature);
  virtual void VerifyCancel(const brillo::SecureBlob& isolate_credential,
                            uint64_t session_id);
  virtual uint32_t VerifyRecoverInit(
      const brillo::SecureBlob& isolate_credential,
      uint64_t session_id,
      uint64_t mechanism_type,
      const std::vector<uint8_t>& mechanism_parameter,
      uint64_t key_handle);
  virtual uint32_t VerifyRecover(
      const brillo::SecureBlob& isolate_credential,
      uint64_t session_id,
      const std::vector<uint8_t>& signature,
      uint64_t max_out_length,
      uint64_t* actual_out_length,
      std::vector<uint8_t>* data);
  virtual uint32_t DigestEncryptUpdate(
      const brillo::SecureBlob& isolate_credential,
      uint64_t session_id,
      const std::vector<uint8_t>& data_in,
      uint64_t max_out_length,
      uint64_t* actual_out_length,
      std::vector<uint8_t>* data_out);
  virtual uint32_t DecryptDigestUpdate(
      const brillo::SecureBlob& isolate_credential,
      uint64_t session_id,
      const std::vector<uint8_t>& data_in,
      uint64_t max_out_length,
      uint64_t* actual_out_length,
      std::vector<uint8_t>* data_out);
  virtual uint32_t SignEncryptUpdate(
      const brillo::SecureBlob& isolate_credential,
      uint64_t session_id,
      const std::vector<uint8_t>& data_in,
      uint64_t max_out_length,
      uint64_t* actual_out_length,
      std::vector<uint8_t>* data_out);
  virtual uint32_t DecryptVerifyUpdate(
      const brillo::SecureBlob& isolate_credential,
      uint64_t session_id,
      const std::vector<uint8_t>& data_in,
      uint64_t max_out_length,
      uint64_t* actual_out_length,
      std::vector<uint8_t>* data_out);
  virtual uint32_t GenerateKey(const brillo::SecureBlob& isolate_credential,
                               uint64_t session_id,
                               uint64_t mechanism_type,
                               const std::vector<uint8_t>& mechanism_parameter,
                               const std::vector<uint8_t>& attributes,
                               uint64_t* key_handle);
  virtual uint32_t GenerateKeyPair(
      const brillo::SecureBlob& isolate_credential,
      uint64_t session_id,
      uint64_t mechanism_type,
      const std::vector<uint8_t>& mechanism_parameter,
      const std::vector<uint8_t>& public_attributes,
      const std::vector<uint8_t>& private_attributes,
      uint64_t* public_key_handle,
      uint64_t* private_key_handle);
  virtual uint32_t WrapKey(const brillo::SecureBlob& isolate_credential,
                           uint64_t session_id,
                           uint64_t mechanism_type,
                           const std::vector<uint8_t>& mechanism_parameter,
                           uint64_t wrapping_key_handle,
                           uint64_t key_handle,
                           uint64_t max_out_length,
                           uint64_t* actual_out_length,
                           std::vector<uint8_t>* wrapped_key);
  virtual uint32_t UnwrapKey(const brillo::SecureBlob& isolate_credential,
                             uint64_t session_id,
                             uint64_t mechanism_type,
                             const std::vector<uint8_t>& mechanism_parameter,
                             uint64_t wrapping_key_handle,
                             const std::vector<uint8_t>& wrapped_key,
                             const std::vector<uint8_t>& attributes,
                             uint64_t* key_handle);
  virtual uint32_t DeriveKey(const brillo::SecureBlob& isolate_credential,
                             uint64_t session_id,
                             uint64_t mechanism_type,
                             const std::vector<uint8_t>& mechanism_parameter,
                             uint64_t base_key_handle,
                             const std::vector<uint8_t>& attributes,
                             uint64_t* key_handle);
  virtual uint32_t SeedRandom(const brillo::SecureBlob& isolate_credential,
                              uint64_t session_id,
                              const std::vector<uint8_t>& seed);
  virtual uint32_t GenerateRandom(
      const brillo::SecureBlob& isolate_credential,
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
    Proxy(DBus::Connection &connection,  // NOLINT(runtime/references)
          const char* path,
          const char* service) : ObjectProxy(connection, path, service) {}
    virtual ~Proxy() {}

   private:
    DISALLOW_COPY_AND_ASSIGN(Proxy);
  };

  std::unique_ptr<Proxy> proxy_;
  // TODO(dkrahn): Once crosbug.com/35421 has been fixed this lock must be
  // removed.  Currently this is needed to avoid flooding the chapsd dbus
  // dispatcher which seems to drop requests under pressure.
  base::Lock lock_;

  DISALLOW_COPY_AND_ASSIGN(ChapsProxyImpl);
};

}  // namespace chaps

#endif  // CHAPS_CHAPS_PROXY_H_
