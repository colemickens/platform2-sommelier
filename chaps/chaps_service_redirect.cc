// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chaps/chaps_service_redirect.h"

#include <dlfcn.h>

#include <base/logging.h>
#include <base/scoped_ptr.h>

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
    return CKR_ARGUMENTS_BAD;

  uint32_t result = CKR_OK;
  CK_ULONG count = 0;
  // First, call with NULL to retrieve the slot count.
  result = functions_->C_GetSlotList(token_present, NULL, &count);
  LOG_CK_RV_ERR_RETURN(result);
  scoped_array<CK_SLOT_ID> slot_array(new CK_SLOT_ID[count]);
  CHECK(slot_array.get()) << "GetSlotList out of memory.";
  // Now, query the actual list.
  result = functions_->C_GetSlotList(token_present, slot_array.get(), &count);
  LOG_CK_RV_ERR_RETURN(result);
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
    return CKR_ARGUMENTS_BAD;
  }

  uint32_t result = CKR_OK;
  CK_SLOT_INFO slot_info;
  result = functions_->C_GetSlotInfo(slot_id, &slot_info);
  LOG_CK_RV_ERR_RETURN(result);
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
  uint32_t result = CKR_OK;
  CK_TOKEN_INFO token_info;
  result = functions_->C_GetTokenInfo(slot_id, &token_info);
  LOG_CK_RV_ERR_RETURN(result);
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

}  // namespace
