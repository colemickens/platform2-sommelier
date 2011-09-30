// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is the Chaps client. Essentially it forwards all PKCS #11 calls to the
// Chaps Daemon (chapsd) via D-Bus.

#include <string>
#include <vector>

#include <base/basictypes.h>
#include <base/logging.h>
#include <base/synchronization/waitable_event.h>

#include "chaps/chaps.h"
#include "chaps/chaps_proxy.h"
#include "chaps/chaps_utility.h"
#include "pkcs11/cryptoki.h"

using base::WaitableEvent;
using std::string;
using std::vector;

static const CK_BYTE kChapsLibraryVersionMajor = 0;
static const CK_BYTE kChapsLibraryVersionMinor = 1;

// The global proxy instance. This is valid only when g_is_initialized is true.
static scoped_ptr<chaps::ChapsInterface> g_proxy;

// Set to true when using a mock proxy.
static bool g_is_using_mock = false;

// Set to true when C_Initialize has been called successfully.
// When not using a mock proxy this is synonymous with (g_proxy != NULL).
static bool g_is_initialized = false;

// This event is not signaled until C_Finalize is called and then becomes
// signaled.
static scoped_ptr<WaitableEvent> g_finalize_event;

// Tear down helper.
static void TearDown() {
  if (g_is_initialized && !g_is_using_mock)
    g_proxy.reset();
  g_is_initialized = false;
}

namespace chaps {

// Helpers to support a mock proxy (useful in testing).
void EnableMockProxy(ChapsInterface* proxy, bool is_initialized) {
  g_proxy.reset(proxy);
  g_is_using_mock = true;
  g_is_initialized = is_initialized;
}

void DisableMockProxy() {
  // We want to release, not reset b/c we don't own the mock proxy.
  ChapsInterface* ignore = g_proxy.release();
  (void)ignore;
  g_is_using_mock = false;
  g_is_initialized = false;
}

}  // namespace chaps

// The following functions are PKCS #11 entry points. They are intentionally
// in the root namespace and are declared 'extern "C"' in pkcs11.h.

// PKCS #11 v2.20 section 11.4 page 102.
// Connects to the D-Bus service.
CK_RV C_Initialize(CK_VOID_PTR pInitArgs) {
  LOG_CK_RV_AND_RETURN_IF(g_is_initialized, CKR_CRYPTOKI_ALREADY_INITIALIZED);
  // Validate args (if any).
  if (pInitArgs) {
    CK_C_INITIALIZE_ARGS_PTR args =
        reinterpret_cast<CK_C_INITIALIZE_ARGS_PTR>(pInitArgs);
    LOG_CK_RV_AND_RETURN_IF(args->pReserved, CKR_ARGUMENTS_BAD);
    // If one of the following is NULL, they all must be NULL.
    if ((!args->CreateMutex || !args->DestroyMutex ||
         !args->LockMutex || !args->UnlockMutex) &&
        (args->CreateMutex || args->DestroyMutex ||
         args->LockMutex || args->UnlockMutex)) {
      LOG_CK_RV_AND_RETURN(CKR_ARGUMENTS_BAD);
    }
    // We require OS locking.
    if (((args->flags & CKF_OS_LOCKING_OK) == 0) && args->CreateMutex) {
      LOG_CK_RV_AND_RETURN(CKR_CANT_LOCK);
    }
  }
  // If we're not using a mock proxy instance we need to create one.
  if (!g_is_using_mock)
    g_proxy.reset(new chaps::ChapsProxyImpl());
  CHECK(g_proxy.get());

  g_finalize_event.reset(new WaitableEvent(true, false));
  CHECK(g_finalize_event.get());

  g_is_initialized = true;
  return CKR_OK;
}

// PKCS #11 v2.20 section 11.4 page 104.
// Closes the D-Bus service connection.
CK_RV C_Finalize(CK_VOID_PTR pReserved) {
  LOG_CK_RV_AND_RETURN_IF(pReserved, CKR_ARGUMENTS_BAD);
  LOG_CK_RV_AND_RETURN_IF(!g_is_initialized, CKR_CRYPTOKI_NOT_INITIALIZED);

  if (g_finalize_event.get())
    g_finalize_event->Signal();
  TearDown();
  return CKR_OK;
}

// PKCS #11 v2.20 section 11.4 page 105.
// Provide library info locally.
// TODO(dkrahn): i18n of strings - crosbug.com/20637
CK_RV C_GetInfo(CK_INFO_PTR pInfo) {
  LOG_CK_RV_AND_RETURN_IF(!g_is_initialized, CKR_CRYPTOKI_NOT_INITIALIZED);
  LOG_CK_RV_AND_RETURN_IF(!pInfo, CKR_ARGUMENTS_BAD);

  pInfo->cryptokiVersion.major = CRYPTOKI_VERSION_MAJOR;
  pInfo->cryptokiVersion.minor = CRYPTOKI_VERSION_MINOR;
  chaps::CopyToCharBuffer("Chromium OS", pInfo->manufacturerID,
                          arraysize(pInfo->manufacturerID));
  pInfo->flags = 0;
  chaps::CopyToCharBuffer("Chaps Client Library", pInfo->libraryDescription,
                          arraysize(pInfo->libraryDescription));
  pInfo->libraryVersion.major = kChapsLibraryVersionMajor;
  pInfo->libraryVersion.minor = kChapsLibraryVersionMinor;
  return CKR_OK;
}

// PKCS #11 v2.20 section 11.5 page 106.
CK_RV C_GetSlotList(CK_BBOOL tokenPresent,
                    CK_SLOT_ID_PTR pSlotList,
                    CK_ULONG_PTR pulCount) {
  LOG_CK_RV_AND_RETURN_IF(!g_is_initialized, CKR_CRYPTOKI_NOT_INITIALIZED);
  LOG_CK_RV_AND_RETURN_IF(!pulCount, CKR_ARGUMENTS_BAD);

  vector<uint32_t> slot_list;
  CK_RV result = g_proxy->GetSlotList((tokenPresent != CK_FALSE), &slot_list);
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  size_t max_copy = static_cast<size_t>(*pulCount);
  *pulCount = static_cast<CK_ULONG>(slot_list.size());
  if (!pSlotList)
    return CKR_OK;
  LOG_CK_RV_AND_RETURN_IF(slot_list.size() > max_copy, CKR_BUFFER_TOO_SMALL);
  for (size_t i = 0; i < slot_list.size(); i++) {
    pSlotList[i] = slot_list[i];
  }
  return CKR_OK;
}

// PKCS #11 v2.20 section 11.5 page 108.
CK_RV C_GetSlotInfo(CK_SLOT_ID slotID, CK_SLOT_INFO_PTR pInfo) {
  LOG_CK_RV_AND_RETURN_IF(!g_is_initialized, CKR_CRYPTOKI_NOT_INITIALIZED);
  LOG_CK_RV_AND_RETURN_IF(!pInfo, CKR_ARGUMENTS_BAD);

  string slot_description;
  string manufacturer_id;
  CK_RV result = g_proxy->GetSlotInfo(
      slotID,
      &slot_description,
      &manufacturer_id,
      chaps::PreservedCK_ULONG(&pInfo->flags),
      static_cast<uint8_t*>(&pInfo->hardwareVersion.major),
      static_cast<uint8_t*>(&pInfo->hardwareVersion.minor),
      static_cast<uint8_t*>(&pInfo->firmwareVersion.major),
      static_cast<uint8_t*>(&pInfo->firmwareVersion.minor));
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  chaps::CopyToCharBuffer(slot_description.c_str(), pInfo->slotDescription,
                          arraysize(pInfo->slotDescription));
  chaps::CopyToCharBuffer(manufacturer_id.c_str(), pInfo->manufacturerID,
                          arraysize(pInfo->manufacturerID));
  return CKR_OK;
}

// PKCS #11 v2.20 section 11.5 page 109.
CK_RV C_GetTokenInfo(CK_SLOT_ID slotID, CK_TOKEN_INFO_PTR pInfo) {
  LOG_CK_RV_AND_RETURN_IF(!g_is_initialized, CKR_CRYPTOKI_NOT_INITIALIZED);
  LOG_CK_RV_AND_RETURN_IF(!pInfo, CKR_ARGUMENTS_BAD);

  string label;
  string manufacturer_id;
  string model;
  string serial_number;
  CK_RV result = g_proxy->GetTokenInfo(
      slotID,
      &label,
      &manufacturer_id,
      &model,
      &serial_number,
      chaps::PreservedCK_ULONG(&pInfo->flags),
      chaps::PreservedCK_ULONG(&pInfo->ulMaxSessionCount),
      chaps::PreservedCK_ULONG(&pInfo->ulSessionCount),
      chaps::PreservedCK_ULONG(&pInfo->ulMaxRwSessionCount),
      chaps::PreservedCK_ULONG(&pInfo->ulRwSessionCount),
      chaps::PreservedCK_ULONG(&pInfo->ulMaxPinLen),
      chaps::PreservedCK_ULONG(&pInfo->ulMinPinLen),
      chaps::PreservedCK_ULONG(&pInfo->ulTotalPublicMemory),
      chaps::PreservedCK_ULONG(&pInfo->ulFreePublicMemory),
      chaps::PreservedCK_ULONG(&pInfo->ulTotalPrivateMemory),
      chaps::PreservedCK_ULONG(&pInfo->ulFreePrivateMemory),
      static_cast<uint8_t*>(&pInfo->hardwareVersion.major),
      static_cast<uint8_t*>(&pInfo->hardwareVersion.minor),
      static_cast<uint8_t*>(&pInfo->firmwareVersion.major),
      static_cast<uint8_t*>(&pInfo->firmwareVersion.minor));
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  chaps::CopyToCharBuffer(label.c_str(), pInfo->label, arraysize(pInfo->label));
  chaps::CopyToCharBuffer(manufacturer_id.c_str(), pInfo->manufacturerID,
                          arraysize(pInfo->manufacturerID));
  chaps::CopyToCharBuffer(model.c_str(), pInfo->model, arraysize(pInfo->model));
  chaps::CopyToCharBuffer(serial_number.c_str(), pInfo->serialNumber,
                          arraysize(pInfo->serialNumber));
  return CKR_OK;
}

// PKCS #11 v2.20 section 11.5 page 110.
// Currently, slot events via D-Bus are not supported because no slot events
// occur with TPM-based tokens.  We want this call to behave properly so we'll
// block the calling thread (if not CKF_DONT_BLOCK) until C_Finalize is called.
CK_RV C_WaitForSlotEvent(CK_FLAGS flags,
                         CK_SLOT_ID_PTR pSlot,
                         CK_VOID_PTR pReserved) {
  LOG_CK_RV_AND_RETURN_IF(!g_is_initialized, CKR_CRYPTOKI_NOT_INITIALIZED);
  LOG_CK_RV_AND_RETURN_IF(!pSlot, CKR_ARGUMENTS_BAD);

  // Currently, all supported tokens are not removable - i.e. no slot events.
  if (CKF_DONT_BLOCK & flags)
    return CKR_NO_EVENT;
  // Block until C_Finalize.
  CHECK(g_finalize_event.get());
  g_finalize_event->Wait();
  return CKR_CRYPTOKI_NOT_INITIALIZED;
}

// PKCS #11 v2.20 section 11.5 page 111.
CK_RV C_GetMechanismList(CK_SLOT_ID slotID,
                         CK_MECHANISM_TYPE_PTR pMechanismList,
                         CK_ULONG_PTR pulCount) {
  LOG_CK_RV_AND_RETURN_IF(!g_is_initialized, CKR_CRYPTOKI_NOT_INITIALIZED);
  LOG_CK_RV_AND_RETURN_IF(!pulCount, CKR_ARGUMENTS_BAD);

  vector<uint32_t> mechanism_list;
  CK_RV result = g_proxy->GetMechanismList(slotID, &mechanism_list);
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  // Copy the mechanism list to caller-supplied memory.
  size_t max_copy = static_cast<size_t>(*pulCount);
  *pulCount = static_cast<CK_ULONG>(mechanism_list.size());
  if (!pMechanismList)
    return CKR_OK;
  LOG_CK_RV_AND_RETURN_IF(mechanism_list.size() > max_copy,
                          CKR_BUFFER_TOO_SMALL);
  for (size_t i = 0; i < mechanism_list.size(); i++) {
    pMechanismList[i] = static_cast<CK_MECHANISM_TYPE>(mechanism_list[i]);
  }
  return CKR_OK;
}

// PKCS #11 v2.20 section 11.5 page 112.
CK_RV C_GetMechanismInfo(CK_SLOT_ID slotID,
                         CK_MECHANISM_TYPE type,
                         CK_MECHANISM_INFO_PTR pInfo) {
  LOG_CK_RV_AND_RETURN_IF(!g_is_initialized, CKR_CRYPTOKI_NOT_INITIALIZED);
  LOG_CK_RV_AND_RETURN_IF(!pInfo, CKR_ARGUMENTS_BAD);

  vector<uint32_t> mechanism_list;
  CK_RV result = g_proxy->GetMechanismInfo(
      slotID,
      type,
      chaps::PreservedCK_ULONG(&pInfo->ulMinKeySize),
      chaps::PreservedCK_ULONG(&pInfo->ulMaxKeySize),
      chaps::PreservedCK_ULONG(&pInfo->flags));
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  return CKR_OK;
}

// PKCS #11 v2.20 section 11.5 page 113.
CK_RV C_InitToken(CK_SLOT_ID slotID,
                  CK_UTF8CHAR_PTR pPin,
                  CK_ULONG ulPinLen,
                  CK_UTF8CHAR_PTR pLabel) {
  LOG_CK_RV_AND_RETURN_IF(!g_is_initialized, CKR_CRYPTOKI_NOT_INITIALIZED);
  LOG_CK_RV_AND_RETURN_IF(!pLabel, CKR_ARGUMENTS_BAD);

  string pin = chaps::CharBufferToString(pPin, ulPinLen);
  string label = chaps::CharBufferToString(pLabel, chaps::kTokenLabelSize);
  string* pin_ptr = (!pPin) ? NULL : &pin;
  CK_RV result = g_proxy->InitToken(slotID, pin_ptr, label);
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  return CKR_OK;
}

// PKCS #11 v2.20 section 11.5 page 115.
CK_RV C_InitPIN(CK_SESSION_HANDLE hSession,
                CK_UTF8CHAR_PTR pPin,
                CK_ULONG ulPinLen) {
  LOG_CK_RV_AND_RETURN_IF(!g_is_initialized, CKR_CRYPTOKI_NOT_INITIALIZED);

  string pin = chaps::CharBufferToString(pPin, ulPinLen);
  string* pin_ptr = (!pPin) ? NULL : &pin;
  CK_RV result = g_proxy->InitPIN(hSession, pin_ptr);
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  return CKR_OK;
}

// PKCS #11 v2.20 section 11.5 page 116.
CK_RV C_SetPIN(CK_SESSION_HANDLE hSession,
               CK_UTF8CHAR_PTR pOldPin,
               CK_ULONG ulOldLen,
               CK_UTF8CHAR_PTR pNewPin,
               CK_ULONG ulNewLen) {
  LOG_CK_RV_AND_RETURN_IF(!g_is_initialized, CKR_CRYPTOKI_NOT_INITIALIZED);

  string old_pin = chaps::CharBufferToString(pOldPin, ulOldLen);
  string* old_pin_ptr = (!pOldPin) ? NULL : &old_pin;
  string new_pin = chaps::CharBufferToString(pNewPin, ulNewLen);
  string* new_pin_ptr = (!pNewPin) ? NULL : &new_pin;
  CK_RV result = g_proxy->SetPIN(hSession, old_pin_ptr, new_pin_ptr);
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  return CKR_OK;
}
