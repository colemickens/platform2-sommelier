// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHAPS_CHAPS_ADAPTOR_H
#define CHAPS_CHAPS_ADAPTOR_H

#include <base/basictypes.h>

#include "chaps/chaps_adaptor_generated.h"

namespace chaps {

class ChapsInterface;

// The ChapsAdaptor class implements the dbus-c++ generated adaptor interface
// and redirects IPC calls to a ChapsInterface instance.  All dbus-c++ specific
// logic, error handling, etc. is implemented here.  Specifically, the
// ChapsInterface instance need not be aware of dbus-c++ or IPC.  This class
// exists because we don't want to couple dbus-c++ with the Chaps service
// implementation.
class ChapsAdaptor : public org::chromium::Chaps_adaptor,
                     public DBus::ObjectAdaptor {
public:
  ChapsAdaptor(ChapsInterface* service);
  virtual ~ChapsAdaptor();

  virtual void GetSlotList(const bool& token_present,
                           std::vector<uint32_t>& slot_list,
                           uint32_t& result,
                           ::DBus::Error& error);
  virtual void GetSlotInfo(const uint32_t& slot_id,
                           std::string& slot_description,
                           std::string& manufacturer_id,
                           uint32_t& flags,
                           uint8_t& hardware_version_major,
                           uint8_t& hardware_version_minor,
                           uint8_t& firmware_version_major,
                           uint8_t& firmware_version_minor,
                           uint32_t& result,
                           ::DBus::Error& error);
  virtual void GetTokenInfo(const uint32_t& slot_id,
                            std::string& label,
                            std::string& manufacturer_id,
                            std::string& model,
                            std::string& serial_number,
                            uint32_t& flags,
                            uint32_t& max_session_count,
                            uint32_t& session_count,
                            uint32_t& max_session_count_rw,
                            uint32_t& session_count_rw,
                            uint32_t& max_pin_len,
                            uint32_t& min_pin_len,
                            uint32_t& total_public_memory,
                            uint32_t& free_public_memory,
                            uint32_t& total_private_memory,
                            uint32_t& free_private_memory,
                            uint8_t& hardware_version_major,
                            uint8_t& hardware_version_minor,
                            uint8_t& firmware_version_major,
                            uint8_t& firmware_version_minor,
                            uint32_t& result,
                            ::DBus::Error& error);
  virtual void GetMechanismList(const uint32_t& slot_id,
                                std::vector<uint32_t>& mechanism_list,
                                uint32_t& result,
                                ::DBus::Error& error);
  virtual void GetMechanismInfo(const uint32_t& slot_id,
                                const uint32_t& mechanism_type,
                                uint32_t& min_key_size,
                                uint32_t& max_key_size,
                                uint32_t& flags,
                                uint32_t& result,
                                ::DBus::Error& error);
  virtual uint32_t InitToken(const uint32_t& slot_id,
                             const bool& use_null_pin,
                             const std::string& optional_so_pin,
                             const std::string& new_token_label,
                             ::DBus::Error& error);
  virtual uint32_t InitPIN(const uint32_t& session_id,
                           const bool& use_null_pin,
                           const std::string& optional_user_pin,
                           ::DBus::Error& error);
  virtual uint32_t SetPIN(const uint32_t& session_id,
                          const bool& use_null_old_pin,
                          const std::string& optional_old_pin,
                          const bool& use_null_new_pin,
                          const std::string& optional_new_pin,
                          ::DBus::Error& error);
  virtual void OpenSession(const uint32_t& slot_id, const uint32_t& flags,
                           uint32_t& session_id, uint32_t& result,
                           ::DBus::Error& error);
  virtual uint32_t CloseSession(const uint32_t& session_id,
                                ::DBus::Error& error);
  virtual uint32_t CloseAllSessions(const uint32_t& slot_id,
                                    ::DBus::Error& error);
  virtual void GetSessionInfo(const uint32_t& session_id,
                              uint32_t& slot_id,
                              uint32_t& state,
                              uint32_t& flags,
                              uint32_t& device_error,
                              uint32_t& result,
                              ::DBus::Error& error);
  virtual void GetOperationState(const uint32_t& session_id,
                                 std::vector<uint8_t>& operation_state,
                                 uint32_t& result,
                                 ::DBus::Error& error);
  virtual uint32_t SetOperationState(
      const uint32_t& session_id,
      const std::vector<uint8_t>& operation_state,
      const uint32_t& encryption_key_handle,
      const uint32_t& authentication_key_handle,
      ::DBus::Error& error);
  virtual uint32_t Login(const uint32_t& session_id,
                         const uint32_t& user_type,
                         const bool& use_null_pin,
                         const std::string& optional_pin,
                         ::DBus::Error& error);
  virtual uint32_t Logout(const uint32_t& session_id, ::DBus::Error& error);
  virtual void CreateObject(
      const uint32_t& session_id,
      const std::vector<uint8_t>& attributes,
      uint32_t& new_object_handle,
      uint32_t& result,
      ::DBus::Error& error);
  virtual void CopyObject(
      const uint32_t& session_id,
      const uint32_t& object_handle,
      const std::vector<uint8_t>& attributes,
      uint32_t& new_object_handle,
      uint32_t& result,
      ::DBus::Error& error);
  virtual uint32_t DestroyObject(const uint32_t& session_id,
                                 const uint32_t& object_handle,
                                 ::DBus::Error& error);
  virtual void GetObjectSize(const uint32_t& session_id,
                             const uint32_t& object_handle,
                             uint32_t& object_size,
                             uint32_t& result,
                             ::DBus::Error& error);
  virtual void GetAttributeValue(const uint32_t& session_id,
                                 const uint32_t& object_handle,
                                 const std::vector<uint8_t>& attributes_in,
                                 std::vector<uint8_t>& attributes_out,
                                 uint32_t& result,
                                 ::DBus::Error& error);
  virtual uint32_t SetAttributeValue(const uint32_t& session_id,
                                     const uint32_t& object_handle,
                                     const std::vector<uint8_t>& attributes,
                                     ::DBus::Error& error);
  virtual uint32_t FindObjectsInit(const uint32_t& session_id,
                                   const std::vector<uint8_t>& attributes,
                                   ::DBus::Error& error);
  virtual void FindObjects(const uint32_t& session_id,
                           const uint32_t& max_object_count,
                           std::vector<uint32_t>& object_list,
                           uint32_t& result,
                           ::DBus::Error& error);
  virtual uint32_t FindObjectsFinal(const uint32_t& session_id,
                                    ::DBus::Error& error);
  virtual uint32_t EncryptInit(const uint32_t& session_id,
                               const uint32_t& mechanism_type,
                               const std::vector<uint8_t>& mechanism_parameter,
                               const uint32_t& key_handle,
                               ::DBus::Error& error);
  virtual void Encrypt(const uint32_t& session_id,
                       const std::vector<uint8_t>& data_in,
                       const uint32_t& max_out_length,
                       uint32_t& actual_out_length,
                       std::vector<uint8_t>& data_out,
                       uint32_t& result,
                       ::DBus::Error& error);
  virtual void EncryptUpdate(const uint32_t& session_id,
                             const std::vector<uint8_t>& data_in,
                             const uint32_t& max_out_length,
                             uint32_t& actual_out_length,
                             std::vector<uint8_t>& data_out,
                             uint32_t& result, ::DBus::Error& error);
  virtual void EncryptFinal(const uint32_t& session_id,
                            const uint32_t& max_out_length,
                            uint32_t& actual_out_length,
                            std::vector<uint8_t>& data_out,
                            uint32_t& result,
                            ::DBus::Error& error);
  virtual uint32_t DecryptInit(const uint32_t& session_id,
                               const uint32_t& mechanism_type,
                               const std::vector<uint8_t>& mechanism_parameter,
                               const uint32_t& key_handle,
                               ::DBus::Error& error);
  virtual void Decrypt(const uint32_t& session_id,
                       const std::vector<uint8_t>& data_in,
                       const uint32_t& max_out_length,
                       uint32_t& actual_out_length,
                       std::vector<uint8_t>& data_out,
                       uint32_t& result,
                       ::DBus::Error& error);
  virtual void DecryptUpdate(const uint32_t& session_id,
                             const std::vector<uint8_t>& data_in,
                             const uint32_t& max_out_length,
                             uint32_t& actual_out_length,
                             std::vector<uint8_t>& data_out,
                             uint32_t& result,
                             ::DBus::Error& error);
  virtual void DecryptFinal(const uint32_t& session_id,
                            const uint32_t& max_out_length,
                            uint32_t& actual_out_length,
                            std::vector<uint8_t>& data_out,
                            uint32_t& result,
                            ::DBus::Error& error);
  virtual uint32_t DigestInit(const uint32_t& session_id,
                              const uint32_t& mechanism_type,
                              const std::vector<uint8_t>& mechanism_parameter,
                              ::DBus::Error& error);
  virtual void Digest(const uint32_t& session_id,
                      const std::vector<uint8_t>& data_in,
                      const uint32_t& max_out_length,
                      uint32_t& actual_out_length,
                      std::vector<uint8_t>& digest,
                      uint32_t& result,
                      ::DBus::Error& error);
  virtual uint32_t DigestUpdate(const uint32_t& session_id,
                                const std::vector<uint8_t>& data_in,
                                ::DBus::Error& error);
  virtual uint32_t DigestKey(const uint32_t& session_id,
                             const uint32_t& key_handle,
                             ::DBus::Error& error);
  virtual void DigestFinal(const uint32_t& session_id,
                           const uint32_t& max_out_length,
                           uint32_t& actual_out_length,
                           std::vector<uint8_t>& digest,
                           uint32_t& result,
                           ::DBus::Error& error);
  virtual uint32_t SignInit(const uint32_t& session_id,
                            const uint32_t& mechanism_type,
                            const std::vector<uint8_t>& mechanism_parameter,
                            const uint32_t& key_handle,
                            ::DBus::Error& error);
  virtual void Sign(const uint32_t& session_id,
                    const std::vector<uint8_t>& data,
                    const uint32_t& max_out_length,
                    uint32_t& actual_out_length,
                    std::vector<uint8_t>& signature,
                    uint32_t& result,
                    ::DBus::Error& error);
  virtual uint32_t SignUpdate(const uint32_t& session_id,
                              const std::vector<uint8_t>& data_part,
                              ::DBus::Error& error);
  virtual void SignFinal(const uint32_t& session_id,
                         const uint32_t& max_out_length,
                         uint32_t& actual_out_length,
                         std::vector<uint8_t>& signature,
                         uint32_t& result,
                         ::DBus::Error& error);
  virtual uint32_t SignRecoverInit(
      const uint32_t& session_id,
      const uint32_t& mechanism_type,
      const std::vector<uint8_t>& mechanism_parameter,
      const uint32_t& key_handle,
      ::DBus::Error& error);
  virtual void SignRecover(const uint32_t& session_id,
                           const std::vector<uint8_t>& data,
                           const uint32_t& max_out_length,
                           uint32_t& actual_out_length,
                           std::vector<uint8_t>& signature,
                           uint32_t& result,
                           ::DBus::Error& error);
  virtual uint32_t VerifyInit(const uint32_t& session_id,
                              const uint32_t& mechanism_type,
                              const std::vector<uint8_t>& mechanism_parameter,
                              const uint32_t& key_handle,
                              ::DBus::Error& error);
  virtual uint32_t Verify(const uint32_t& session_id,
                          const std::vector<uint8_t>& data,
                          const std::vector<uint8_t>& signature,
                          ::DBus::Error& error);
  virtual uint32_t VerifyUpdate(const uint32_t& session_id,
                                const std::vector<uint8_t>& data_part,
                                ::DBus::Error& error);
  virtual uint32_t VerifyFinal(const uint32_t& session_id,
                               const std::vector<uint8_t>& signature,
                               ::DBus::Error& error);
  virtual uint32_t VerifyRecoverInit(
      const uint32_t& session_id,
      const uint32_t& mechanism_type,
      const std::vector<uint8_t>& mechanism_parameter,
      const uint32_t& key_handle,
      ::DBus::Error& error);
  virtual void VerifyRecover(const uint32_t& session_id,
                             const std::vector<uint8_t>& signature,
                             const uint32_t& max_out_length,
                             uint32_t& actual_out_length,
                             std::vector<uint8_t>& data,
                             uint32_t& result,
                             ::DBus::Error& error);
  virtual void DigestEncryptUpdate(const uint32_t& session_id,
                                   const std::vector<uint8_t>& data_in,
                                   const uint32_t& max_out_length,
                                   uint32_t& actual_out_length,
                                   std::vector<uint8_t>& data_out,
                                   uint32_t& result,
                                   ::DBus::Error& error);
  virtual void DecryptDigestUpdate(const uint32_t& session_id,
                                   const std::vector<uint8_t>& data_in,
                                   const uint32_t& max_out_length,
                                   uint32_t& actual_out_length,
                                   std::vector<uint8_t>& data_out,
                                   uint32_t& result,
                                   ::DBus::Error& error);
  virtual void SignEncryptUpdate(const uint32_t& session_id,
                                 const std::vector<uint8_t>& data_in,
                                 const uint32_t& max_out_length,
                                 uint32_t& actual_out_length,
                                 std::vector<uint8_t>& data_out,
                                 uint32_t& result,
                                 ::DBus::Error& error);
  virtual void DecryptVerifyUpdate(const uint32_t& session_id,
                                   const std::vector<uint8_t>& data_in,
                                   const uint32_t& max_out_length,
                                   uint32_t& actual_out_length,
                                   std::vector<uint8_t>& data_out,
                                   uint32_t& result,
                                   ::DBus::Error& error);
  virtual void GenerateKey(const uint32_t& session_id,
                           const uint32_t& mechanism_type,
                           const std::vector<uint8_t>& mechanism_parameter,
                           const std::vector<uint8_t>& attributes,
                           uint32_t& key_handle,
                           uint32_t& result,
                           ::DBus::Error& error);
  virtual void GenerateKeyPair(
      const uint32_t& session_id,
      const uint32_t& mechanism_type,
      const std::vector<uint8_t>& mechanism_parameter,
      const std::vector<uint8_t>& public_attributes,
      const std::vector<uint8_t>& private_attributes,
      uint32_t& public_key_handle,
      uint32_t& private_key_handle,
      uint32_t& result,
      ::DBus::Error& error);

private:
  ChapsInterface* service_;

  DISALLOW_COPY_AND_ASSIGN(ChapsAdaptor);
};

}  // namespace
#endif  // CHAPS_CHAPS_ADAPTOR_H
