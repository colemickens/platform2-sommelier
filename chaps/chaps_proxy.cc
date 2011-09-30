// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chaps/chaps_proxy.h"

#include <base/logging.h>

#include "chaps/chaps.h"
#include "chaps/chaps_utility.h"
#include "pkcs11/cryptoki.h"

using std::string;
using std::vector;

namespace chaps {

ChapsProxyImpl::ChapsProxyImpl() {}

ChapsProxyImpl::~ChapsProxyImpl() {}

bool ChapsProxyImpl::Init() {
  try {
    if (!DBus::default_dispatcher)
      DBus::default_dispatcher = new DBus::BusDispatcher();
    if (!proxy_.get()) {
      // Establish a D-Bus connection.
      DBus::Connection connection = DBus::Connection::SystemBus();
      proxy_.reset(new Proxy(connection, kChapsServicePath,
                             kChapsServiceName));
    }
    if (proxy_.get())
      return true;
  } catch (DBus::Error err) {
    LOG(ERROR) << "DBus::Error - " << err.what();
  }
  return false;
}

uint32_t ChapsProxyImpl::GetSlotList(bool token_present,
                                     vector<uint32_t>* slot_list) {
  LOG_CK_RV_AND_RETURN_IF(!proxy_.get(), CKR_CRYPTOKI_NOT_INITIALIZED);
  LOG_CK_RV_AND_RETURN_IF(!slot_list, CKR_ARGUMENTS_BAD);

  uint32_t result = CKR_OK;
  try {
    proxy_->GetSlotList(token_present, *slot_list, result);
  } catch (DBus::Error err) {
    result = CKR_GENERAL_ERROR;
    LOG(ERROR) << "DBus::Error - " << err.what();
  }
  return result;
}

uint32_t ChapsProxyImpl::GetSlotInfo(uint32_t slot_id,
                                     string* slot_description,
                                     string* manufacturer_id,
                                     uint32_t* flags,
                                     uint8_t* hardware_version_major,
                                     uint8_t* hardware_version_minor,
                                     uint8_t* firmware_version_major,
                                     uint8_t* firmware_version_minor) {
  LOG_CK_RV_AND_RETURN_IF(!proxy_.get(), CKR_CRYPTOKI_NOT_INITIALIZED);
  if (!slot_description || !manufacturer_id || !flags ||
      !hardware_version_major || !hardware_version_minor ||
      !firmware_version_major || !firmware_version_minor)
    LOG_CK_RV_AND_RETURN(CKR_ARGUMENTS_BAD);

  uint32_t result = CKR_OK;
  try {
    proxy_->GetSlotInfo(slot_id,
                        *slot_description,
                        *manufacturer_id,
                        *flags,
                        *hardware_version_major,
                        *hardware_version_minor,
                        *firmware_version_major,
                        *firmware_version_minor,
                        result);
  } catch (DBus::Error err) {
    result = CKR_GENERAL_ERROR;
    LOG(ERROR) << "DBus::Error - " << err.what();
  }
  return result;
}

uint32_t ChapsProxyImpl::GetTokenInfo(uint32_t slot_id,
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
                                      uint8_t* firmware_version_minor) {
  LOG_CK_RV_AND_RETURN_IF(!proxy_.get(), CKR_CRYPTOKI_NOT_INITIALIZED);
  if (!label || !manufacturer_id || !model || !serial_number || !flags ||
      !max_session_count || !session_count || !max_session_count_rw ||
      !session_count_rw || !max_pin_len || !min_pin_len ||
      !total_public_memory || !free_public_memory || !total_private_memory ||
      !total_public_memory ||
      !hardware_version_major || !hardware_version_minor ||
      !firmware_version_major || !firmware_version_minor)
    LOG_CK_RV_AND_RETURN(CKR_ARGUMENTS_BAD);

  uint32_t result = CKR_OK;
  try {
    proxy_->GetTokenInfo(slot_id,
                         *label,
                         *manufacturer_id,
                         *model,
                         *serial_number,
                         *flags,
                         *max_session_count,
                         *session_count,
                         *max_session_count_rw,
                         *session_count_rw,
                         *max_pin_len,
                         *min_pin_len,
                         *total_public_memory,
                         *free_public_memory,
                         *total_private_memory,
                         *free_private_memory,
                         *hardware_version_major,
                         *hardware_version_minor,
                         *firmware_version_major,
                         *firmware_version_minor,
                         result);
  } catch (DBus::Error err) {
    result = CKR_GENERAL_ERROR;
    LOG(ERROR) << "DBus::Error - " << err.what();
  }
  return result;
}

uint32_t ChapsProxyImpl::GetMechanismList(
    uint32_t slot_id,
    std::vector<uint32_t>* mechanism_list) {
  LOG_CK_RV_AND_RETURN_IF(!proxy_.get(), CKR_CRYPTOKI_NOT_INITIALIZED);
  LOG_CK_RV_AND_RETURN_IF(!mechanism_list, CKR_ARGUMENTS_BAD);

  uint32_t result = CKR_OK;
  try {
    proxy_->GetMechanismList(slot_id, *mechanism_list, result);
  } catch (DBus::Error err) {
    result = CKR_GENERAL_ERROR;
    LOG(ERROR) << "DBus::Error - " << err.what();
  }
  return result;
}

uint32_t ChapsProxyImpl::GetMechanismInfo(uint32_t slot_id,
                                          uint32_t mechanism_type,
                                          uint32_t* min_key_size,
                                          uint32_t* max_key_size,
                                          uint32_t* flags) {
  LOG_CK_RV_AND_RETURN_IF(!proxy_.get(), CKR_CRYPTOKI_NOT_INITIALIZED);
  if (!min_key_size || !max_key_size || !flags)
    LOG_CK_RV_AND_RETURN(CKR_ARGUMENTS_BAD);

  uint32_t result = CKR_OK;
  try {
    proxy_->GetMechanismInfo(slot_id,
                             mechanism_type,
                             *min_key_size,
                             *max_key_size,
                             *flags,
                             result);
  } catch (DBus::Error err) {
    result = CKR_GENERAL_ERROR;
    LOG(ERROR) << "DBus::Error - " << err.what();
  }
  return result;
}

}  // namespace
