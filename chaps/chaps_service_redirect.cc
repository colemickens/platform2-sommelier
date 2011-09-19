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

}  // namespace
