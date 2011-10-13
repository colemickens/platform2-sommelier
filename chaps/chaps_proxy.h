// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#ifndef CHAPS_CHAPS_PROXY_H
#define CHAPS_CHAPS_PROXY_H

#include <base/scoped_ptr.h>

#include "chaps/chaps_interface.h"
#include "chaps/chaps_proxy_generated.h"

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
  bool Init();
  // ChapsInterface methods.
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
                                     std::string* operation_state);
  virtual uint32_t SetOperationState(
      uint32_t session_id,
      const std::string& operation_state,
      uint32_t encryption_key_handle,
      uint32_t authentication_key_handle);
  virtual uint32_t Login(uint32_t session_id,
                         uint32_t user_type,
                         const std::string* pin);
  virtual uint32_t Logout(uint32_t session_id);
  virtual uint32_t CreateObject(uint32_t session_id,
                                const std::string& attributes,
                                uint32_t* new_object_handle);
  virtual uint32_t CopyObject(uint32_t session_id,
                              uint32_t object_handle,
                              const std::string& attributes,
                              uint32_t* new_object_handle);
  virtual uint32_t DestroyObject(uint32_t session_id,
                                 uint32_t object_handle);
  virtual uint32_t GetObjectSize(uint32_t session_id,
                                 uint32_t object_handle,
                                 uint32_t* object_size);
  virtual uint32_t GetAttributeValue(uint32_t session_id,
                                     uint32_t object_handle,
                                     const std::string& attributes_in,
                                     std::string* attributes_out);
  virtual uint32_t SetAttributeValue(uint32_t session_id,
                                     uint32_t object_handle,
                                     const std::string& attributes);
  virtual uint32_t FindObjectsInit(uint32_t session_id,
                                   const std::string& attributes);
  virtual uint32_t FindObjects(uint32_t session_id,
                               uint32_t max_object_count,
                               std::vector<uint32_t>* object_list);
  virtual uint32_t FindObjectsFinal(uint32_t session_id);

private:
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
