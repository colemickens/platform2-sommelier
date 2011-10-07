// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chaps/chaps_service_redirect.h"

#include <dlfcn.h>

#include <base/logging.h>
#include <base/scoped_ptr.h>

#include "chaps/chaps.h"
#include "chaps/chaps_utility.h"

using std::string;
using std::vector;

typedef CK_RV (*GetFunctionList)(CK_FUNCTION_LIST_PTR_PTR);

namespace chaps {

ChapsServiceRedirect::ChapsServiceRedirect(const char* library_path)
    : library_path_(library_path),
      library_(NULL),
      functions_(NULL) {}

ChapsServiceRedirect::~ChapsServiceRedirect() {
  TearDown();
}

bool ChapsServiceRedirect::Init() {
  library_ = dlopen(library_path_.c_str(), RTLD_NOW | RTLD_LOCAL);
  if (!library_) {
    LOG(ERROR) << "Failed to load " << library_path_ << " - " << dlerror();
    return false;
  }
  GetFunctionList get_func_list =
      reinterpret_cast<GetFunctionList>(dlsym(library_, "C_GetFunctionList"));
  if (!get_func_list) {
    LOG(ERROR) << "Failed to find C_GetFunctionList - " << dlerror();
    TearDown();
    return false;
  }
  if (get_func_list(&functions_) != CKR_OK) {
    LOG(ERROR) << "C_GetFunctionList failed.";
    TearDown();
    return false;
  }
  CHECK(functions_) << "Library successfully returned NULL function list.";
  if (functions_->C_Initialize(NULL) != CKR_OK) {
    LOG(ERROR) << "C_Initialize failed.";
    TearDown();
    return false;
  }
  return true;
}

void ChapsServiceRedirect::TearDown() {
  if (functions_) {
    functions_->C_Finalize(NULL);
    functions_ = NULL;
  }
  if (library_) {
    dlclose(library_);
    library_ = NULL;
  }
}

uint32_t ChapsServiceRedirect::GetSlotList(bool token_present,
                                           vector<uint32_t>* slot_list) {
  CHECK(functions_);
  if (!slot_list || slot_list->size() > 0)
    LOG_CK_RV_AND_RETURN(CKR_ARGUMENTS_BAD);
  CK_ULONG count = 0;
  // First, call with NULL to retrieve the slot count.
  CK_RV result = functions_->C_GetSlotList(token_present, NULL, &count);
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  scoped_array<CK_SLOT_ID> slot_array(new CK_SLOT_ID[count]);
  CHECK(slot_array.get()) << "GetSlotList out of memory.";
  // Now, query the actual list.
  result = functions_->C_GetSlotList(token_present, slot_array.get(), &count);
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  for (CK_ULONG i = 0; i < count; i++) {
    slot_list->push_back(static_cast<uint32_t>(slot_array[i]));
  }
  return CKR_OK;
}

uint32_t ChapsServiceRedirect::GetSlotInfo(uint32_t slot_id,
                                           string* slot_description,
                                           string* manufacturer_id,
                                           uint32_t* flags,
                                           uint8_t* hardware_version_major,
                                           uint8_t* hardware_version_minor,
                                           uint8_t* firmware_version_major,
                                           uint8_t* firmware_version_minor) {
  CHECK(functions_);
  if (!slot_description || !manufacturer_id || !flags ||
      !hardware_version_major || !hardware_version_minor ||
      !firmware_version_major || !firmware_version_minor) {
    LOG_CK_RV_AND_RETURN(CKR_ARGUMENTS_BAD);
  }
  CK_SLOT_INFO slot_info;
  CK_RV result = functions_->C_GetSlotInfo(slot_id, &slot_info);
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  *slot_description = CharBufferToString(slot_info.slotDescription,
                                         arraysize(slot_info.slotDescription));
  *manufacturer_id = CharBufferToString(slot_info.manufacturerID,
                                        arraysize(slot_info.manufacturerID));
  *flags = static_cast<uint32_t>(slot_info.flags);
  *hardware_version_major = slot_info.hardwareVersion.major;
  *hardware_version_minor = slot_info.hardwareVersion.minor;
  *firmware_version_major = slot_info.firmwareVersion.major;
  *firmware_version_minor = slot_info.firmwareVersion.minor;
  return CKR_OK;
}

uint32_t ChapsServiceRedirect::GetTokenInfo(uint32_t slot_id,
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
  CHECK(functions_);
  if (!label || !manufacturer_id || !model || !serial_number || !flags ||
      !max_session_count || !session_count || !max_session_count_rw ||
      !session_count_rw || !max_pin_len || !min_pin_len ||
      !total_public_memory || !free_public_memory || !total_private_memory ||
      !total_public_memory ||
      !hardware_version_major || !hardware_version_minor ||
      !firmware_version_major || !firmware_version_minor) {
    LOG_CK_RV_AND_RETURN(CKR_ARGUMENTS_BAD);
  }
  CK_TOKEN_INFO token_info;
  CK_RV result = functions_->C_GetTokenInfo(slot_id, &token_info);
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  *label = CharBufferToString(token_info.label, arraysize(token_info.label));
  *manufacturer_id = CharBufferToString(token_info.manufacturerID,
                                        arraysize(token_info.manufacturerID));
  *model = CharBufferToString(token_info.model, arraysize(token_info.model));
  *serial_number = CharBufferToString(token_info.serialNumber,
                                      arraysize(token_info.serialNumber));
  *flags = token_info.flags;
  *max_session_count = token_info.ulMaxSessionCount;
  *session_count = token_info.ulSessionCount;
  *max_session_count_rw = token_info.ulMaxRwSessionCount;
  *session_count_rw = token_info.ulRwSessionCount;
  *max_pin_len = token_info.ulMaxPinLen;
  *min_pin_len = token_info.ulMinPinLen;
  *total_public_memory = token_info.ulTotalPublicMemory;
  *free_public_memory = token_info.ulFreePublicMemory;
  *total_private_memory = token_info.ulTotalPrivateMemory;
  *free_private_memory = token_info.ulFreePrivateMemory;
  *hardware_version_major = token_info.hardwareVersion.major;
  *hardware_version_minor = token_info.hardwareVersion.minor;
  *firmware_version_major = token_info.firmwareVersion.major;
  *firmware_version_minor = token_info.firmwareVersion.minor;
  return CKR_OK;
}

uint32_t ChapsServiceRedirect::GetMechanismList(
    uint32_t slot_id,
    std::vector<uint32_t>* mechanism_list) {
  CHECK(functions_);
  LOG_CK_RV_AND_RETURN_IF(!mechanism_list || mechanism_list->size() > 0,
                          CKR_ARGUMENTS_BAD);
  CK_ULONG count = 0;
  // First, call with NULL to retrieve the mechanism count.
  CK_RV result = functions_->C_GetMechanismList(slot_id, NULL, &count);
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  scoped_array<CK_MECHANISM_TYPE> mech_array(new CK_MECHANISM_TYPE[count]);
  LOG_CK_RV_AND_RETURN_IF(!mech_array.get(), CKR_HOST_MEMORY);
  // Now, query the actual list.
  result = functions_->C_GetMechanismList(slot_id, mech_array.get(), &count);
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  for (CK_ULONG i = 0; i < count; i++) {
    mechanism_list->push_back(static_cast<uint32_t>(mech_array[i]));
  }
  return CKR_OK;
}

uint32_t ChapsServiceRedirect::GetMechanismInfo(uint32_t slot_id,
                                                uint32_t mechanism_type,
                                                uint32_t* min_key_size,
                                                uint32_t* max_key_size,
                                                uint32_t* flags) {
  CHECK(functions_);
  if (!min_key_size || !max_key_size || !flags)
    LOG_CK_RV_AND_RETURN(CKR_ARGUMENTS_BAD);
  CK_MECHANISM_INFO mech_info;
  CK_RV result = functions_->C_GetMechanismInfo(slot_id,
                                                mechanism_type,
                                                &mech_info);
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  *min_key_size = static_cast<uint32_t>(mech_info.ulMinKeySize);
  *max_key_size = static_cast<uint32_t>(mech_info.ulMaxKeySize);
  *flags = static_cast<uint32_t>(mech_info.flags);
  return CKR_OK;
}

uint32_t ChapsServiceRedirect::InitToken(uint32_t slot_id, const string* so_pin,
                                         const string& label) {
  CHECK(functions_);
  CK_UTF8CHAR_PTR pin_buffer =
      so_pin ? StringToCharBuffer(so_pin->data()) : NULL;
  CK_ULONG pin_length = so_pin ? static_cast<CK_ULONG>(so_pin->length()) : 0;
  CK_UTF8CHAR label_buffer[chaps::kTokenLabelSize];
  CopyToCharBuffer(label.c_str(), label_buffer, chaps::kTokenLabelSize);
  CK_RV result = functions_->C_InitToken(slot_id,
                                         pin_buffer,
                                         pin_length,
                                         label_buffer);
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  return CKR_OK;
}

uint32_t ChapsServiceRedirect::InitPIN(uint32_t session_id, const string* pin) {
  CHECK(functions_);
  CK_UTF8CHAR_PTR pin_buffer = pin ? StringToCharBuffer(pin->data()) : NULL;
  CK_ULONG pin_length = pin ? static_cast<CK_ULONG>(pin->length()) : 0;
  CK_RV result = functions_->C_InitPIN(session_id, pin_buffer, pin_length);
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  return CKR_OK;
}

uint32_t ChapsServiceRedirect::SetPIN(uint32_t session_id,
                                      const string* old_pin,
                                      const string* new_pin) {
  CHECK(functions_);
  CK_UTF8CHAR_PTR old_pin_buffer =
      old_pin ? StringToCharBuffer(old_pin->data()) : NULL;
  CK_ULONG old_pin_length =
      old_pin ? static_cast<CK_ULONG>(old_pin->length()) : 0;
  CK_UTF8CHAR_PTR new_pin_buffer =
      new_pin ? StringToCharBuffer(new_pin->data()) : NULL;
  CK_ULONG new_pin_length =
      new_pin ? static_cast<CK_ULONG>(new_pin->length()) : 0;
  CK_RV result = functions_->C_SetPIN(session_id,
                                      old_pin_buffer, old_pin_length,
                                      new_pin_buffer, new_pin_length);
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  return CKR_OK;
}

uint32_t ChapsServiceRedirect::OpenSession(uint32_t slot_id, uint32_t flags,
                                           uint32_t* session_id) {
  CHECK(functions_);
  LOG_CK_RV_AND_RETURN_IF(!session_id, CKR_ARGUMENTS_BAD);
  CK_SESSION_HANDLE handle;
  uint32_t result = functions_->C_OpenSession(slot_id, flags, NULL, NULL,
                                              &handle);
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  *session_id = handle;
  return CKR_OK;
}

uint32_t ChapsServiceRedirect::CloseSession(uint32_t session_id) {
  CHECK(functions_);
  uint32_t result = functions_->C_CloseSession(session_id);
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  return CKR_OK;
}

uint32_t ChapsServiceRedirect::CloseAllSessions(uint32_t slot_id) {
  CHECK(functions_);
  uint32_t result = functions_->C_CloseAllSessions(slot_id);
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  return CKR_OK;
}

uint32_t ChapsServiceRedirect::GetSessionInfo(uint32_t session_id,
                                              uint32_t* slot_id,
                                              uint32_t* state,
                                              uint32_t* flags,
                                              uint32_t* device_error) {
  CHECK(functions_);
  if (!slot_id || !state || !flags || !device_error)
    LOG_CK_RV_AND_RETURN(CKR_ARGUMENTS_BAD);
  CK_SESSION_INFO info;
  uint32_t result = functions_->C_GetSessionInfo(session_id, &info);
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  *slot_id = static_cast<uint32_t>(info.slotID);
  *state = static_cast<uint32_t>(info.state);
  *flags = static_cast<uint32_t>(info.flags);
  *device_error = static_cast<uint32_t>(info.ulDeviceError);
  return CKR_OK;
}

uint32_t ChapsServiceRedirect::GetOperationState(
    uint32_t session_id,
    std::vector<uint8_t>* operation_state) {
  CHECK(functions_);
  LOG_CK_RV_AND_RETURN_IF(!operation_state, CKR_ARGUMENTS_BAD);
  CK_ULONG size = 0;
  // First, call with NULL to retrieve the state size.
  CK_RV result = functions_->C_GetOperationState(session_id, NULL, &size);
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  scoped_array<CK_BYTE> buffer(new CK_BYTE[size]);
  LOG_CK_RV_AND_RETURN_IF(!buffer.get(), CKR_HOST_MEMORY);
  // Now, get the actual state data.
  result = functions_->C_GetOperationState(session_id, buffer.get(), &size);
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  operation_state->assign(&buffer[0], &buffer[size]);
  return CKR_OK;
}

uint32_t ChapsServiceRedirect::SetOperationState(
    uint32_t session_id,
    const std::vector<uint8_t>& operation_state,
    uint32_t encryption_key_handle,
    uint32_t authentication_key_handle) {
  CHECK(functions_);
  uint32_t result = functions_->C_SetOperationState(
      session_id,
      static_cast<CK_BYTE_PTR>(
          const_cast<unsigned char*>(&operation_state.front())),
      static_cast<CK_ULONG>(operation_state.size()),
      static_cast<CK_OBJECT_HANDLE>(encryption_key_handle),
      static_cast<CK_OBJECT_HANDLE>(authentication_key_handle));
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  return CKR_OK;
}

uint32_t ChapsServiceRedirect::Login(uint32_t session_id,
                                     uint32_t user_type,
                                     const std::string* pin) {
  CHECK(functions_);
  CK_UTF8CHAR_PTR pin_buffer = pin ? StringToCharBuffer(pin->data()) : NULL;
  CK_ULONG pin_length = pin ? static_cast<CK_ULONG>(pin->length()) : 0;
  uint32_t result = functions_->C_Login(session_id,
                                        user_type,
                                        pin_buffer,
                                        pin_length);
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  LOG(INFO) << "Login success!";
  return CKR_OK;
}

uint32_t ChapsServiceRedirect::Logout(uint32_t session_id) {
  CHECK(functions_);
  uint32_t result = functions_->C_Logout(session_id);
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  return CKR_OK;
}

}  // namespace
