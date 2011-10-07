// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#ifndef CHAPS_CHAPS_SERVICE_REDIRECT_H
#define CHAPS_CHAPS_SERVICE_REDIRECT_H

#include "chaps/chaps_interface.h"
#include "pkcs11/cryptoki.h"

namespace chaps {

// ChapsServiceRedirect simply redirects calls to a PKCS #11 library.
class ChapsServiceRedirect : public ChapsInterface {
public:
  explicit ChapsServiceRedirect(const char* library_path);
  virtual ~ChapsServiceRedirect();
  bool Init();
  void TearDown();
  // ChapsInterface methods
  virtual uint32_t GetSlotList(bool token_present,
                               std::vector<uint32_t>* slot_list);
  virtual uint32_t GetSlotInfo(uint32_t slot_id,
                               std::string* slot_description,
                               std::string* manufacturer_id,
                               uint32_t* flags,
                               uint8_t* hardware_version_major,
                               uint8_t* hardware_version_minor,
                               uint8_t* firmware_version_major,
                               uint8_t* firmware_version_minor);
  virtual uint32_t GetTokenInfo(uint32_t slot_id,
                                std::string* label,
                                std::string* manufacturer_id,
                                std::string* model,
                                std::string* serial_number,
                                uint32_t* flags,
                                uint32_t* max_session_count,
                                uint32_t* session_count,
                                uint32_t* max_session_count_rw,
                                uint32_t* session_count_rw,
                                uint32_t* max_pin_len,
                                uint32_t* min_pin_len,
                                uint32_t* total_public_memory,
                                uint32_t* free_public_memory,
                                uint32_t* total_private_memory,
                                uint32_t* free_private_memory,
                                uint8_t* hardware_version_major,
                                uint8_t* hardware_version_minor,
                                uint8_t* firmware_version_major,
                                uint8_t* firmware_version_minor);
  virtual uint32_t GetMechanismList(uint32_t slot_id,
                                    std::vector<uint32_t>* mechanism_list);
  virtual uint32_t GetMechanismInfo(uint32_t slot_id,
                                    uint32_t mechanism_type,
                                    uint32_t* min_key_size,
                                    uint32_t* max_key_size,
                                    uint32_t* flags);
  virtual uint32_t InitToken(uint32_t slot_id, const std::string* so_pin,
                             const std::string& label);
  virtual uint32_t InitPIN(uint32_t session_id, const std::string* pin);
  virtual uint32_t SetPIN(uint32_t session_id, const std::string* old_pin,
                          const std::string* new_pin);
  virtual uint32_t OpenSession(uint32_t slot_id, uint32_t flags,
                               uint32_t* session_id);
  virtual uint32_t CloseSession(uint32_t session_id);
  virtual uint32_t CloseAllSessions(uint32_t slot_id);
  virtual uint32_t GetSessionInfo(uint32_t session_id,
                                  uint32_t* slot_id,
                                  uint32_t* state,
                                  uint32_t* flags,
                                  uint32_t* device_error);
  virtual uint32_t GetOperationState(uint32_t session_id,
                                     std::vector<uint8_t>* operation_state);
  virtual uint32_t SetOperationState(
      uint32_t session_id,
      const std::vector<uint8_t>& operation_state,
      uint32_t encryption_key_handle,
      uint32_t authentication_key_handle);
  virtual uint32_t Login(uint32_t session_id,
                         uint32_t user_type,
                         const std::string* pin);
  virtual uint32_t Logout(uint32_t session_id);
private:
  std::string library_path_;
  void* library_;
  CK_FUNCTION_LIST_PTR functions_;

  DISALLOW_COPY_AND_ASSIGN(ChapsServiceRedirect);
};

}  // namespace
#endif  // CHAPS_CHAPS_SERVICE_REDIRECT_H
