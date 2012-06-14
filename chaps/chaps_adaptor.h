// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHAPS_CHAPS_ADAPTOR_H
#define CHAPS_CHAPS_ADAPTOR_H

#include <base/basictypes.h>

#include "chaps_adaptor_generated.h"

namespace base {
  class Lock;
}

namespace chaps {

class ChapsInterface;
class LoginEventListener;

// The ChapsAdaptor class implements the dbus-c++ generated adaptor interface
// and redirects IPC calls to a ChapsInterface instance.  All dbus-c++ specific
// logic, error handling, etc. is implemented here.  Specifically, the
// ChapsInterface instance need not be aware of dbus-c++ or IPC.  This class
// exists because we don't want to couple dbus-c++ with the Chaps service
// implementation.
class ChapsAdaptor : public org::chromium::Chaps_adaptor,
                     public DBus::ObjectAdaptor {
public:
  ChapsAdaptor(base::Lock* lock,
               ChapsInterface* service,
               LoginEventListener* login_listener);
  virtual ~ChapsAdaptor();

  virtual void OnLogin(const std::string& path,
                       const std::vector<uint8_t>& auth_data,
                       ::DBus::Error &error);
  virtual void OnLogout(const std::string& path, ::DBus::Error &error);
  virtual void OnChangeAuthData(const std::string& path,
                                const std::vector<uint8_t>& old_auth_data,
                                const std::vector<uint8_t>& new_auth_data,
                                ::DBus::Error &error);
  virtual void SetLogLevel(const int32_t& level, ::DBus::Error &error);
  virtual void GetSlotList(const bool& token_present,
                           std::vector<uint64_t>& slot_list,
                           uint32_t& result,
                           ::DBus::Error& error);
  virtual void GetSlotInfo(const uint64_t& slot_id,
                           std::vector<uint8_t>& slot_description,
                           std::vector<uint8_t>& manufacturer_id,
                           uint64_t& flags,
                           uint8_t& hardware_version_major,
                           uint8_t& hardware_version_minor,
                           uint8_t& firmware_version_major,
                           uint8_t& firmware_version_minor,
                           uint32_t& result,
                           ::DBus::Error& error);
  virtual void GetTokenInfo(const uint64_t& slot_id,
                            std::vector<uint8_t>& label,
                            std::vector<uint8_t>& manufacturer_id,
                            std::vector<uint8_t>& model,
                            std::vector<uint8_t>& serial_number,
                            uint64_t& flags,
                            uint64_t& max_session_count,
                            uint64_t& session_count,
                            uint64_t& max_session_count_rw,
                            uint64_t& session_count_rw,
                            uint64_t& max_pin_len,
                            uint64_t& min_pin_len,
                            uint64_t& total_public_memory,
                            uint64_t& free_public_memory,
                            uint64_t& total_private_memory,
                            uint64_t& free_private_memory,
                            uint8_t& hardware_version_major,
                            uint8_t& hardware_version_minor,
                            uint8_t& firmware_version_major,
                            uint8_t& firmware_version_minor,
                            uint32_t& result,
                            ::DBus::Error& error);
  virtual void GetMechanismList(const uint64_t& slot_id,
                                std::vector<uint64_t>& mechanism_list,
                                uint32_t& result,
                                ::DBus::Error& error);
  virtual void GetMechanismInfo(const uint64_t& slot_id,
                                const uint64_t& mechanism_type,
                                uint64_t& min_key_size,
                                uint64_t& max_key_size,
                                uint64_t& flags,
                                uint32_t& result,
                                ::DBus::Error& error);
  virtual uint32_t InitToken(const uint64_t& slot_id,
                             const bool& use_null_pin,
                             const std::string& optional_so_pin,
                             const std::vector<uint8_t>& new_token_label,
                             ::DBus::Error& error);
  virtual uint32_t InitPIN(const uint64_t& session_id,
                           const bool& use_null_pin,
                           const std::string& optional_user_pin,
                           ::DBus::Error& error);
  virtual uint32_t SetPIN(const uint64_t& session_id,
                          const bool& use_null_old_pin,
                          const std::string& optional_old_pin,
                          const bool& use_null_new_pin,
                          const std::string& optional_new_pin,
                          ::DBus::Error& error);
  virtual void OpenSession(const uint64_t& slot_id, const uint64_t& flags,
                           uint64_t& session_id, uint32_t& result,
                           ::DBus::Error& error);
  virtual uint32_t CloseSession(const uint64_t& session_id,
                                ::DBus::Error& error);
  virtual uint32_t CloseAllSessions(const uint64_t& slot_id,
                                    ::DBus::Error& error);
  virtual void GetSessionInfo(const uint64_t& session_id,
                              uint64_t& slot_id,
                              uint64_t& state,
                              uint64_t& flags,
                              uint64_t& device_error,
                              uint32_t& result,
                              ::DBus::Error& error);
  virtual void GetOperationState(const uint64_t& session_id,
                                 std::vector<uint8_t>& operation_state,
                                 uint32_t& result,
                                 ::DBus::Error& error);
  virtual uint32_t SetOperationState(
      const uint64_t& session_id,
      const std::vector<uint8_t>& operation_state,
      const uint64_t& encryption_key_handle,
      const uint64_t& authentication_key_handle,
      ::DBus::Error& error);
  virtual uint32_t Login(const uint64_t& session_id,
                         const uint64_t& user_type,
                         const bool& use_null_pin,
                         const std::string& optional_pin,
                         ::DBus::Error& error);
  virtual uint32_t Logout(const uint64_t& session_id, ::DBus::Error& error);
  virtual void CreateObject(
      const uint64_t& session_id,
      const std::vector<uint8_t>& attributes,
      uint64_t& new_object_handle,
      uint32_t& result,
      ::DBus::Error& error);
  virtual void CopyObject(
      const uint64_t& session_id,
      const uint64_t& object_handle,
      const std::vector<uint8_t>& attributes,
      uint64_t& new_object_handle,
      uint32_t& result,
      ::DBus::Error& error);
  virtual uint32_t DestroyObject(const uint64_t& session_id,
                                 const uint64_t& object_handle,
                                 ::DBus::Error& error);
  virtual void GetObjectSize(const uint64_t& session_id,
                             const uint64_t& object_handle,
                             uint64_t& object_size,
                             uint32_t& result,
                             ::DBus::Error& error);
  virtual void GetAttributeValue(const uint64_t& session_id,
                                 const uint64_t& object_handle,
                                 const std::vector<uint8_t>& attributes_in,
                                 std::vector<uint8_t>& attributes_out,
                                 uint32_t& result,
                                 ::DBus::Error& error);
  virtual uint32_t SetAttributeValue(const uint64_t& session_id,
                                     const uint64_t& object_handle,
                                     const std::vector<uint8_t>& attributes,
                                     ::DBus::Error& error);
  virtual uint32_t FindObjectsInit(const uint64_t& session_id,
                                   const std::vector<uint8_t>& attributes,
                                   ::DBus::Error& error);
  virtual void FindObjects(const uint64_t& session_id,
                           const uint64_t& max_object_count,
                           std::vector<uint64_t>& object_list,
                           uint32_t& result,
                           ::DBus::Error& error);
  virtual uint32_t FindObjectsFinal(const uint64_t& session_id,
                                    ::DBus::Error& error);
  virtual uint32_t EncryptInit(const uint64_t& session_id,
                               const uint64_t& mechanism_type,
                               const std::vector<uint8_t>& mechanism_parameter,
                               const uint64_t& key_handle,
                               ::DBus::Error& error);
  virtual void Encrypt(const uint64_t& session_id,
                       const std::vector<uint8_t>& data_in,
                       const uint64_t& max_out_length,
                       uint64_t& actual_out_length,
                       std::vector<uint8_t>& data_out,
                       uint32_t& result,
                       ::DBus::Error& error);
  virtual void EncryptUpdate(const uint64_t& session_id,
                             const std::vector<uint8_t>& data_in,
                             const uint64_t& max_out_length,
                             uint64_t& actual_out_length,
                             std::vector<uint8_t>& data_out,
                             uint32_t& result, ::DBus::Error& error);
  virtual void EncryptFinal(const uint64_t& session_id,
                            const uint64_t& max_out_length,
                            uint64_t& actual_out_length,
                            std::vector<uint8_t>& data_out,
                            uint32_t& result,
                            ::DBus::Error& error);
  virtual uint32_t DecryptInit(const uint64_t& session_id,
                               const uint64_t& mechanism_type,
                               const std::vector<uint8_t>& mechanism_parameter,
                               const uint64_t& key_handle,
                               ::DBus::Error& error);
  virtual void Decrypt(const uint64_t& session_id,
                       const std::vector<uint8_t>& data_in,
                       const uint64_t& max_out_length,
                       uint64_t& actual_out_length,
                       std::vector<uint8_t>& data_out,
                       uint32_t& result,
                       ::DBus::Error& error);
  virtual void DecryptUpdate(const uint64_t& session_id,
                             const std::vector<uint8_t>& data_in,
                             const uint64_t& max_out_length,
                             uint64_t& actual_out_length,
                             std::vector<uint8_t>& data_out,
                             uint32_t& result,
                             ::DBus::Error& error);
  virtual void DecryptFinal(const uint64_t& session_id,
                            const uint64_t& max_out_length,
                            uint64_t& actual_out_length,
                            std::vector<uint8_t>& data_out,
                            uint32_t& result,
                            ::DBus::Error& error);
  virtual uint32_t DigestInit(const uint64_t& session_id,
                              const uint64_t& mechanism_type,
                              const std::vector<uint8_t>& mechanism_parameter,
                              ::DBus::Error& error);
  virtual void Digest(const uint64_t& session_id,
                      const std::vector<uint8_t>& data_in,
                      const uint64_t& max_out_length,
                      uint64_t& actual_out_length,
                      std::vector<uint8_t>& digest,
                      uint32_t& result,
                      ::DBus::Error& error);
  virtual uint32_t DigestUpdate(const uint64_t& session_id,
                                const std::vector<uint8_t>& data_in,
                                ::DBus::Error& error);
  virtual uint32_t DigestKey(const uint64_t& session_id,
                             const uint64_t& key_handle,
                             ::DBus::Error& error);
  virtual void DigestFinal(const uint64_t& session_id,
                           const uint64_t& max_out_length,
                           uint64_t& actual_out_length,
                           std::vector<uint8_t>& digest,
                           uint32_t& result,
                           ::DBus::Error& error);
  virtual uint32_t SignInit(const uint64_t& session_id,
                            const uint64_t& mechanism_type,
                            const std::vector<uint8_t>& mechanism_parameter,
                            const uint64_t& key_handle,
                            ::DBus::Error& error);
  virtual void Sign(const uint64_t& session_id,
                    const std::vector<uint8_t>& data,
                    const uint64_t& max_out_length,
                    uint64_t& actual_out_length,
                    std::vector<uint8_t>& signature,
                    uint32_t& result,
                    ::DBus::Error& error);
  virtual uint32_t SignUpdate(const uint64_t& session_id,
                              const std::vector<uint8_t>& data_part,
                              ::DBus::Error& error);
  virtual void SignFinal(const uint64_t& session_id,
                         const uint64_t& max_out_length,
                         uint64_t& actual_out_length,
                         std::vector<uint8_t>& signature,
                         uint32_t& result,
                         ::DBus::Error& error);
  virtual uint32_t SignRecoverInit(
      const uint64_t& session_id,
      const uint64_t& mechanism_type,
      const std::vector<uint8_t>& mechanism_parameter,
      const uint64_t& key_handle,
      ::DBus::Error& error);
  virtual void SignRecover(const uint64_t& session_id,
                           const std::vector<uint8_t>& data,
                           const uint64_t& max_out_length,
                           uint64_t& actual_out_length,
                           std::vector<uint8_t>& signature,
                           uint32_t& result,
                           ::DBus::Error& error);
  virtual uint32_t VerifyInit(const uint64_t& session_id,
                              const uint64_t& mechanism_type,
                              const std::vector<uint8_t>& mechanism_parameter,
                              const uint64_t& key_handle,
                              ::DBus::Error& error);
  virtual uint32_t Verify(const uint64_t& session_id,
                          const std::vector<uint8_t>& data,
                          const std::vector<uint8_t>& signature,
                          ::DBus::Error& error);
  virtual uint32_t VerifyUpdate(const uint64_t& session_id,
                                const std::vector<uint8_t>& data_part,
                                ::DBus::Error& error);
  virtual uint32_t VerifyFinal(const uint64_t& session_id,
                               const std::vector<uint8_t>& signature,
                               ::DBus::Error& error);
  virtual uint32_t VerifyRecoverInit(
      const uint64_t& session_id,
      const uint64_t& mechanism_type,
      const std::vector<uint8_t>& mechanism_parameter,
      const uint64_t& key_handle,
      ::DBus::Error& error);
  virtual void VerifyRecover(const uint64_t& session_id,
                             const std::vector<uint8_t>& signature,
                             const uint64_t& max_out_length,
                             uint64_t& actual_out_length,
                             std::vector<uint8_t>& data,
                             uint32_t& result,
                             ::DBus::Error& error);
  virtual void DigestEncryptUpdate(const uint64_t& session_id,
                                   const std::vector<uint8_t>& data_in,
                                   const uint64_t& max_out_length,
                                   uint64_t& actual_out_length,
                                   std::vector<uint8_t>& data_out,
                                   uint32_t& result,
                                   ::DBus::Error& error);
  virtual void DecryptDigestUpdate(const uint64_t& session_id,
                                   const std::vector<uint8_t>& data_in,
                                   const uint64_t& max_out_length,
                                   uint64_t& actual_out_length,
                                   std::vector<uint8_t>& data_out,
                                   uint32_t& result,
                                   ::DBus::Error& error);
  virtual void SignEncryptUpdate(const uint64_t& session_id,
                                 const std::vector<uint8_t>& data_in,
                                 const uint64_t& max_out_length,
                                 uint64_t& actual_out_length,
                                 std::vector<uint8_t>& data_out,
                                 uint32_t& result,
                                 ::DBus::Error& error);
  virtual void DecryptVerifyUpdate(const uint64_t& session_id,
                                   const std::vector<uint8_t>& data_in,
                                   const uint64_t& max_out_length,
                                   uint64_t& actual_out_length,
                                   std::vector<uint8_t>& data_out,
                                   uint32_t& result,
                                   ::DBus::Error& error);
  virtual void GenerateKey(const uint64_t& session_id,
                           const uint64_t& mechanism_type,
                           const std::vector<uint8_t>& mechanism_parameter,
                           const std::vector<uint8_t>& attributes,
                           uint64_t& key_handle,
                           uint32_t& result,
                           ::DBus::Error& error);
  virtual void GenerateKeyPair(
      const uint64_t& session_id,
      const uint64_t& mechanism_type,
      const std::vector<uint8_t>& mechanism_parameter,
      const std::vector<uint8_t>& public_attributes,
      const std::vector<uint8_t>& private_attributes,
      uint64_t& public_key_handle,
      uint64_t& private_key_handle,
      uint32_t& result,
      ::DBus::Error& error);
  virtual void WrapKey(const uint64_t& session_id,
                       const uint64_t& mechanism_type,
                       const std::vector<uint8_t>& mechanism_parameter,
                       const uint64_t& wrapping_key_handle,
                       const uint64_t& key_handle,
                       const uint64_t& max_out_length,
                       uint64_t& actual_out_length,
                       std::vector<uint8_t>& wrapped_key,
                       uint32_t& result,
                       ::DBus::Error& error);
  virtual void UnwrapKey(const uint64_t& session_id,
                         const uint64_t& mechanism_type,
                         const std::vector<uint8_t>& mechanism_parameter,
                         const uint64_t& wrapping_key_handle,
                         const std::vector<uint8_t>& wrapped_key,
                         const std::vector<uint8_t>& attributes,
                         uint64_t& key_handle,
                         uint32_t& result,
                         ::DBus::Error& error);
  virtual void DeriveKey(const uint64_t& session_id,
                         const uint64_t& mechanism_type,
                         const std::vector<uint8_t>& mechanism_parameter,
                         const uint64_t& base_key_handle,
                         const std::vector<uint8_t>& attributes,
                         uint64_t& key_handle,
                         uint32_t& result,
                         ::DBus::Error& error);
  virtual uint32_t SeedRandom(const uint64_t& session_id,
                              const std::vector<uint8_t>& seed,
                              ::DBus::Error& error);
  virtual void GenerateRandom(const uint64_t& session_id,
                              const uint64_t& num_bytes,
                              std::vector<uint8_t>& random_data,
                              uint32_t& result,
                              ::DBus::Error& error);

private:
  base::Lock* lock_;
  ChapsInterface* service_;
  LoginEventListener* login_listener_;

  DISALLOW_COPY_AND_ASSIGN(ChapsAdaptor);
};

}  // namespace
#endif  // CHAPS_CHAPS_ADAPTOR_H
