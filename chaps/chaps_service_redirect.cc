// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chaps/chaps_service_redirect.h"

#include <dlfcn.h>
#include <errno.h>

#include <base/logging.h>
#include <base/scoped_ptr.h>

#include "chaps/attributes.h"
#include "chaps/chaps.h"
#include "chaps/chaps_utility.h"

using std::string;
using std::vector;

typedef CK_RV (*GetFunctionList)(CK_FUNCTION_LIST_PTR_PTR);

namespace chaps {

ChapsServiceRedirect::ChapsServiceRedirect(const char* library_path)
    : library_path_(library_path),
      library_(NULL),
      functions_(NULL),
      is_initialized_(false) {}

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
  return true;
}

bool ChapsServiceRedirect::Init2() {
  const char* kUser = "chronos";
  const char* kGroup = "pkcs11";
  CHECK(functions_);
  if (is_initialized_)
    return true;
  if (!SetProcessUserAndGroup(kUser, kGroup, false))
    return false;
  CK_RV result = functions_->C_Initialize(NULL);
  if (result != CKR_OK && result != CKR_CRYPTOKI_ALREADY_INITIALIZED) {
    LOG(ERROR) << "C_Initialize : " << CK_RVToString(result);
    return false;
  }
  LOG(INFO) << library_path_ << " initialized.";
  is_initialized_ = true;
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
                                           vector<uint64_t>* slot_list) {
  if (!slot_list || slot_list->size() > 0)
    LOG_CK_RV_AND_RETURN(CKR_ARGUMENTS_BAD);
  if (!Init2()) {
    // If we can't initialize the target library, we will successfully report
    // zero slots.
    return CKR_OK;
  }
  CK_ULONG count = 0;
  // First, call with NULL to retrieve the slot count.
  CK_RV result = functions_->C_GetSlotList(token_present, NULL, &count);
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  scoped_array<CK_SLOT_ID> slot_array(new CK_SLOT_ID[count]);
  CHECK(slot_array.get()) << "GetSlotList out of memory.";
  // Now, query the actual list.
  result = functions_->C_GetSlotList(token_present, slot_array.get(), &count);
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  for (CK_ULONG i = 0; i < count; ++i) {
    slot_list->push_back(static_cast<uint64_t>(slot_array[i]));
  }
  return CKR_OK;
}

uint32_t ChapsServiceRedirect::GetSlotInfo(uint64_t slot_id,
                                           vector<uint8_t>* slot_description,
                                           vector<uint8_t>* manufacturer_id,
                                           uint64_t* flags,
                                           uint8_t* hardware_version_major,
                                           uint8_t* hardware_version_minor,
                                           uint8_t* firmware_version_major,
                                           uint8_t* firmware_version_minor) {
  LOG_CK_RV_AND_RETURN_IF(!Init2(), CKR_GENERAL_ERROR);
  if (!slot_description || !manufacturer_id || !flags ||
      !hardware_version_major || !hardware_version_minor ||
      !firmware_version_major || !firmware_version_minor) {
    LOG_CK_RV_AND_RETURN(CKR_ARGUMENTS_BAD);
  }
  CK_SLOT_INFO slot_info;
  CK_RV result = functions_->C_GetSlotInfo(slot_id, &slot_info);
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  *slot_description =
      ConvertByteBufferToVector(slot_info.slotDescription,
                                arraysize(slot_info.slotDescription));
  *manufacturer_id =
      ConvertByteBufferToVector(slot_info.manufacturerID,
                                arraysize(slot_info.manufacturerID));
  *flags = static_cast<uint64_t>(slot_info.flags);
  *hardware_version_major = slot_info.hardwareVersion.major;
  *hardware_version_minor = slot_info.hardwareVersion.minor;
  *firmware_version_major = slot_info.firmwareVersion.major;
  *firmware_version_minor = slot_info.firmwareVersion.minor;
  return CKR_OK;
}

uint32_t ChapsServiceRedirect::GetTokenInfo(uint64_t slot_id,
                                            vector<uint8_t>* label,
                                            vector<uint8_t>* manufacturer_id,
                                            vector<uint8_t>* model,
                                            vector<uint8_t>* serial_number,
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
                                            uint8_t* firmware_version_minor) {
  LOG_CK_RV_AND_RETURN_IF(!Init2(), CKR_GENERAL_ERROR);
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
  *label =
      ConvertByteBufferToVector(token_info.label, arraysize(token_info.label));
  *manufacturer_id =
      ConvertByteBufferToVector(token_info.manufacturerID,
                                arraysize(token_info.manufacturerID));
  *model = ConvertByteBufferToVector(token_info.model,
                                     arraysize(token_info.model));
  *serial_number =
      ConvertByteBufferToVector(token_info.serialNumber,
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
    uint64_t slot_id,
    vector<uint64_t>* mechanism_list) {
  LOG_CK_RV_AND_RETURN_IF(!Init2(), CKR_GENERAL_ERROR);
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
  for (CK_ULONG i = 0; i < count; ++i) {
    mechanism_list->push_back(static_cast<uint64_t>(mech_array[i]));
  }
  return CKR_OK;
}

uint32_t ChapsServiceRedirect::GetMechanismInfo(uint64_t slot_id,
                                                uint64_t mechanism_type,
                                                uint64_t* min_key_size,
                                                uint64_t* max_key_size,
                                                uint64_t* flags) {
  LOG_CK_RV_AND_RETURN_IF(!Init2(), CKR_GENERAL_ERROR);
  if (!min_key_size || !max_key_size || !flags)
    LOG_CK_RV_AND_RETURN(CKR_ARGUMENTS_BAD);
  CK_MECHANISM_INFO mech_info;
  CK_RV result = functions_->C_GetMechanismInfo(slot_id,
                                                mechanism_type,
                                                &mech_info);
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  *min_key_size = static_cast<uint64_t>(mech_info.ulMinKeySize);
  *max_key_size = static_cast<uint64_t>(mech_info.ulMaxKeySize);
  *flags = static_cast<uint64_t>(mech_info.flags);
  return CKR_OK;
}

uint32_t ChapsServiceRedirect::InitToken(uint64_t slot_id,
                                         const string* so_pin,
                                         const vector<uint8_t>& label) {
  LOG_CK_RV_AND_RETURN_IF(!Init2(), CKR_GENERAL_ERROR);
  LOG_CK_RV_AND_RETURN_IF(label.size() != chaps::kTokenLabelSize,
                          CKR_ARGUMENTS_BAD);
  CK_UTF8CHAR_PTR pin_buffer =
      so_pin ? ConvertStringToCharBuffer(so_pin->data()) : NULL;
  CK_ULONG pin_length = so_pin ? static_cast<CK_ULONG>(so_pin->length()) : 0;
  CK_UTF8CHAR label_buffer[chaps::kTokenLabelSize];
  memcpy(label_buffer, &label.front(), chaps::kTokenLabelSize);
  CK_RV result = functions_->C_InitToken(slot_id,
                                         pin_buffer,
                                         pin_length,
                                         label_buffer);
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  return CKR_OK;
}

uint32_t ChapsServiceRedirect::InitPIN(uint64_t session_id, const string* pin) {
  LOG_CK_RV_AND_RETURN_IF(!Init2(), CKR_GENERAL_ERROR);
  CK_UTF8CHAR_PTR pin_buffer =
      pin ? ConvertStringToCharBuffer(pin->data()) : NULL;
  CK_ULONG pin_length = pin ? static_cast<CK_ULONG>(pin->length()) : 0;
  CK_RV result = functions_->C_InitPIN(session_id, pin_buffer, pin_length);
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  return CKR_OK;
}

uint32_t ChapsServiceRedirect::SetPIN(uint64_t session_id,
                                      const string* old_pin,
                                      const string* new_pin) {
  LOG_CK_RV_AND_RETURN_IF(!Init2(), CKR_GENERAL_ERROR);
  CK_UTF8CHAR_PTR old_pin_buffer =
      old_pin ? ConvertStringToCharBuffer(old_pin->data()) : NULL;
  CK_ULONG old_pin_length =
      old_pin ? static_cast<CK_ULONG>(old_pin->length()) : 0;
  CK_UTF8CHAR_PTR new_pin_buffer =
      new_pin ? ConvertStringToCharBuffer(new_pin->data()) : NULL;
  CK_ULONG new_pin_length =
      new_pin ? static_cast<CK_ULONG>(new_pin->length()) : 0;
  CK_RV result = functions_->C_SetPIN(session_id,
                                      old_pin_buffer, old_pin_length,
                                      new_pin_buffer, new_pin_length);
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  return CKR_OK;
}

uint32_t ChapsServiceRedirect::OpenSession(uint64_t slot_id, uint64_t flags,
                                           uint64_t* session_id) {
  LOG_CK_RV_AND_RETURN_IF(!Init2(), CKR_GENERAL_ERROR);
  LOG_CK_RV_AND_RETURN_IF(!session_id, CKR_ARGUMENTS_BAD);
  CK_SESSION_HANDLE handle;
  uint32_t result = functions_->C_OpenSession(slot_id, flags, NULL, NULL,
                                              &handle);
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  *session_id = handle;
  return CKR_OK;
}

uint32_t ChapsServiceRedirect::CloseSession(uint64_t session_id) {
  LOG_CK_RV_AND_RETURN_IF(!Init2(), CKR_GENERAL_ERROR);
  uint32_t result = functions_->C_CloseSession(session_id);
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  return CKR_OK;
}

uint32_t ChapsServiceRedirect::CloseAllSessions(uint64_t slot_id) {
  LOG_CK_RV_AND_RETURN_IF(!Init2(), CKR_GENERAL_ERROR);
  uint32_t result = functions_->C_CloseAllSessions(slot_id);
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  return CKR_OK;
}

uint32_t ChapsServiceRedirect::GetSessionInfo(uint64_t session_id,
                                              uint64_t* slot_id,
                                              uint64_t* state,
                                              uint64_t* flags,
                                              uint64_t* device_error) {
  LOG_CK_RV_AND_RETURN_IF(!Init2(), CKR_GENERAL_ERROR);
  if (!slot_id || !state || !flags || !device_error)
    LOG_CK_RV_AND_RETURN(CKR_ARGUMENTS_BAD);
  CK_SESSION_INFO info;
  uint32_t result = functions_->C_GetSessionInfo(session_id, &info);
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  *slot_id = static_cast<uint64_t>(info.slotID);
  *state = static_cast<uint64_t>(info.state);
  *flags = static_cast<uint64_t>(info.flags);
  *device_error = static_cast<uint64_t>(info.ulDeviceError);
  return CKR_OK;
}

uint32_t ChapsServiceRedirect::GetOperationState(
    uint64_t session_id,
    vector<uint8_t>* operation_state) {
  LOG_CK_RV_AND_RETURN_IF(!Init2(), CKR_GENERAL_ERROR);
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
  *operation_state = ConvertByteBufferToVector(buffer.get(), size);
  return CKR_OK;
}

uint32_t ChapsServiceRedirect::SetOperationState(
    uint64_t session_id,
    const vector<uint8_t>& operation_state,
    uint64_t encryption_key_handle,
    uint64_t authentication_key_handle) {
  LOG_CK_RV_AND_RETURN_IF(!Init2(), CKR_GENERAL_ERROR);
  uint32_t result = functions_->C_SetOperationState(
      session_id,
      static_cast<CK_BYTE_PTR>(const_cast<uint8_t*>(&operation_state.front())),
      static_cast<CK_ULONG>(operation_state.size()),
      static_cast<CK_OBJECT_HANDLE>(encryption_key_handle),
      static_cast<CK_OBJECT_HANDLE>(authentication_key_handle));
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  return CKR_OK;
}

uint32_t ChapsServiceRedirect::Login(uint64_t session_id,
                                     uint64_t user_type,
                                     const string* pin) {
  LOG_CK_RV_AND_RETURN_IF(!Init2(), CKR_GENERAL_ERROR);
  CK_UTF8CHAR_PTR pin_buffer =
      pin ? ConvertStringToCharBuffer(pin->data()) : NULL;
  CK_ULONG pin_length = pin ? static_cast<CK_ULONG>(pin->length()) : 0;
  uint32_t result = functions_->C_Login(session_id,
                                        user_type,
                                        pin_buffer,
                                        pin_length);
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  return CKR_OK;
}

uint32_t ChapsServiceRedirect::Logout(uint64_t session_id) {
  LOG_CK_RV_AND_RETURN_IF(!Init2(), CKR_GENERAL_ERROR);
  uint32_t result = functions_->C_Logout(session_id);
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  return CKR_OK;
}

uint32_t ChapsServiceRedirect::CreateObject(uint64_t session_id,
                                            const vector<uint8_t>& attributes,
                                            uint64_t* new_object_handle) {
  LOG_CK_RV_AND_RETURN_IF(!Init2(), CKR_GENERAL_ERROR);
  LOG_CK_RV_AND_RETURN_IF(!new_object_handle, CKR_ARGUMENTS_BAD);
  Attributes tmp;
  if (!tmp.Parse(attributes))
    LOG_CK_RV_AND_RETURN(CKR_TEMPLATE_INCONSISTENT);
  uint32_t result = functions_->C_CreateObject(
      session_id,
      tmp.attributes(),
      tmp.num_attributes(),
      PreservedUint64_t(new_object_handle));
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  return CKR_OK;
}

uint32_t ChapsServiceRedirect::CopyObject(uint64_t session_id,
                                          uint64_t object_handle,
                                          const vector<uint8_t>& attributes,
                                          uint64_t* new_object_handle) {
  LOG_CK_RV_AND_RETURN_IF(!Init2(), CKR_GENERAL_ERROR);
  LOG_CK_RV_AND_RETURN_IF(!new_object_handle, CKR_ARGUMENTS_BAD);
  Attributes tmp;
  if (!tmp.Parse(attributes))
    LOG_CK_RV_AND_RETURN(CKR_TEMPLATE_INCONSISTENT);
  uint32_t result = functions_->C_CopyObject(
      session_id,
      object_handle,
      tmp.attributes(),
      tmp.num_attributes(),
      PreservedUint64_t(new_object_handle));
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  return CKR_OK;
}

uint32_t ChapsServiceRedirect::DestroyObject(uint64_t session_id,
                                             uint64_t object_handle) {
  LOG_CK_RV_AND_RETURN_IF(!Init2(), CKR_GENERAL_ERROR);
  uint32_t result = functions_->C_DestroyObject(session_id, object_handle);
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  return CKR_OK;
}

uint32_t ChapsServiceRedirect::GetObjectSize(uint64_t session_id,
                                             uint64_t object_handle,
                                             uint64_t* object_size) {
  LOG_CK_RV_AND_RETURN_IF(!Init2(), CKR_GENERAL_ERROR);
  LOG_CK_RV_AND_RETURN_IF(!object_size, CKR_ARGUMENTS_BAD);
  uint32_t result = functions_->C_GetObjectSize(session_id,
                                                object_handle,
                                                PreservedUint64_t(object_size));
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  return CKR_OK;
}

uint32_t ChapsServiceRedirect::GetAttributeValue(
    uint64_t session_id,
    uint64_t object_handle,
    const vector<uint8_t>& attributes_in,
    vector<uint8_t>* attributes_out) {
  LOG_CK_RV_AND_RETURN_IF(!Init2(), CKR_GENERAL_ERROR);
  LOG_CK_RV_AND_RETURN_IF(!attributes_out, CKR_ARGUMENTS_BAD);
  Attributes tmp;
  if (!tmp.Parse(attributes_in))
    LOG_CK_RV_AND_RETURN(CKR_TEMPLATE_INCONSISTENT);
  uint32_t result = functions_->C_GetAttributeValue(session_id,
                                                    object_handle,
                                                    tmp.attributes(),
                                                    tmp.num_attributes());
  if (result != CKR_OK)
    LOG_CK_RV(result);
  if (!tmp.Serialize(attributes_out))
    LOG_CK_RV_AND_RETURN(CKR_TEMPLATE_INCONSISTENT);
  return result;
}

uint32_t ChapsServiceRedirect::SetAttributeValue(
    uint64_t session_id,
    uint64_t object_handle,
    const vector<uint8_t>& attributes) {
  LOG_CK_RV_AND_RETURN_IF(!Init2(), CKR_GENERAL_ERROR);
  Attributes tmp;
  LOG_CK_RV_AND_RETURN_IF(!tmp.Parse(attributes), CKR_TEMPLATE_INCONSISTENT);
  uint32_t result = functions_->C_SetAttributeValue(session_id,
                                                    object_handle,
                                                    tmp.attributes(),
                                                    tmp.num_attributes());
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  return CKR_OK;
}

uint32_t ChapsServiceRedirect::FindObjectsInit(
    uint64_t session_id,
    const vector<uint8_t>& attributes) {
  LOG_CK_RV_AND_RETURN_IF(!Init2(), CKR_GENERAL_ERROR);
  Attributes tmp;
  LOG_CK_RV_AND_RETURN_IF(!tmp.Parse(attributes), CKR_TEMPLATE_INCONSISTENT);
  uint32_t result = functions_->C_FindObjectsInit(session_id,
                                                  tmp.attributes(),
                                                  tmp.num_attributes());
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  return CKR_OK;
}

uint32_t ChapsServiceRedirect::FindObjects(uint64_t session_id,
                                           uint64_t max_object_count,
                                           vector<uint64_t>* object_list) {
  LOG_CK_RV_AND_RETURN_IF(!Init2(), CKR_GENERAL_ERROR);
  if (!object_list || object_list->size() > 0)
    LOG_CK_RV_AND_RETURN(CKR_ARGUMENTS_BAD);
  scoped_array<CK_OBJECT_HANDLE> object_handles(
      new CK_OBJECT_HANDLE[max_object_count]);
  CHECK(object_handles.get());
  CK_ULONG object_count = 0;
  uint32_t result = functions_->C_FindObjects(session_id,
                                              object_handles.get(),
                                              max_object_count,
                                              &object_count);
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  for (CK_ULONG i = 0; i < object_count; i++) {
    object_list->push_back(static_cast<uint64_t>(object_handles[i]));
  }
  return CKR_OK;
}

uint32_t ChapsServiceRedirect::FindObjectsFinal(uint64_t session_id) {
  LOG_CK_RV_AND_RETURN_IF(!Init2(), CKR_GENERAL_ERROR);
  uint32_t result = functions_->C_FindObjectsFinal(session_id);
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  return CKR_OK;
}

uint32_t ChapsServiceRedirect::EncryptInit(
    uint64_t session_id,
    uint64_t mechanism_type,
    const vector<uint8_t>& mechanism_parameter,
    uint64_t key_handle) {
  LOG_CK_RV_AND_RETURN_IF(!Init2(), CKR_GENERAL_ERROR);
  CK_MECHANISM mechanism;
  mechanism.mechanism = static_cast<CK_MECHANISM_TYPE>(mechanism_type);
  mechanism.pParameter = const_cast<uint8_t*>(&mechanism_parameter.front());
  mechanism.ulParameterLen = mechanism_parameter.size();
  uint32_t result = functions_->C_EncryptInit(
      session_id,
      &mechanism,
      static_cast<CK_OBJECT_HANDLE>(key_handle));
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  return CKR_OK;
}

uint32_t ChapsServiceRedirect::Encrypt(uint64_t session_id,
                                       const vector<uint8_t>& data_in,
                                       uint64_t max_out_length,
                                       uint64_t* actual_out_length,
                                       vector<uint8_t>* data_out) {
  LOG_CK_RV_AND_RETURN_IF(!Init2(), CKR_GENERAL_ERROR);
  LOG_CK_RV_AND_RETURN_IF(!actual_out_length || !data_out, CKR_ARGUMENTS_BAD);
  CK_BYTE_PTR in_bytes =
      static_cast<CK_BYTE_PTR>(const_cast<uint8_t*>(&data_in.front()));
  scoped_array<CK_BYTE> out_bytes(NULL);
  if (max_out_length) {
    out_bytes.reset(new CK_BYTE[max_out_length]);
    CHECK(out_bytes.get());
  }
  CK_ULONG length = max_out_length;
  uint32_t result = functions_->C_Encrypt(session_id,
                                          in_bytes,
                                          data_in.size(),
                                          out_bytes.get(),
                                          &length);
  // Provide the length before checking the result. This handles cases like
  // CKR_BUFFER_TOO_SMALL.
  *actual_out_length = static_cast<uint64_t>(length);
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  *data_out = ConvertByteBufferToVector(out_bytes.get(), length);
  return CKR_OK;
}

uint32_t ChapsServiceRedirect::EncryptUpdate(
    uint64_t session_id,
    const vector<uint8_t>& data_in,
    uint64_t max_out_length,
    uint64_t* actual_out_length,
    vector<uint8_t>* data_out) {
  LOG_CK_RV_AND_RETURN_IF(!Init2(), CKR_GENERAL_ERROR);
  LOG_CK_RV_AND_RETURN_IF(!actual_out_length || !data_out, CKR_ARGUMENTS_BAD);
  CK_BYTE_PTR in_bytes =
      static_cast<CK_BYTE_PTR>(const_cast<uint8_t*>(&data_in.front()));
  scoped_array<CK_BYTE> out_bytes(NULL);
  if (max_out_length) {
    out_bytes.reset(new CK_BYTE[max_out_length]);
    CHECK(out_bytes.get());
  }
  CK_ULONG length = max_out_length;
  uint32_t result = functions_->C_EncryptUpdate(session_id,
                                                in_bytes,
                                                data_in.size(),
                                                out_bytes.get(),
                                                &length);
  // Provide the length before checking the result. This handles cases like
  // CKR_BUFFER_TOO_SMALL.
  *actual_out_length = static_cast<uint64_t>(length);
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  *data_out = ConvertByteBufferToVector(out_bytes.get(), length);
  return CKR_OK;
}

uint32_t ChapsServiceRedirect::EncryptFinal(uint64_t session_id,
                                            uint64_t max_out_length,
                                            uint64_t* actual_out_length,
                                            vector<uint8_t>* data_out) {
  LOG_CK_RV_AND_RETURN_IF(!Init2(), CKR_GENERAL_ERROR);
  LOG_CK_RV_AND_RETURN_IF(!actual_out_length || !data_out, CKR_ARGUMENTS_BAD);
  scoped_array<CK_BYTE> out_bytes(NULL);
  if (max_out_length) {
    out_bytes.reset(new CK_BYTE[max_out_length]);
    CHECK(out_bytes.get());
  }
  CK_ULONG length = max_out_length;
  uint32_t result = functions_->C_EncryptFinal(session_id,
                                               out_bytes.get(),
                                               &length);
  // Provide the length before checking the result. This handles cases like
  // CKR_BUFFER_TOO_SMALL.
  *actual_out_length = static_cast<uint64_t>(length);
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  *data_out = ConvertByteBufferToVector(out_bytes.get(), length);
  return CKR_OK;
}

uint32_t ChapsServiceRedirect::DecryptInit(
    uint64_t session_id,
    uint64_t mechanism_type,
    const vector<uint8_t>& mechanism_parameter,
    uint64_t key_handle) {
  LOG_CK_RV_AND_RETURN_IF(!Init2(), CKR_GENERAL_ERROR);
  CK_MECHANISM mechanism;
  mechanism.mechanism = static_cast<CK_MECHANISM_TYPE>(mechanism_type);
  mechanism.pParameter = const_cast<uint8_t*>(&mechanism_parameter.front());
  mechanism.ulParameterLen = mechanism_parameter.size();
  uint32_t result = functions_->C_DecryptInit(
      session_id,
      &mechanism,
      static_cast<CK_OBJECT_HANDLE>(key_handle));
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  return CKR_OK;
}

uint32_t ChapsServiceRedirect::Decrypt(uint64_t session_id,
                                       const vector<uint8_t>& data_in,
                                       uint64_t max_out_length,
                                       uint64_t* actual_out_length,
                                       vector<uint8_t>* data_out) {
  LOG_CK_RV_AND_RETURN_IF(!Init2(), CKR_GENERAL_ERROR);
  LOG_CK_RV_AND_RETURN_IF(!actual_out_length || !data_out, CKR_ARGUMENTS_BAD);
  CK_BYTE_PTR in_bytes =
      static_cast<CK_BYTE_PTR>(const_cast<uint8_t*>(&data_in.front()));
  scoped_array<CK_BYTE> out_bytes(NULL);
  if (max_out_length) {
    out_bytes.reset(new CK_BYTE[max_out_length]);
    CHECK(out_bytes.get());
  }
  CK_ULONG length = max_out_length;
  uint32_t result = functions_->C_Decrypt(session_id,
                                          in_bytes,
                                          data_in.size(),
                                          out_bytes.get(),
                                          &length);
  // Provide the length before checking the result. This handles cases like
  // CKR_BUFFER_TOO_SMALL.
  *actual_out_length = static_cast<uint64_t>(length);
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  if (out_bytes.get())
    *data_out = ConvertByteBufferToVector(out_bytes.get(), length);
  return CKR_OK;
}

uint32_t ChapsServiceRedirect::DecryptUpdate(
    uint64_t session_id,
    const vector<uint8_t>& data_in,
    uint64_t max_out_length,
    uint64_t* actual_out_length,
    vector<uint8_t>* data_out) {
  LOG_CK_RV_AND_RETURN_IF(!Init2(), CKR_GENERAL_ERROR);
  LOG_CK_RV_AND_RETURN_IF(!actual_out_length || !data_out, CKR_ARGUMENTS_BAD);
  CK_BYTE_PTR in_bytes =
      static_cast<CK_BYTE_PTR>(const_cast<uint8_t*>(&data_in.front()));
  scoped_array<CK_BYTE> out_bytes(NULL);
  if (max_out_length) {
    out_bytes.reset(new CK_BYTE[max_out_length]);
    CHECK(out_bytes.get());
  }
  CK_ULONG length = max_out_length;
  uint32_t result = functions_->C_DecryptUpdate(session_id,
                                                in_bytes,
                                                data_in.size(),
                                                out_bytes.get(),
                                                &length);
  // Provide the length before checking the result. This handles cases like
  // CKR_BUFFER_TOO_SMALL.
  *actual_out_length = static_cast<uint64_t>(length);
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  *data_out = ConvertByteBufferToVector(out_bytes.get(), length);
  return CKR_OK;
}

uint32_t ChapsServiceRedirect::DecryptFinal(uint64_t session_id,
                                            uint64_t max_out_length,
                                            uint64_t* actual_out_length,
                                            vector<uint8_t>* data_out) {
  LOG_CK_RV_AND_RETURN_IF(!Init2(), CKR_GENERAL_ERROR);
  LOG_CK_RV_AND_RETURN_IF(!actual_out_length || !data_out, CKR_ARGUMENTS_BAD);
  scoped_array<CK_BYTE> out_bytes(NULL);
  if (max_out_length) {
    out_bytes.reset(new CK_BYTE[max_out_length]);
    CHECK(out_bytes.get());
  }
  CK_ULONG length = max_out_length;
  uint32_t result = functions_->C_DecryptFinal(session_id,
                                               out_bytes.get(),
                                               &length);
  // Provide the length before checking the result. This handles cases like
  // CKR_BUFFER_TOO_SMALL.
  *actual_out_length = static_cast<uint64_t>(length);
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  *data_out = ConvertByteBufferToVector(out_bytes.get(), length);
  return CKR_OK;
}

uint32_t ChapsServiceRedirect::DigestInit(
    uint64_t session_id,
    uint64_t mechanism_type,
    const vector<uint8_t>& mechanism_parameter) {
  LOG_CK_RV_AND_RETURN_IF(!Init2(), CKR_GENERAL_ERROR);
  CK_MECHANISM mechanism;
  mechanism.mechanism = static_cast<CK_MECHANISM_TYPE>(mechanism_type);
  mechanism.pParameter = const_cast<uint8_t*>(&mechanism_parameter.front());
  mechanism.ulParameterLen = mechanism_parameter.size();
  uint32_t result = functions_->C_DigestInit(session_id, &mechanism);
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  return CKR_OK;
}

uint32_t ChapsServiceRedirect::Digest(uint64_t session_id,
                                      const vector<uint8_t>& data_in,
                                      uint64_t max_out_length,
                                      uint64_t* actual_out_length,
                                      vector<uint8_t>* digest) {
  LOG_CK_RV_AND_RETURN_IF(!Init2(), CKR_GENERAL_ERROR);
  LOG_CK_RV_AND_RETURN_IF(!actual_out_length || !digest, CKR_ARGUMENTS_BAD);
  CK_BYTE_PTR in_bytes =
      static_cast<CK_BYTE_PTR>(const_cast<uint8_t*>(&data_in.front()));
  scoped_array<CK_BYTE> out_bytes(NULL);
  if (max_out_length) {
    out_bytes.reset(new CK_BYTE[max_out_length]);
    CHECK(out_bytes.get());
  }
  CK_ULONG length = max_out_length;
  uint32_t result = functions_->C_Digest(session_id,
                                         in_bytes,
                                         data_in.size(),
                                         out_bytes.get(),
                                         &length);
  // Provide the length before checking the result. This handles cases like
  // CKR_BUFFER_TOO_SMALL.
  *actual_out_length = static_cast<uint64_t>(length);
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  *digest = ConvertByteBufferToVector(out_bytes.get(), length);
  return CKR_OK;
}

uint32_t ChapsServiceRedirect::DigestUpdate(uint64_t session_id,
                                            const vector<uint8_t>& data_in) {
  LOG_CK_RV_AND_RETURN_IF(!Init2(), CKR_GENERAL_ERROR);
  CK_BYTE_PTR in_bytes =
      static_cast<CK_BYTE_PTR>(const_cast<uint8_t*>(&data_in.front()));
  uint32_t result = functions_->C_DigestUpdate(session_id,
                                               in_bytes,
                                               data_in.size());
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  return CKR_OK;
}

uint32_t ChapsServiceRedirect::DigestKey(uint64_t session_id,
                                         uint64_t key_handle) {
  LOG_CK_RV_AND_RETURN_IF(!Init2(), CKR_GENERAL_ERROR);
  uint32_t result = functions_->C_DigestKey(session_id, key_handle);
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  return CKR_OK;
}

uint32_t ChapsServiceRedirect::DigestFinal(uint64_t session_id,
                                           uint64_t max_out_length,
                                           uint64_t* actual_out_length,
                                           vector<uint8_t>* digest) {
  LOG_CK_RV_AND_RETURN_IF(!Init2(), CKR_GENERAL_ERROR);
  LOG_CK_RV_AND_RETURN_IF(!actual_out_length || !digest, CKR_ARGUMENTS_BAD);
  scoped_array<CK_BYTE> out_bytes(NULL);
  if (max_out_length) {
    out_bytes.reset(new CK_BYTE[max_out_length]);
    CHECK(out_bytes.get());
  }
  CK_ULONG length = max_out_length;
  uint32_t result = functions_->C_DigestFinal(session_id,
                                              out_bytes.get(),
                                              &length);
  // Provide the length before checking the result. This handles cases like
  // CKR_BUFFER_TOO_SMALL.
  *actual_out_length = static_cast<uint64_t>(length);
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  *digest = ConvertByteBufferToVector(out_bytes.get(), length);
  return CKR_OK;
}

uint32_t ChapsServiceRedirect::SignInit(
    uint64_t session_id,
    uint64_t mechanism_type,
    const vector<uint8_t>& mechanism_parameter,
    uint64_t key_handle) {
  LOG_CK_RV_AND_RETURN_IF(!Init2(), CKR_GENERAL_ERROR);
  CK_MECHANISM mechanism;
  mechanism.mechanism = static_cast<CK_MECHANISM_TYPE>(mechanism_type);
  mechanism.pParameter = const_cast<uint8_t*>(&mechanism_parameter.front());
  mechanism.ulParameterLen = mechanism_parameter.size();
  uint32_t result = functions_->C_SignInit(session_id, &mechanism, key_handle);
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  return CKR_OK;
}

uint32_t ChapsServiceRedirect::Sign(uint64_t session_id,
                                    const vector<uint8_t>& data,
                                    uint64_t max_out_length,
                                    uint64_t* actual_out_length,
                                    vector<uint8_t>* signature) {
  LOG_CK_RV_AND_RETURN_IF(!Init2(), CKR_GENERAL_ERROR);
  LOG_CK_RV_AND_RETURN_IF(!actual_out_length || !signature, CKR_ARGUMENTS_BAD);
  CK_BYTE_PTR in_bytes =
      static_cast<CK_BYTE_PTR>(const_cast<uint8_t*>(&data.front()));
  scoped_array<CK_BYTE> out_bytes(NULL);
  if (max_out_length) {
    out_bytes.reset(new CK_BYTE[max_out_length]);
    CHECK(out_bytes.get());
  }
  CK_ULONG length = max_out_length;
  uint32_t result = functions_->C_Sign(session_id,
                                       in_bytes,
                                       data.size(),
                                       out_bytes.get(),
                                       &length);
  // Provide the length before checking the result. This handles cases like
  // CKR_BUFFER_TOO_SMALL.
  *actual_out_length = static_cast<uint64_t>(length);
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  *signature = ConvertByteBufferToVector(out_bytes.get(), length);
  return CKR_OK;
}

uint32_t ChapsServiceRedirect::SignUpdate(uint64_t session_id,
                                          const vector<uint8_t>& data_part) {
  LOG_CK_RV_AND_RETURN_IF(!Init2(), CKR_GENERAL_ERROR);
  CK_BYTE_PTR in_bytes =
      static_cast<CK_BYTE_PTR>(const_cast<uint8_t*>(&data_part.front()));
  uint32_t result = functions_->C_SignUpdate(session_id,
                                             in_bytes,
                                             data_part.size());
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  return CKR_OK;
}

uint32_t ChapsServiceRedirect::SignFinal(uint64_t session_id,
                                         uint64_t max_out_length,
                                         uint64_t* actual_out_length,
                                         vector<uint8_t>* signature) {
  LOG_CK_RV_AND_RETURN_IF(!Init2(), CKR_GENERAL_ERROR);
  LOG_CK_RV_AND_RETURN_IF(!actual_out_length || !signature, CKR_ARGUMENTS_BAD);
  scoped_array<CK_BYTE> out_bytes(NULL);
  if (max_out_length) {
    out_bytes.reset(new CK_BYTE[max_out_length]);
    CHECK(out_bytes.get());
  }
  CK_ULONG length = max_out_length;
  uint32_t result = functions_->C_SignFinal(session_id,
                                            out_bytes.get(),
                                            &length);
  // Provide the length before checking the result. This handles cases like
  // CKR_BUFFER_TOO_SMALL.
  *actual_out_length = static_cast<uint64_t>(length);
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  *signature = ConvertByteBufferToVector(out_bytes.get(), length);
  return CKR_OK;
}

uint32_t ChapsServiceRedirect::SignRecoverInit(
      uint64_t session_id,
      uint64_t mechanism_type,
      const vector<uint8_t>& mechanism_parameter,
      uint64_t key_handle) {
  LOG_CK_RV_AND_RETURN_IF(!Init2(), CKR_GENERAL_ERROR);
  CK_MECHANISM mechanism;
  mechanism.mechanism = static_cast<CK_MECHANISM_TYPE>(mechanism_type);
  mechanism.pParameter = const_cast<uint8_t*>(&mechanism_parameter.front());
  mechanism.ulParameterLen = mechanism_parameter.size();
  uint32_t result = functions_->C_SignRecoverInit(session_id,
                                                  &mechanism,
                                                  key_handle);
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  return CKR_OK;
}

uint32_t ChapsServiceRedirect::SignRecover(uint64_t session_id,
                                           const vector<uint8_t>& data,
                                           uint64_t max_out_length,
                                           uint64_t* actual_out_length,
                                           vector<uint8_t>* signature) {
  LOG_CK_RV_AND_RETURN_IF(!Init2(), CKR_GENERAL_ERROR);
  LOG_CK_RV_AND_RETURN_IF(!actual_out_length || !signature, CKR_ARGUMENTS_BAD);
  CK_BYTE_PTR in_bytes =
      static_cast<CK_BYTE_PTR>(const_cast<uint8_t*>(&data.front()));
  scoped_array<CK_BYTE> out_bytes(NULL);
  if (max_out_length) {
    out_bytes.reset(new CK_BYTE[max_out_length]);
    CHECK(out_bytes.get());
  }
  CK_ULONG length = max_out_length;
  uint32_t result = functions_->C_SignRecover(session_id,
                                              in_bytes,
                                              data.size(),
                                              out_bytes.get(),
                                              &length);
  // Provide the length before checking the result. This handles cases like
  // CKR_BUFFER_TOO_SMALL.
  *actual_out_length = static_cast<uint64_t>(length);
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  *signature = ConvertByteBufferToVector(out_bytes.get(), length);
  return CKR_OK;
}

uint32_t ChapsServiceRedirect::VerifyInit(
    uint64_t session_id,
    uint64_t mechanism_type,
    const vector<uint8_t>& mechanism_parameter,
    uint64_t key_handle) {
  LOG_CK_RV_AND_RETURN_IF(!Init2(), CKR_GENERAL_ERROR);
  CK_MECHANISM mechanism;
  mechanism.mechanism = static_cast<CK_MECHANISM_TYPE>(mechanism_type);
  mechanism.pParameter = const_cast<uint8_t*>(&mechanism_parameter.front());
  mechanism.ulParameterLen = mechanism_parameter.size();
  uint32_t result = functions_->C_VerifyInit(session_id,
                                             &mechanism,
                                             key_handle);
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  return CKR_OK;
}

uint32_t ChapsServiceRedirect::Verify(uint64_t session_id,
                                      const vector<uint8_t>& data,
                                      const vector<uint8_t>& signature) {
  LOG_CK_RV_AND_RETURN_IF(!Init2(), CKR_GENERAL_ERROR);
  CK_BYTE_PTR data_bytes =
      static_cast<CK_BYTE_PTR>(const_cast<uint8_t*>(&data.front()));
  CK_BYTE_PTR sig_bytes =
      static_cast<CK_BYTE_PTR>(const_cast<uint8_t*>(&signature.front()));
  uint32_t result = functions_->C_Verify(session_id,
                                         data_bytes,
                                         data.size(),
                                         sig_bytes,
                                         signature.size());
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  return CKR_OK;
}

uint32_t ChapsServiceRedirect::VerifyUpdate(uint64_t session_id,
                                            const vector<uint8_t>& data_part) {
  LOG_CK_RV_AND_RETURN_IF(!Init2(), CKR_GENERAL_ERROR);
  CK_BYTE_PTR in_bytes =
      static_cast<CK_BYTE_PTR>(const_cast<uint8_t*>(&data_part.front()));
  uint32_t result = functions_->C_VerifyUpdate(session_id,
                                               in_bytes,
                                               data_part.size());
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  return CKR_OK;
}

uint32_t ChapsServiceRedirect::VerifyFinal(uint64_t session_id,
                                           const vector<uint8_t>& signature) {
  LOG_CK_RV_AND_RETURN_IF(!Init2(), CKR_GENERAL_ERROR);
  CK_BYTE_PTR in_bytes =
      static_cast<CK_BYTE_PTR>(const_cast<uint8_t*>(&signature.front()));
  uint32_t result = functions_->C_VerifyFinal(session_id,
                                              in_bytes,
                                              signature.size());
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  return CKR_OK;
}

uint32_t ChapsServiceRedirect::VerifyRecoverInit(
      uint64_t session_id,
      uint64_t mechanism_type,
      const vector<uint8_t>& mechanism_parameter,
      uint64_t key_handle) {
  LOG_CK_RV_AND_RETURN_IF(!Init2(), CKR_GENERAL_ERROR);
  CK_MECHANISM mechanism;
  mechanism.mechanism = static_cast<CK_MECHANISM_TYPE>(mechanism_type);
  mechanism.pParameter = const_cast<uint8_t*>(&mechanism_parameter.front());
  mechanism.ulParameterLen = mechanism_parameter.size();
  uint32_t result = functions_->C_VerifyRecoverInit(session_id,
                                                    &mechanism,
                                                    key_handle);
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  return CKR_OK;
}

uint32_t ChapsServiceRedirect::VerifyRecover(uint64_t session_id,
                                             const vector<uint8_t>& signature,
                                             uint64_t max_out_length,
                                             uint64_t* actual_out_length,
                                             vector<uint8_t>* data) {
  LOG_CK_RV_AND_RETURN_IF(!Init2(), CKR_GENERAL_ERROR);
  LOG_CK_RV_AND_RETURN_IF(!actual_out_length || !data, CKR_ARGUMENTS_BAD);
  CK_BYTE_PTR in_bytes =
      static_cast<CK_BYTE_PTR>(const_cast<uint8_t*>(&signature.front()));
  scoped_array<CK_BYTE> out_bytes(NULL);
  if (max_out_length) {
    out_bytes.reset(new CK_BYTE[max_out_length]);
    CHECK(out_bytes.get());
  }
  CK_ULONG length = max_out_length;
  uint32_t result = functions_->C_VerifyRecover(session_id,
                                                in_bytes,
                                                signature.size(),
                                                out_bytes.get(),
                                                &length);
  // Provide the length before checking the result. This handles cases like
  // CKR_BUFFER_TOO_SMALL.
  *actual_out_length = static_cast<uint64_t>(length);
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  *data = ConvertByteBufferToVector(out_bytes.get(), length);
  return CKR_OK;
}

uint32_t ChapsServiceRedirect::DigestEncryptUpdate(
    uint64_t session_id,
    const vector<uint8_t>& data_in,
    uint64_t max_out_length,
    uint64_t* actual_out_length,
    vector<uint8_t>* data_out) {
  LOG_CK_RV_AND_RETURN_IF(!Init2(), CKR_GENERAL_ERROR);
  LOG_CK_RV_AND_RETURN_IF(!actual_out_length || !data_out, CKR_ARGUMENTS_BAD);
  CK_BYTE_PTR in_bytes =
      static_cast<CK_BYTE_PTR>(const_cast<uint8_t*>(&data_in.front()));
  scoped_array<CK_BYTE> out_bytes(NULL);
  if (max_out_length) {
    out_bytes.reset(new CK_BYTE[max_out_length]);
    CHECK(out_bytes.get());
  }
  CK_ULONG length = max_out_length;
  uint32_t result = functions_->C_DigestEncryptUpdate(session_id,
                                                      in_bytes,
                                                      data_in.size(),
                                                      out_bytes.get(),
                                                      &length);
  // Provide the length before checking the result. This handles cases like
  // CKR_BUFFER_TOO_SMALL.
  *actual_out_length = static_cast<uint64_t>(length);
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  *data_out = ConvertByteBufferToVector(out_bytes.get(), length);
  return CKR_OK;
}

uint32_t ChapsServiceRedirect::DecryptDigestUpdate(
    uint64_t session_id,
    const vector<uint8_t>& data_in,
    uint64_t max_out_length,
    uint64_t* actual_out_length,
    vector<uint8_t>* data_out) {
  LOG_CK_RV_AND_RETURN_IF(!Init2(), CKR_GENERAL_ERROR);
  LOG_CK_RV_AND_RETURN_IF(!actual_out_length || !data_out, CKR_ARGUMENTS_BAD);
  CK_BYTE_PTR in_bytes =
      static_cast<CK_BYTE_PTR>(const_cast<uint8_t*>(&data_in.front()));
  scoped_array<CK_BYTE> out_bytes(NULL);
  if (max_out_length) {
    out_bytes.reset(new CK_BYTE[max_out_length]);
    CHECK(out_bytes.get());
  }
  CK_ULONG length = max_out_length;
  uint32_t result = functions_->C_DecryptDigestUpdate(session_id,
                                                      in_bytes,
                                                      data_in.size(),
                                                      out_bytes.get(),
                                                      &length);
  // Provide the length before checking the result. This handles cases like
  // CKR_BUFFER_TOO_SMALL.
  *actual_out_length = static_cast<uint64_t>(length);
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  *data_out = ConvertByteBufferToVector(out_bytes.get(), length);
  return CKR_OK;
}

uint32_t ChapsServiceRedirect::SignEncryptUpdate(
    uint64_t session_id,
    const vector<uint8_t>& data_in,
    uint64_t max_out_length,
    uint64_t* actual_out_length,
    vector<uint8_t>* data_out) {
  LOG_CK_RV_AND_RETURN_IF(!Init2(), CKR_GENERAL_ERROR);
  LOG_CK_RV_AND_RETURN_IF(!actual_out_length || !data_out, CKR_ARGUMENTS_BAD);
  CK_BYTE_PTR in_bytes =
      static_cast<CK_BYTE_PTR>(const_cast<uint8_t*>(&data_in.front()));
  scoped_array<CK_BYTE> out_bytes(NULL);
  if (max_out_length) {
    out_bytes.reset(new CK_BYTE[max_out_length]);
    CHECK(out_bytes.get());
  }
  CK_ULONG length = max_out_length;
  uint32_t result = functions_->C_SignEncryptUpdate(session_id,
                                                    in_bytes,
                                                    data_in.size(),
                                                    out_bytes.get(),
                                                    &length);
  // Provide the length before checking the result. This handles cases like
  // CKR_BUFFER_TOO_SMALL.
  *actual_out_length = static_cast<uint64_t>(length);
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  *data_out = ConvertByteBufferToVector(out_bytes.get(), length);
  return CKR_OK;
}

uint32_t ChapsServiceRedirect::DecryptVerifyUpdate(
    uint64_t session_id,
    const vector<uint8_t>& data_in,
    uint64_t max_out_length,
    uint64_t* actual_out_length,
    vector<uint8_t>* data_out) {
  LOG_CK_RV_AND_RETURN_IF(!Init2(), CKR_GENERAL_ERROR);
  LOG_CK_RV_AND_RETURN_IF(!actual_out_length || !data_out, CKR_ARGUMENTS_BAD);
  CK_BYTE_PTR in_bytes =
      static_cast<CK_BYTE_PTR>(const_cast<uint8_t*>(&data_in.front()));
  scoped_array<CK_BYTE> out_bytes(NULL);
  if (max_out_length) {
    out_bytes.reset(new CK_BYTE[max_out_length]);
    CHECK(out_bytes.get());
  }
  CK_ULONG length = max_out_length;
  uint32_t result = functions_->C_DecryptVerifyUpdate(session_id,
                                                      in_bytes,
                                                      data_in.size(),
                                                      out_bytes.get(),
                                                      &length);
  // Provide the length before checking the result. This handles cases like
  // CKR_BUFFER_TOO_SMALL.
  *actual_out_length = static_cast<uint64_t>(length);
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  *data_out = ConvertByteBufferToVector(out_bytes.get(), length);
  return CKR_OK;
}

uint32_t ChapsServiceRedirect::GenerateKey(
    uint64_t session_id,
    uint64_t mechanism_type,
    const vector<uint8_t>& mechanism_parameter,
    const vector<uint8_t>& attributes,
    uint64_t* key_handle) {
  LOG_CK_RV_AND_RETURN_IF(!Init2(), CKR_GENERAL_ERROR);
  LOG_CK_RV_AND_RETURN_IF(!key_handle, CKR_ARGUMENTS_BAD);
  CK_MECHANISM mechanism;
  mechanism.mechanism = static_cast<CK_MECHANISM_TYPE>(mechanism_type);
  mechanism.pParameter = const_cast<uint8_t*>(&mechanism_parameter.front());
  mechanism.ulParameterLen = mechanism_parameter.size();
  Attributes parsed_attributes;
  if (!parsed_attributes.Parse(attributes))
    LOG_CK_RV_AND_RETURN(CKR_TEMPLATE_INCONSISTENT);
  uint32_t result = functions_->C_GenerateKey(
      session_id,
      &mechanism,
      parsed_attributes.attributes(),
      parsed_attributes.num_attributes(),
      PreservedUint64_t(key_handle));
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  return CKR_OK;
}

uint32_t ChapsServiceRedirect::GenerateKeyPair(
    uint64_t session_id,
    uint64_t mechanism_type,
    const vector<uint8_t>& mechanism_parameter,
    const vector<uint8_t>& public_attributes,
    const vector<uint8_t>& private_attributes,
    uint64_t* public_key_handle,
    uint64_t* private_key_handle) {
  LOG_CK_RV_AND_RETURN_IF(!Init2(), CKR_GENERAL_ERROR);
  LOG_CK_RV_AND_RETURN_IF(!public_key_handle || !private_key_handle,
                          CKR_ARGUMENTS_BAD);
  CK_MECHANISM mechanism;
  mechanism.mechanism = static_cast<CK_MECHANISM_TYPE>(mechanism_type);
  mechanism.pParameter = const_cast<uint8_t*>(&mechanism_parameter.front());
  mechanism.ulParameterLen = mechanism_parameter.size();
  Attributes public_parsed_attributes;
  if (!public_parsed_attributes.Parse(public_attributes))
    LOG_CK_RV_AND_RETURN(CKR_TEMPLATE_INCONSISTENT);
  Attributes private_parsed_attributes;
  if (!private_parsed_attributes.Parse(private_attributes))
    LOG_CK_RV_AND_RETURN(CKR_TEMPLATE_INCONSISTENT);
  uint32_t result = functions_->C_GenerateKeyPair(
      session_id,
      &mechanism,
      public_parsed_attributes.attributes(),
      public_parsed_attributes.num_attributes(),
      private_parsed_attributes.attributes(),
      private_parsed_attributes.num_attributes(),
      PreservedUint64_t(public_key_handle),
      PreservedUint64_t(private_key_handle));
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  return CKR_OK;
}

uint32_t ChapsServiceRedirect::WrapKey(
    uint64_t session_id,
    uint64_t mechanism_type,
    const vector<uint8_t>& mechanism_parameter,
    uint64_t wrapping_key_handle,
    uint64_t key_handle,
    uint64_t max_out_length,
    uint64_t* actual_out_length,
    vector<uint8_t>* wrapped_key) {
  LOG_CK_RV_AND_RETURN_IF(!Init2(), CKR_GENERAL_ERROR);
  LOG_CK_RV_AND_RETURN_IF(!actual_out_length || !wrapped_key,
                          CKR_ARGUMENTS_BAD);
  CK_MECHANISM mechanism;
  mechanism.mechanism = static_cast<CK_MECHANISM_TYPE>(mechanism_type);
  mechanism.pParameter = const_cast<uint8_t*>(&mechanism_parameter.front());
  mechanism.ulParameterLen = mechanism_parameter.size();
  scoped_array<CK_BYTE> out_bytes(NULL);
  if (max_out_length) {
    out_bytes.reset(new CK_BYTE[max_out_length]);
    CHECK(out_bytes.get());
  }
  CK_ULONG length = max_out_length;
  uint32_t result = functions_->C_WrapKey(session_id,
                                          &mechanism,
                                          wrapping_key_handle,
                                          key_handle,
                                          out_bytes.get(),
                                          &length);
  // Provide the length before checking the result. This handles cases like
  // CKR_BUFFER_TOO_SMALL.
  *actual_out_length = static_cast<uint64_t>(length);
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  *wrapped_key = ConvertByteBufferToVector(out_bytes.get(), length);
  return CKR_OK;
}

uint32_t ChapsServiceRedirect::UnwrapKey(
    uint64_t session_id,
    uint64_t mechanism_type,
    const vector<uint8_t>& mechanism_parameter,
    uint64_t wrapping_key_handle,
    const vector<uint8_t>& wrapped_key,
    const vector<uint8_t>& attributes,
    uint64_t* key_handle) {
  LOG_CK_RV_AND_RETURN_IF(!Init2(), CKR_GENERAL_ERROR);
  LOG_CK_RV_AND_RETURN_IF(!key_handle, CKR_ARGUMENTS_BAD);
  CK_MECHANISM mechanism;
  mechanism.mechanism = static_cast<CK_MECHANISM_TYPE>(mechanism_type);
  mechanism.pParameter = const_cast<uint8_t*>(&mechanism_parameter.front());
  mechanism.ulParameterLen = mechanism_parameter.size();
  Attributes parsed_attributes;
  if (!parsed_attributes.Parse(attributes))
    LOG_CK_RV_AND_RETURN(CKR_TEMPLATE_INCONSISTENT);
  CK_BYTE_PTR in_bytes =
      static_cast<CK_BYTE_PTR>(const_cast<uint8_t*>(&wrapped_key.front()));
  uint32_t result = functions_->C_UnwrapKey(session_id,
                                            &mechanism,
                                            wrapping_key_handle,
                                            in_bytes,
                                            wrapped_key.size(),
                                            parsed_attributes.attributes(),
                                            parsed_attributes.num_attributes(),
                                            PreservedUint64_t(key_handle));
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  return CKR_OK;
}

uint32_t ChapsServiceRedirect::DeriveKey(
    uint64_t session_id,
    uint64_t mechanism_type,
    const vector<uint8_t>& mechanism_parameter,
    uint64_t base_key_handle,
    const vector<uint8_t>& attributes,
    uint64_t* key_handle) {
  LOG_CK_RV_AND_RETURN_IF(!Init2(), CKR_GENERAL_ERROR);
  LOG_CK_RV_AND_RETURN_IF(!key_handle, CKR_ARGUMENTS_BAD);
  CK_MECHANISM mechanism;
  mechanism.mechanism = static_cast<CK_MECHANISM_TYPE>(mechanism_type);
  mechanism.pParameter = const_cast<uint8_t*>(&mechanism_parameter.front());
  mechanism.ulParameterLen = mechanism_parameter.size();
  Attributes parsed_attributes;
  if (!parsed_attributes.Parse(attributes))
    LOG_CK_RV_AND_RETURN(CKR_TEMPLATE_INCONSISTENT);
  uint32_t result = functions_->C_DeriveKey(session_id,
                                            &mechanism,
                                            base_key_handle,
                                            parsed_attributes.attributes(),
                                            parsed_attributes.num_attributes(),
                                            PreservedUint64_t(key_handle));
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  return CKR_OK;
}

uint32_t ChapsServiceRedirect::SeedRandom(uint64_t session_id,
                                          const vector<uint8_t>& seed) {
  LOG_CK_RV_AND_RETURN_IF(!Init2(), CKR_GENERAL_ERROR);
  LOG_CK_RV_AND_RETURN_IF(seed.size() == 0, CKR_ARGUMENTS_BAD);
  CK_BYTE_PTR in_bytes =
      static_cast<CK_BYTE_PTR>(const_cast<uint8_t*>(&seed.front()));
  uint32_t result = functions_->C_SeedRandom(session_id, in_bytes, seed.size());
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  return CKR_OK;
}

uint32_t ChapsServiceRedirect::GenerateRandom(uint64_t session_id,
                                              uint64_t num_bytes,
                                              vector<uint8_t>* random_data) {
  LOG_CK_RV_AND_RETURN_IF(!Init2(), CKR_GENERAL_ERROR);
  LOG_CK_RV_AND_RETURN_IF(!random_data || num_bytes == 0, CKR_ARGUMENTS_BAD);
  scoped_array<CK_BYTE> out_bytes(new CK_BYTE[num_bytes]);
  CHECK(out_bytes.get());
  uint32_t result = functions_->C_GenerateRandom(session_id,
                                                 out_bytes.get(),
                                                 num_bytes);
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  *random_data = ConvertByteBufferToVector(out_bytes.get(), num_bytes);
  return CKR_OK;
}

}  // namespace
