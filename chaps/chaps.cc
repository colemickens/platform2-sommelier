// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is the Chaps client. Essentially it forwards all PKCS #11 calls to the
// Chaps Daemon (chapsd) via D-Bus.

#include <string>
#include <vector>

#include <base/basictypes.h>
#include <base/logging.h>
#include <base/synchronization/waitable_event.h>

#include "chaps/attributes.h"
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

// This function implements the output handling convention described in
// PKCS #11 section 11.2.  This method handles the following cases:
// 1) Caller passes a NULL buffer.
// 2) Caller passes a buffer that's too small.
// 3) Caller passes a buffer that is large enough.
// Parameters:
//    result - The result of the operation as returned by chapsd.  This will be
//             clobbered if an error occurs, otherwise it is returned as is.
//    output - The output of the operation as provided by chapsd.  This should
//             always fit in the caller-supplied output buffer.
//    output_length - The output length as provided by chapsd.  This is used
//                    when no data, only the length has been provided by chapsd.
//    out_buffer - The caller-supplied output buffer; this may be NULL.
//    out_buffer_length - The caller-supplied output buffer length, in bytes.
//                        This will be updated with the actual output length.
static CK_RV HandlePKCS11Output(CK_RV result,
                                const vector<uint8_t>& output,
                                uint64_t output_length,
                                CK_BYTE_PTR out_buffer,
                                CK_ULONG_PTR out_buffer_length) {
  if (result == CKR_OK && out_buffer) {
    if (output.size() > *out_buffer_length)
      return CKR_GENERAL_ERROR;
    *out_buffer_length = output.size();
    memcpy(out_buffer, &output.front(), output.size());
  } else {
    *out_buffer_length = static_cast<CK_ULONG>(output_length);
    if (result == CKR_BUFFER_TOO_SMALL)
      result = CKR_OK;
  }
  return result;
}

namespace chaps {

// Helpers to support a mock proxy (useful in testing).
EXPORT_SPEC void EnableMockProxy(ChapsInterface* proxy, bool is_initialized) {
  g_proxy.reset(proxy);
  g_is_using_mock = true;
  g_is_initialized = is_initialized;
}

EXPORT_SPEC void DisableMockProxy() {
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
  if (!g_is_using_mock) {
    scoped_ptr<chaps::ChapsProxyImpl> proxy(new chaps::ChapsProxyImpl());
    CHECK(proxy.get());
    if (!proxy->Init())
      LOG_CK_RV_AND_RETURN(CKR_GENERAL_ERROR);
    g_proxy.reset(proxy.release());
  }
  CHECK(g_proxy.get());

  g_finalize_event.reset(new WaitableEvent(true, false));
  CHECK(g_finalize_event.get());

  g_is_initialized = true;
  VLOG(1) << __func__ << " - CKR_OK";
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
  VLOG(1) << __func__ << " - CKR_OK";
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
  chaps::CopyStringToCharBuffer("Chromium OS",
                                pInfo->manufacturerID,
                                arraysize(pInfo->manufacturerID));
  pInfo->flags = 0;
  chaps::CopyStringToCharBuffer("Chaps Client Library",
                                pInfo->libraryDescription,
                                arraysize(pInfo->libraryDescription));
  pInfo->libraryVersion.major = kChapsLibraryVersionMajor;
  pInfo->libraryVersion.minor = kChapsLibraryVersionMinor;
  VLOG(1) << __func__ << " - CKR_OK";
  return CKR_OK;
}

// PKCS #11 v2.20 section 11.4 page 106.
CK_RV C_GetFunctionList(CK_FUNCTION_LIST_PTR_PTR ppFunctionList) {
  static CK_VERSION version = {2, 20};
  static CK_FUNCTION_LIST functionList = {
    version,
    // Let pkcs11f.h populate the function pointers in order.
#define CK_PKCS11_FUNCTION_INFO(func) &func,
    // We want only the function names, not the arguments.
#undef CK_NEED_ARG_LIST
#include "pkcs11/pkcs11f.h"
#undef CK_PKCS11_FUNCTION_INFO
  };
  *ppFunctionList = &functionList;
  VLOG(1) << __func__ << " - CKR_OK";
  return CKR_OK;
}

// PKCS #11 v2.20 section 11.5 page 106.
CK_RV C_GetSlotList(CK_BBOOL tokenPresent,
                    CK_SLOT_ID_PTR pSlotList,
                    CK_ULONG_PTR pulCount) {
  LOG_CK_RV_AND_RETURN_IF(!g_is_initialized, CKR_CRYPTOKI_NOT_INITIALIZED);
  LOG_CK_RV_AND_RETURN_IF(!pulCount, CKR_ARGUMENTS_BAD);
  vector<uint64_t> slot_list;
  CK_RV result = g_proxy->GetSlotList((tokenPresent != CK_FALSE), &slot_list);
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  size_t max_copy = static_cast<size_t>(*pulCount);
  *pulCount = static_cast<CK_ULONG>(slot_list.size());
  if (!pSlotList)
    return CKR_OK;
  LOG_CK_RV_AND_RETURN_IF(slot_list.size() > max_copy, CKR_BUFFER_TOO_SMALL);
  for (size_t i = 0; i < slot_list.size(); ++i) {
    pSlotList[i] = slot_list[i];
  }
  VLOG(1) << __func__ << " - CKR_OK";
  return CKR_OK;
}

// PKCS #11 v2.20 section 11.5 page 108.
CK_RV C_GetSlotInfo(CK_SLOT_ID slotID, CK_SLOT_INFO_PTR pInfo) {
  LOG_CK_RV_AND_RETURN_IF(!g_is_initialized, CKR_CRYPTOKI_NOT_INITIALIZED);
  LOG_CK_RV_AND_RETURN_IF(!pInfo, CKR_ARGUMENTS_BAD);
  vector<uint8_t> slot_description;
  vector<uint8_t> manufacturer_id;
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
  chaps::CopyVectorToCharBuffer(slot_description,
                                pInfo->slotDescription,
                                arraysize(pInfo->slotDescription));
  chaps::CopyVectorToCharBuffer(manufacturer_id,
                                pInfo->manufacturerID,
                                arraysize(pInfo->manufacturerID));
  VLOG(1) << __func__ << " - CKR_OK";
  return CKR_OK;
}

// PKCS #11 v2.20 section 11.5 page 109.
CK_RV C_GetTokenInfo(CK_SLOT_ID slotID, CK_TOKEN_INFO_PTR pInfo) {
  LOG_CK_RV_AND_RETURN_IF(!g_is_initialized, CKR_CRYPTOKI_NOT_INITIALIZED);
  LOG_CK_RV_AND_RETURN_IF(!pInfo, CKR_ARGUMENTS_BAD);
  vector<uint8_t> label;
  vector<uint8_t> manufacturer_id;
  vector<uint8_t> model;
  vector<uint8_t> serial_number;
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
  chaps::CopyVectorToCharBuffer(label,
                                pInfo->label,
                                arraysize(pInfo->label));
  chaps::CopyVectorToCharBuffer(manufacturer_id,
                                pInfo->manufacturerID,
                                arraysize(pInfo->manufacturerID));
  chaps::CopyVectorToCharBuffer(model,
                                pInfo->model,
                                arraysize(pInfo->model));
  chaps::CopyVectorToCharBuffer(serial_number,
                                pInfo->serialNumber,
                                arraysize(pInfo->serialNumber));
  VLOG(1) << __func__ << " - CKR_OK";
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
  vector<uint64_t> mechanism_list;
  CK_RV result = g_proxy->GetMechanismList(slotID, &mechanism_list);
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  // Copy the mechanism list to caller-supplied memory.
  size_t max_copy = static_cast<size_t>(*pulCount);
  *pulCount = static_cast<CK_ULONG>(mechanism_list.size());
  if (!pMechanismList)
    return CKR_OK;
  LOG_CK_RV_AND_RETURN_IF(mechanism_list.size() > max_copy,
                          CKR_BUFFER_TOO_SMALL);
  for (size_t i = 0; i < mechanism_list.size(); ++i) {
    pMechanismList[i] = static_cast<CK_MECHANISM_TYPE>(mechanism_list[i]);
  }
  VLOG(1) << __func__ << " - CKR_OK";
  return CKR_OK;
}

// PKCS #11 v2.20 section 11.5 page 112.
CK_RV C_GetMechanismInfo(CK_SLOT_ID slotID,
                         CK_MECHANISM_TYPE type,
                         CK_MECHANISM_INFO_PTR pInfo) {
  LOG_CK_RV_AND_RETURN_IF(!g_is_initialized, CKR_CRYPTOKI_NOT_INITIALIZED);
  LOG_CK_RV_AND_RETURN_IF(!pInfo, CKR_ARGUMENTS_BAD);
  vector<uint64_t> mechanism_list;
  CK_RV result = g_proxy->GetMechanismInfo(
      slotID,
      type,
      chaps::PreservedCK_ULONG(&pInfo->ulMinKeySize),
      chaps::PreservedCK_ULONG(&pInfo->ulMaxKeySize),
      chaps::PreservedCK_ULONG(&pInfo->flags));
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  VLOG(1) << __func__ << " - CKR_OK";
  return CKR_OK;
}

// PKCS #11 v2.20 section 11.5 page 113.
CK_RV C_InitToken(CK_SLOT_ID slotID,
                  CK_UTF8CHAR_PTR pPin,
                  CK_ULONG ulPinLen,
                  CK_UTF8CHAR_PTR pLabel) {
  LOG_CK_RV_AND_RETURN_IF(!g_is_initialized, CKR_CRYPTOKI_NOT_INITIALIZED);
  LOG_CK_RV_AND_RETURN_IF(!pLabel, CKR_ARGUMENTS_BAD);
  string pin = chaps::ConvertCharBufferToString(pPin, ulPinLen);
  vector<uint8_t> label =
      chaps::ConvertByteBufferToVector(pLabel, chaps::kTokenLabelSize);
  string* pin_ptr = (!pPin) ? NULL : &pin;
  CK_RV result = g_proxy->InitToken(slotID, pin_ptr, label);
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  VLOG(1) << __func__ << " - CKR_OK";
  return CKR_OK;
}

// PKCS #11 v2.20 section 11.5 page 115.
CK_RV C_InitPIN(CK_SESSION_HANDLE hSession,
                CK_UTF8CHAR_PTR pPin,
                CK_ULONG ulPinLen) {
  LOG_CK_RV_AND_RETURN_IF(!g_is_initialized, CKR_CRYPTOKI_NOT_INITIALIZED);
  string pin = chaps::ConvertCharBufferToString(pPin, ulPinLen);
  string* pin_ptr = (!pPin) ? NULL : &pin;
  CK_RV result = g_proxy->InitPIN(hSession, pin_ptr);
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  VLOG(1) << __func__ << " - CKR_OK";
  return CKR_OK;
}

// PKCS #11 v2.20 section 11.5 page 116.
CK_RV C_SetPIN(CK_SESSION_HANDLE hSession,
               CK_UTF8CHAR_PTR pOldPin,
               CK_ULONG ulOldLen,
               CK_UTF8CHAR_PTR pNewPin,
               CK_ULONG ulNewLen) {
  LOG_CK_RV_AND_RETURN_IF(!g_is_initialized, CKR_CRYPTOKI_NOT_INITIALIZED);
  string old_pin = chaps::ConvertCharBufferToString(pOldPin, ulOldLen);
  string* old_pin_ptr = (!pOldPin) ? NULL : &old_pin;
  string new_pin = chaps::ConvertCharBufferToString(pNewPin, ulNewLen);
  string* new_pin_ptr = (!pNewPin) ? NULL : &new_pin;
  CK_RV result = g_proxy->SetPIN(hSession, old_pin_ptr, new_pin_ptr);
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  VLOG(1) << __func__ << " - CKR_OK";
  return CKR_OK;
}

// PKCS #11 v2.20 section 11.6 page 117.
CK_RV C_OpenSession(CK_SLOT_ID slotID,
                    CK_FLAGS flags,
                    CK_VOID_PTR pApplication,
                    CK_NOTIFY Notify,
                    CK_SESSION_HANDLE_PTR phSession) {
  LOG_CK_RV_AND_RETURN_IF(!g_is_initialized, CKR_CRYPTOKI_NOT_INITIALIZED);
  LOG_CK_RV_AND_RETURN_IF(!phSession, CKR_ARGUMENTS_BAD);
  // pApplication and Notify are intentionally ignored.  We don't support
  // notification callbacks and the PKCS #11 specification does not require us
  // to.  See PKCS #11 v2.20 section 11.17 for details.
  CK_RV result = g_proxy->OpenSession(slotID, flags,
                                      chaps::PreservedCK_ULONG(phSession));
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  VLOG(1) << __func__ << " - CKR_OK";
  return CKR_OK;
}

// PKCS #11 v2.20 section 11.6 page 118.
CK_RV C_CloseSession(CK_SESSION_HANDLE hSession) {
  LOG_CK_RV_AND_RETURN_IF(!g_is_initialized, CKR_CRYPTOKI_NOT_INITIALIZED);
  CK_RV result = g_proxy->CloseSession(hSession);
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  VLOG(1) << __func__ << " - CKR_OK";
  return CKR_OK;
}

// PKCS #11 v2.20 section 11.6 page 120.
CK_RV C_CloseAllSessions(CK_SLOT_ID slotID) {
  LOG_CK_RV_AND_RETURN_IF(!g_is_initialized, CKR_CRYPTOKI_NOT_INITIALIZED);
  CK_RV result = g_proxy->CloseAllSessions(slotID);
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  VLOG(1) << __func__ << " - CKR_OK";
  return CKR_OK;
}

// PKCS #11 v2.20 section 11.6 page 120.
CK_RV C_GetSessionInfo(CK_SESSION_HANDLE hSession, CK_SESSION_INFO_PTR pInfo) {
  LOG_CK_RV_AND_RETURN_IF(!g_is_initialized, CKR_CRYPTOKI_NOT_INITIALIZED);
  LOG_CK_RV_AND_RETURN_IF(!pInfo, CKR_ARGUMENTS_BAD);

  CK_RV result = g_proxy->GetSessionInfo(
      hSession,
      chaps::PreservedCK_ULONG(&pInfo->slotID),
      chaps::PreservedCK_ULONG(&pInfo->state),
      chaps::PreservedCK_ULONG(&pInfo->flags),
      chaps::PreservedCK_ULONG(&pInfo->ulDeviceError));
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  VLOG(1) << __func__ << " - CKR_OK";
  return CKR_OK;
}

// PKCS #11 v2.20 section 11.6 page 121.
CK_RV C_GetOperationState(CK_SESSION_HANDLE hSession,
                          CK_BYTE_PTR pOperationState,
                          CK_ULONG_PTR pulOperationStateLen) {
  LOG_CK_RV_AND_RETURN_IF(!g_is_initialized, CKR_CRYPTOKI_NOT_INITIALIZED);
  LOG_CK_RV_AND_RETURN_IF(!pulOperationStateLen, CKR_ARGUMENTS_BAD);

  vector<uint8_t> operation_state;
  CK_RV result = g_proxy->GetOperationState(hSession, &operation_state);
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  // Copy the data and length to caller-supplied memory.
  size_t max_copy = static_cast<size_t>(*pulOperationStateLen);
  *pulOperationStateLen = static_cast<CK_ULONG>(operation_state.size());
  if (!pOperationState)
    return CKR_OK;
  LOG_CK_RV_AND_RETURN_IF(operation_state.size() > max_copy,
                          CKR_BUFFER_TOO_SMALL);
  memcpy(pOperationState, &operation_state.front(), operation_state.size());
  VLOG(1) << __func__ << " - CKR_OK";
  return CKR_OK;
}

// PKCS #11 v2.20 section 11.6 page 123.
CK_RV C_SetOperationState(CK_SESSION_HANDLE hSession,
                          CK_BYTE_PTR pOperationState,
                          CK_ULONG ulOperationStateLen,
                          CK_OBJECT_HANDLE hEncryptionKey,
                          CK_OBJECT_HANDLE hAuthenticationKey) {
  LOG_CK_RV_AND_RETURN_IF(!g_is_initialized, CKR_CRYPTOKI_NOT_INITIALIZED);
  LOG_CK_RV_AND_RETURN_IF(!pOperationState, CKR_ARGUMENTS_BAD);

  vector<uint8_t> operation_state =
      chaps::ConvertByteBufferToVector(pOperationState,
                                              ulOperationStateLen);
  CK_RV result = g_proxy->SetOperationState(hSession,
                                            operation_state,
                                            hEncryptionKey,
                                            hAuthenticationKey);
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  VLOG(1) << __func__ << " - CKR_OK";
  return CKR_OK;
}

// PKCS #11 v2.20 section 11.6 page 125.
CK_RV C_Login(CK_SESSION_HANDLE hSession,
              CK_USER_TYPE userType,
              CK_UTF8CHAR_PTR pPin,
              CK_ULONG ulPinLen) {
  LOG_CK_RV_AND_RETURN_IF(!g_is_initialized, CKR_CRYPTOKI_NOT_INITIALIZED);
  string pin = chaps::ConvertCharBufferToString(pPin, ulPinLen);
  string* pin_ptr = (!pPin) ? NULL : &pin;
  CK_RV result = g_proxy->Login(hSession, userType, pin_ptr);
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  VLOG(1) << __func__ << " - CKR_OK";
  return CKR_OK;
}

// PKCS #11 v2.20 section 11.6 page 127.
CK_RV C_Logout(CK_SESSION_HANDLE hSession) {
  LOG_CK_RV_AND_RETURN_IF(!g_is_initialized, CKR_CRYPTOKI_NOT_INITIALIZED);
  CK_RV result = g_proxy->Logout(hSession);
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  VLOG(1) << __func__ << " - CKR_OK";
  return CKR_OK;
}

// PKCS #11 v2.20 section 11.7 page 128.
CK_RV C_CreateObject(CK_SESSION_HANDLE hSession,
                     CK_ATTRIBUTE_PTR pTemplate,
                     CK_ULONG ulCount,
                     CK_OBJECT_HANDLE_PTR phObject) {
  LOG_CK_RV_AND_RETURN_IF(!g_is_initialized, CKR_CRYPTOKI_NOT_INITIALIZED);
  if (pTemplate == NULL_PTR || phObject == NULL_PTR)
    LOG_CK_RV_AND_RETURN(CKR_ARGUMENTS_BAD);
  chaps::Attributes attributes(pTemplate, ulCount);
  vector<uint8_t> serialized_attributes;
  if (!attributes.Serialize(&serialized_attributes))
    LOG_CK_RV_AND_RETURN(CKR_TEMPLATE_INCONSISTENT);
  CK_RV result = g_proxy->CreateObject(
      hSession,
      serialized_attributes,
      chaps::PreservedCK_ULONG(phObject));
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  VLOG(1) << __func__ << " - CKR_OK";
  return CKR_OK;
}

// PKCS #11 v2.20 section 11.7 page 130.
CK_RV C_CopyObject(CK_SESSION_HANDLE hSession,
                   CK_OBJECT_HANDLE hObject,
                   CK_ATTRIBUTE_PTR pTemplate,
                   CK_ULONG ulCount,
                   CK_OBJECT_HANDLE_PTR phNewObject) {
  LOG_CK_RV_AND_RETURN_IF(!g_is_initialized, CKR_CRYPTOKI_NOT_INITIALIZED);
  if (pTemplate == NULL_PTR || phNewObject == NULL_PTR)
    LOG_CK_RV_AND_RETURN(CKR_ARGUMENTS_BAD);
  chaps::Attributes attributes(pTemplate, ulCount);
  vector<uint8_t> serialized_attributes;
  if (!attributes.Serialize(&serialized_attributes))
    LOG_CK_RV_AND_RETURN(CKR_TEMPLATE_INCONSISTENT);
  CK_RV result = g_proxy->CopyObject(
      hSession,
      hObject,
      serialized_attributes,
      chaps::PreservedCK_ULONG(phNewObject));
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  VLOG(1) << __func__ << " - CKR_OK";
  return CKR_OK;
}

// PKCS #11 v2.20 section 11.7 page 131.
CK_RV C_DestroyObject(CK_SESSION_HANDLE hSession, CK_OBJECT_HANDLE hObject) {
  LOG_CK_RV_AND_RETURN_IF(!g_is_initialized, CKR_CRYPTOKI_NOT_INITIALIZED);
  CK_RV result = g_proxy->DestroyObject(hSession, hObject);
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  VLOG(1) << __func__ << " - CKR_OK";
  return CKR_OK;
}

// PKCS #11 v2.20 section 11.7 page 132.
CK_RV C_GetObjectSize(CK_SESSION_HANDLE hSession,
                      CK_OBJECT_HANDLE hObject,
                      CK_ULONG_PTR pulSize) {
  LOG_CK_RV_AND_RETURN_IF(!g_is_initialized, CKR_CRYPTOKI_NOT_INITIALIZED);
  LOG_CK_RV_AND_RETURN_IF(!pulSize, CKR_ARGUMENTS_BAD);
  CK_RV result = g_proxy->GetObjectSize(hSession,
                                        hObject,
                                        chaps::PreservedCK_ULONG(pulSize));
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  VLOG(1) << __func__ << " - CKR_OK";
  return CKR_OK;
}

// PKCS #11 v2.20 section 11.7 page 133.
CK_RV C_GetAttributeValue(CK_SESSION_HANDLE hSession,
                          CK_OBJECT_HANDLE hObject,
                          CK_ATTRIBUTE_PTR pTemplate,
                          CK_ULONG ulCount) {
  LOG_CK_RV_AND_RETURN_IF(!g_is_initialized, CKR_CRYPTOKI_NOT_INITIALIZED);
  LOG_CK_RV_AND_RETURN_IF(!pTemplate, CKR_ARGUMENTS_BAD);
  chaps::Attributes attributes(pTemplate, ulCount);
  vector<uint8_t> serialized_attributes_in;
  if (!attributes.Serialize(&serialized_attributes_in))
    LOG_CK_RV_AND_RETURN(CKR_TEMPLATE_INCONSISTENT);
  vector<uint8_t> serialized_attributes_out;
  CK_RV result = g_proxy->GetAttributeValue(hSession,
                                            hObject,
                                            serialized_attributes_in,
                                            &serialized_attributes_out);
  // There are a few errors that can be returned while information about one or
  // more attributes has been provided.  We need to continue in these cases.
  if (result != CKR_OK &&
      result != CKR_ATTRIBUTE_TYPE_INVALID &&
      result != CKR_ATTRIBUTE_SENSITIVE &&
      result != CKR_BUFFER_TOO_SMALL)
    LOG_CK_RV_AND_RETURN(result);
  // Chapsd ensures the value is serialized correctly; we can assert.
  CHECK(attributes.ParseAndFill(serialized_attributes_out));
  VLOG(1) << __func__ << " - " << chaps::CK_RVToString(result);
  return result;
}

// PKCS #11 v2.20 section 11.7 page 135.
CK_RV C_SetAttributeValue(CK_SESSION_HANDLE hSession,
                          CK_OBJECT_HANDLE hObject,
                          CK_ATTRIBUTE_PTR pTemplate,
                          CK_ULONG ulCount) {
  LOG_CK_RV_AND_RETURN_IF(!g_is_initialized, CKR_CRYPTOKI_NOT_INITIALIZED);
  LOG_CK_RV_AND_RETURN_IF(!pTemplate, CKR_ARGUMENTS_BAD);
  chaps::Attributes attributes(pTemplate, ulCount);
  vector<uint8_t> serialized_attributes;
  if (!attributes.Serialize(&serialized_attributes))
    LOG_CK_RV_AND_RETURN(CKR_TEMPLATE_INCONSISTENT);
  CK_RV result = g_proxy->SetAttributeValue(hSession,
                                            hObject,
                                            serialized_attributes);
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  VLOG(1) << __func__ << " - CKR_OK";
  return CKR_OK;
}

// PKCS #11 v2.20 section 11.7 page 136.
CK_RV C_FindObjectsInit(CK_SESSION_HANDLE hSession,
                        CK_ATTRIBUTE_PTR pTemplate,
                        CK_ULONG ulCount) {
  LOG_CK_RV_AND_RETURN_IF(!g_is_initialized, CKR_CRYPTOKI_NOT_INITIALIZED);
  LOG_CK_RV_AND_RETURN_IF(!pTemplate && ulCount > 0, CKR_ARGUMENTS_BAD);
  chaps::Attributes attributes(pTemplate, ulCount);
  vector<uint8_t> serialized_attributes;
  if (!attributes.Serialize(&serialized_attributes))
    LOG_CK_RV_AND_RETURN(CKR_TEMPLATE_INCONSISTENT);
  CK_RV result = g_proxy->FindObjectsInit(hSession, serialized_attributes);
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  VLOG(1) << __func__ << " - CKR_OK";
  return CKR_OK;
}

// PKCS #11 v2.20 section 11.7 page 137.
CK_RV C_FindObjects(CK_SESSION_HANDLE hSession,
                    CK_OBJECT_HANDLE_PTR phObject,
                    CK_ULONG ulMaxObjectCount,
                    CK_ULONG_PTR pulObjectCount) {
  LOG_CK_RV_AND_RETURN_IF(!g_is_initialized, CKR_CRYPTOKI_NOT_INITIALIZED);
  LOG_CK_RV_AND_RETURN_IF(!phObject || !pulObjectCount, CKR_ARGUMENTS_BAD);
  vector<uint64_t> object_list;
  CK_RV result = g_proxy->FindObjects(hSession, ulMaxObjectCount, &object_list);
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  LOG_CK_RV_AND_RETURN_IF(object_list.size() > ulMaxObjectCount,
                          CKR_GENERAL_ERROR);
  *pulObjectCount = static_cast<CK_ULONG>(object_list.size());
  for (size_t i = 0; i < object_list.size(); i++) {
    phObject[i] = static_cast<CK_OBJECT_HANDLE>(object_list[i]);
  }
  VLOG(1) << __func__ << " - CKR_OK";
  return CKR_OK;
}

// PKCS #11 v2.20 section 11.7 page 138.
CK_RV C_FindObjectsFinal(CK_SESSION_HANDLE hSession) {
  LOG_CK_RV_AND_RETURN_IF(!g_is_initialized, CKR_CRYPTOKI_NOT_INITIALIZED);
  CK_RV result = g_proxy->FindObjectsFinal(hSession);
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  VLOG(1) << __func__ << " - CKR_OK";
  return CKR_OK;
}

// PKCS #11 v2.20 section 11.8 page 139.
CK_RV C_EncryptInit(CK_SESSION_HANDLE hSession,
                    CK_MECHANISM_PTR pMechanism,
                    CK_OBJECT_HANDLE hKey) {
  LOG_CK_RV_AND_RETURN_IF(!g_is_initialized, CKR_CRYPTOKI_NOT_INITIALIZED);
  LOG_CK_RV_AND_RETURN_IF(!pMechanism, CKR_ARGUMENTS_BAD);
  CK_RV result = g_proxy->EncryptInit(
      hSession,
      pMechanism->mechanism,
      chaps::ConvertByteBufferToVector(
          reinterpret_cast<CK_BYTE_PTR>(pMechanism->pParameter),
          pMechanism->ulParameterLen),
      hKey);
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  VLOG(1) << __func__ << " - CKR_OK";
  return CKR_OK;
}

// PKCS #11 v2.20 section 11.8 page 140.
CK_RV C_Encrypt(CK_SESSION_HANDLE hSession,
                CK_BYTE_PTR pData,
                CK_ULONG ulDataLen,
                CK_BYTE_PTR pEncryptedData,
                CK_ULONG_PTR pulEncryptedDataLen) {
  LOG_CK_RV_AND_RETURN_IF(!g_is_initialized, CKR_CRYPTOKI_NOT_INITIALIZED);
  if ((!pData && ulDataLen > 0) || !pulEncryptedDataLen)
    LOG_CK_RV_AND_RETURN(CKR_ARGUMENTS_BAD);
  vector<uint8_t> data_out;
  uint64_t data_out_length;
  uint64_t max_out_length =
      pEncryptedData ? static_cast<uint64_t>(*pulEncryptedDataLen) : 0;
  CK_RV result = g_proxy->Encrypt(hSession,
                                  chaps::ConvertByteBufferToVector(pData,
                                                                   ulDataLen),
                                  max_out_length,
                                  &data_out_length,
                                  &data_out);
  result = HandlePKCS11Output(result,
                              data_out,
                              data_out_length,
                              pEncryptedData,
                              pulEncryptedDataLen);
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  VLOG(1) << __func__ << " - CKR_OK";
  return CKR_OK;
}

// PKCS #11 v2.20 section 11.8 page 141.
CK_RV C_EncryptUpdate(CK_SESSION_HANDLE hSession,
                      CK_BYTE_PTR pPart,
                      CK_ULONG ulPartLen,
                      CK_BYTE_PTR pEncryptedPart,
                      CK_ULONG_PTR pulEncryptedPartLen) {
  LOG_CK_RV_AND_RETURN_IF(!g_is_initialized, CKR_CRYPTOKI_NOT_INITIALIZED);
  LOG_CK_RV_AND_RETURN_IF(!pPart || !pulEncryptedPartLen, CKR_ARGUMENTS_BAD);
  vector<uint8_t> data_out;
  uint64_t data_out_length;
  uint64_t max_out_length =
      pEncryptedPart ? static_cast<uint64_t>(*pulEncryptedPartLen) : 0;
  CK_RV result = g_proxy->EncryptUpdate(
      hSession,
      chaps::ConvertByteBufferToVector(pPart, ulPartLen),
      max_out_length,
      &data_out_length,
      &data_out);
  result = HandlePKCS11Output(result,
                              data_out,
                              data_out_length,
                              pEncryptedPart,
                              pulEncryptedPartLen);
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  VLOG(1) << __func__ << " - CKR_OK";
  return CKR_OK;
}

// PKCS #11 v2.20 section 11.8 page 141.
CK_RV C_EncryptFinal(CK_SESSION_HANDLE hSession,
                     CK_BYTE_PTR pLastEncryptedPart,
                     CK_ULONG_PTR pulLastEncryptedPartLen) {
  LOG_CK_RV_AND_RETURN_IF(!g_is_initialized, CKR_CRYPTOKI_NOT_INITIALIZED);
  LOG_CK_RV_AND_RETURN_IF(!pulLastEncryptedPartLen, CKR_ARGUMENTS_BAD);
  vector<uint8_t> data_out;
  uint64_t data_out_length;
  uint64_t max_out_length =
      pLastEncryptedPart ? static_cast<uint64_t>(*pulLastEncryptedPartLen) : 0;
  CK_RV result = g_proxy->EncryptFinal(hSession,
                                       max_out_length,
                                       &data_out_length,
                                       &data_out);
  result = HandlePKCS11Output(result,
                              data_out,
                              data_out_length,
                              pLastEncryptedPart,
                              pulLastEncryptedPartLen);
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  VLOG(1) << __func__ << " - CKR_OK";
  return CKR_OK;
}

// PKCS #11 v2.20 section 11.9 page 144.
CK_RV C_DecryptInit(CK_SESSION_HANDLE hSession,
                    CK_MECHANISM_PTR pMechanism,
                    CK_OBJECT_HANDLE hKey) {
  LOG_CK_RV_AND_RETURN_IF(!g_is_initialized, CKR_CRYPTOKI_NOT_INITIALIZED);
  LOG_CK_RV_AND_RETURN_IF(!pMechanism, CKR_ARGUMENTS_BAD);
  CK_RV result = g_proxy->DecryptInit(
      hSession,
      pMechanism->mechanism,
      chaps::ConvertByteBufferToVector(
          reinterpret_cast<CK_BYTE_PTR>(pMechanism->pParameter),
          pMechanism->ulParameterLen),
      hKey);
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  VLOG(1) << __func__ << " - CKR_OK";
  return CKR_OK;
}

// PKCS #11 v2.20 section 11.9 page 145.
CK_RV C_Decrypt(CK_SESSION_HANDLE hSession,
                CK_BYTE_PTR pEncryptedData,
                CK_ULONG ulEncryptedDataLen,
                CK_BYTE_PTR pData,
                CK_ULONG_PTR pulDataLen) {
  LOG_CK_RV_AND_RETURN_IF(!g_is_initialized, CKR_CRYPTOKI_NOT_INITIALIZED);
  if ((!pEncryptedData && ulEncryptedDataLen > 0) || !pulDataLen)
    LOG_CK_RV_AND_RETURN(CKR_ARGUMENTS_BAD);
  vector<uint8_t> data_out;
  uint64_t data_out_length;
  uint64_t max_out_length = pData ? static_cast<uint64_t>(*pulDataLen) : 0;
  CK_RV result = g_proxy->Decrypt(hSession,
                                  chaps::ConvertByteBufferToVector(
                                      pEncryptedData,
                                      ulEncryptedDataLen),
                                  max_out_length,
                                  &data_out_length,
                                  &data_out);
  result = HandlePKCS11Output(result,
                              data_out,
                              data_out_length,
                              pData,
                              pulDataLen);
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  VLOG(1) << __func__ << " - CKR_OK";
  return CKR_OK;
}

// PKCS #11 v2.20 section 11.9 page 146.
CK_RV C_DecryptUpdate(CK_SESSION_HANDLE hSession,
                      CK_BYTE_PTR pEncryptedPart,
                      CK_ULONG ulEncryptedPartLen,
                      CK_BYTE_PTR pPart,
                      CK_ULONG_PTR pulPartLen) {
  LOG_CK_RV_AND_RETURN_IF(!g_is_initialized, CKR_CRYPTOKI_NOT_INITIALIZED);
  LOG_CK_RV_AND_RETURN_IF(!pEncryptedPart || !pulPartLen, CKR_ARGUMENTS_BAD);
  vector<uint8_t> data_out;
  uint64_t data_out_length;
  uint64_t max_out_length = pPart ? static_cast<uint64_t>(*pulPartLen) : 0;
  CK_RV result = g_proxy->DecryptUpdate(hSession,
                                        chaps::ConvertByteBufferToVector(
                                            pEncryptedPart,
                                            ulEncryptedPartLen),
                                        max_out_length,
                                        &data_out_length,
                                        &data_out);
  result = HandlePKCS11Output(result,
                              data_out,
                              data_out_length,
                              pPart,
                              pulPartLen);
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  VLOG(1) << __func__ << " - CKR_OK";
  return CKR_OK;
}

// PKCS #11 v2.20 section 11.9 page 146.
CK_RV C_DecryptFinal(CK_SESSION_HANDLE hSession,
                     CK_BYTE_PTR pLastPart,
                     CK_ULONG_PTR pulLastPartLen) {
  LOG_CK_RV_AND_RETURN_IF(!g_is_initialized, CKR_CRYPTOKI_NOT_INITIALIZED);
  LOG_CK_RV_AND_RETURN_IF(!pulLastPartLen, CKR_ARGUMENTS_BAD);
  vector<uint8_t> data_out;
  uint64_t data_out_length;
  uint64_t max_out_length =
      pLastPart ? static_cast<uint64_t>(*pulLastPartLen) : 0;
  CK_RV result = g_proxy->DecryptFinal(hSession,
                                       max_out_length,
                                       &data_out_length,
                                       &data_out);
  result = HandlePKCS11Output(result,
                              data_out,
                              data_out_length,
                              pLastPart,
                              pulLastPartLen);
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  VLOG(1) << __func__ << " - CKR_OK";
  return CKR_OK;
}

// PKCS #11 v2.20 section 11.10 page 148.
CK_RV C_DigestInit(CK_SESSION_HANDLE hSession,
                   CK_MECHANISM_PTR pMechanism) {
  LOG_CK_RV_AND_RETURN_IF(!g_is_initialized, CKR_CRYPTOKI_NOT_INITIALIZED);
  LOG_CK_RV_AND_RETURN_IF(!pMechanism, CKR_ARGUMENTS_BAD);
  vector<uint8_t> parameter = chaps::ConvertByteBufferToVector(
      reinterpret_cast<CK_BYTE_PTR>(pMechanism->pParameter),
      pMechanism->ulParameterLen);
  CK_RV result = g_proxy->DigestInit(hSession,
                                     pMechanism->mechanism,
                                     parameter);
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  VLOG(1) << __func__ << " - CKR_OK";
  return CKR_OK;
}

// PKCS #11 v2.20 section 11.10 page 149.
CK_RV C_Digest(CK_SESSION_HANDLE hSession,
               CK_BYTE_PTR pData,
               CK_ULONG ulDataLen,
               CK_BYTE_PTR pDigest,
               CK_ULONG_PTR pulDigestLen) {
  LOG_CK_RV_AND_RETURN_IF(!g_is_initialized, CKR_CRYPTOKI_NOT_INITIALIZED);
  if ((!pData && ulDataLen > 0) || !pulDigestLen)
    LOG_CK_RV_AND_RETURN(CKR_ARGUMENTS_BAD);
  vector<uint8_t> data_out;
  uint64_t data_out_length;
  uint64_t max_out_length = pDigest ? static_cast<uint64_t>(*pulDigestLen) : 0;
  CK_RV result = g_proxy->Digest(hSession,
                                 chaps::ConvertByteBufferToVector(pData,
                                                                  ulDataLen),
                                 max_out_length,
                                 &data_out_length,
                                 &data_out);
  result = HandlePKCS11Output(result,
                              data_out,
                              data_out_length,
                              pDigest,
                              pulDigestLen);
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  VLOG(1) << __func__ << " - CKR_OK";
  return CKR_OK;
}

// PKCS #11 v2.20 section 11.10 page 150.
CK_RV C_DigestUpdate(CK_SESSION_HANDLE hSession,
                     CK_BYTE_PTR pPart,
                     CK_ULONG ulPartLen) {
  LOG_CK_RV_AND_RETURN_IF(!g_is_initialized, CKR_CRYPTOKI_NOT_INITIALIZED);
  LOG_CK_RV_AND_RETURN_IF(!pPart, CKR_ARGUMENTS_BAD);
  CK_RV result = g_proxy->DigestUpdate(
      hSession,
      chaps::ConvertByteBufferToVector(pPart, ulPartLen));
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  VLOG(1) << __func__ << " - CKR_OK";
  return CKR_OK;
}

// PKCS #11 v2.20 section 11.10 page 150.
CK_RV C_DigestKey(CK_SESSION_HANDLE hSession, CK_OBJECT_HANDLE hKey) {
  LOG_CK_RV_AND_RETURN_IF(!g_is_initialized, CKR_CRYPTOKI_NOT_INITIALIZED);
  CK_RV result = g_proxy->DigestKey(hSession, hKey);
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  VLOG(1) << __func__ << " - CKR_OK";
  return CKR_OK;
}

// PKCS #11 v2.20 section 11.10 page 151.
CK_RV C_DigestFinal(CK_SESSION_HANDLE hSession,
                    CK_BYTE_PTR pDigest,
                    CK_ULONG_PTR pulDigestLen) {
  LOG_CK_RV_AND_RETURN_IF(!g_is_initialized, CKR_CRYPTOKI_NOT_INITIALIZED);
  LOG_CK_RV_AND_RETURN_IF(!pulDigestLen, CKR_ARGUMENTS_BAD);
  vector<uint8_t> data_out;
  uint64_t data_out_length;
  uint64_t max_out_length = pDigest ? static_cast<uint64_t>(*pulDigestLen) : 0;
  CK_RV result = g_proxy->DigestFinal(hSession,
                                      max_out_length,
                                      &data_out_length,
                                      &data_out);
  result = HandlePKCS11Output(result,
                              data_out,
                              data_out_length,
                              pDigest,
                              pulDigestLen);
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  VLOG(1) << __func__ << " - CKR_OK";
  return CKR_OK;
}

// PKCS #11 v2.20 section 11.11 page 152.
CK_RV C_SignInit(CK_SESSION_HANDLE hSession,
                 CK_MECHANISM_PTR pMechanism,
                 CK_OBJECT_HANDLE hKey) {
  LOG_CK_RV_AND_RETURN_IF(!g_is_initialized, CKR_CRYPTOKI_NOT_INITIALIZED);
  LOG_CK_RV_AND_RETURN_IF(!pMechanism, CKR_ARGUMENTS_BAD);
  vector<uint8_t> parameter = chaps::ConvertByteBufferToVector(
      reinterpret_cast<CK_BYTE_PTR>(pMechanism->pParameter),
      pMechanism->ulParameterLen);
  CK_RV result = g_proxy->SignInit(hSession,
                                   pMechanism->mechanism,
                                   parameter,
                                   hKey);
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  VLOG(1) << __func__ << " - CKR_OK";
  return CKR_OK;
}

// PKCS #11 v2.20 section 11.11 page 153.
CK_RV C_Sign (CK_SESSION_HANDLE hSession,
              CK_BYTE_PTR pData,
              CK_ULONG ulDataLen,
              CK_BYTE_PTR pSignature,
              CK_ULONG_PTR pulSignatureLen) {
  LOG_CK_RV_AND_RETURN_IF(!g_is_initialized, CKR_CRYPTOKI_NOT_INITIALIZED);
  if ((!pData && ulDataLen > 0) || !pulSignatureLen)
    LOG_CK_RV_AND_RETURN(CKR_ARGUMENTS_BAD);
  vector<uint8_t> data_out;
  uint64_t data_out_length;
  uint64_t max_out_length =
      pSignature ? static_cast<uint64_t>(*pulSignatureLen) : 0;
  CK_RV result = g_proxy->Sign(hSession,
                               chaps::ConvertByteBufferToVector(pData,
                                                                ulDataLen),
                               max_out_length,
                               &data_out_length,
                               &data_out);
  result = HandlePKCS11Output(result,
                              data_out,
                              data_out_length,
                              pSignature,
                              pulSignatureLen);
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  VLOG(1) << __func__ << " - CKR_OK";
  return CKR_OK;
}

// PKCS #11 v2.20 section 11.11 page 154.
CK_RV C_SignUpdate(CK_SESSION_HANDLE hSession,
                   CK_BYTE_PTR pPart,
                   CK_ULONG ulPartLen) {
  LOG_CK_RV_AND_RETURN_IF(!g_is_initialized, CKR_CRYPTOKI_NOT_INITIALIZED);
  LOG_CK_RV_AND_RETURN_IF(!pPart, CKR_ARGUMENTS_BAD);
  CK_RV result = g_proxy->SignUpdate(hSession,
                                     chaps::ConvertByteBufferToVector(pPart,
                                                               ulPartLen));
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  VLOG(1) << __func__ << " - CKR_OK";
  return CKR_OK;
}

// PKCS #11 v2.20 section 11.11 page 154.
CK_RV C_SignFinal(CK_SESSION_HANDLE hSession,
                  CK_BYTE_PTR pSignature,
                  CK_ULONG_PTR pulSignatureLen) {
  LOG_CK_RV_AND_RETURN_IF(!g_is_initialized, CKR_CRYPTOKI_NOT_INITIALIZED);
  LOG_CK_RV_AND_RETURN_IF(!pulSignatureLen, CKR_ARGUMENTS_BAD);
  vector<uint8_t> data_out;
  uint64_t data_out_length;
  uint64_t max_out_length =
      pSignature ? static_cast<uint64_t>(*pulSignatureLen) : 0;
  CK_RV result = g_proxy->SignFinal(hSession,
                                    max_out_length,
                                    &data_out_length,
                                    &data_out);
  result = HandlePKCS11Output(result,
                              data_out,
                              data_out_length,
                              pSignature,
                              pulSignatureLen);
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  VLOG(1) << __func__ << " - CKR_OK";
  return CKR_OK;
}

// PKCS #11 v2.20 section 11.11 page 155.
CK_RV C_SignRecoverInit(CK_SESSION_HANDLE hSession,
                        CK_MECHANISM_PTR pMechanism,
                        CK_OBJECT_HANDLE hKey) {
  LOG_CK_RV_AND_RETURN_IF(!g_is_initialized, CKR_CRYPTOKI_NOT_INITIALIZED);
  LOG_CK_RV_AND_RETURN_IF(!pMechanism, CKR_ARGUMENTS_BAD);
  vector<uint8_t> parameter = chaps::ConvertByteBufferToVector(
      reinterpret_cast<CK_BYTE_PTR>(pMechanism->pParameter),
      pMechanism->ulParameterLen);
  CK_RV result = g_proxy->SignRecoverInit(hSession,
                                          pMechanism->mechanism,
                                          parameter,
                                          hKey);
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  VLOG(1) << __func__ << " - CKR_OK";
  return CKR_OK;
}

// PKCS #11 v2.20 section 11.11 page 156.
CK_RV C_SignRecover(CK_SESSION_HANDLE hSession,
                    CK_BYTE_PTR pData,
                    CK_ULONG ulDataLen,
                    CK_BYTE_PTR pSignature,
                    CK_ULONG_PTR pulSignatureLen) {
  LOG_CK_RV_AND_RETURN_IF(!g_is_initialized, CKR_CRYPTOKI_NOT_INITIALIZED);
  if ((!pData && ulDataLen > 0) || !pulSignatureLen)
    LOG_CK_RV_AND_RETURN(CKR_ARGUMENTS_BAD);
  vector<uint8_t> data_out;
  uint64_t data_out_length;
  uint64_t max_out_length =
      pSignature ? static_cast<uint64_t>(*pulSignatureLen) : 0;
  CK_RV result = g_proxy->SignRecover(hSession,
                                      chaps::ConvertByteBufferToVector(pData,
                                                                ulDataLen),
                                      max_out_length,
                                      &data_out_length,
                                      &data_out);
  result = HandlePKCS11Output(result,
                              data_out,
                              data_out_length,
                              pSignature,
                              pulSignatureLen);
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  VLOG(1) << __func__ << " - CKR_OK";
  return CKR_OK;
}

// PKCS #11 v2.20 section 11.12 page 157.
CK_RV C_VerifyInit(CK_SESSION_HANDLE hSession,
                   CK_MECHANISM_PTR pMechanism,
                   CK_OBJECT_HANDLE hKey) {
  LOG_CK_RV_AND_RETURN_IF(!g_is_initialized, CKR_CRYPTOKI_NOT_INITIALIZED);
  LOG_CK_RV_AND_RETURN_IF(!pMechanism, CKR_ARGUMENTS_BAD);
  vector<uint8_t> parameter = chaps::ConvertByteBufferToVector(
      reinterpret_cast<CK_BYTE_PTR>(pMechanism->pParameter),
      pMechanism->ulParameterLen);
  CK_RV result = g_proxy->VerifyInit(hSession,
                                     pMechanism->mechanism,
                                     parameter,
                                     hKey);
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  VLOG(1) << __func__ << " - CKR_OK";
  return CKR_OK;
}

// PKCS #11 v2.20 section 11.12 page 158.
CK_RV C_Verify(CK_SESSION_HANDLE hSession,
               CK_BYTE_PTR pData,
               CK_ULONG ulDataLen,
               CK_BYTE_PTR pSignature,
               CK_ULONG ulSignatureLen) {
  LOG_CK_RV_AND_RETURN_IF(!g_is_initialized, CKR_CRYPTOKI_NOT_INITIALIZED);
  if (!pSignature || (!pData && ulDataLen > 0))
    LOG_CK_RV_AND_RETURN(CKR_ARGUMENTS_BAD);
  CK_RV result = g_proxy->Verify(
      hSession,
      chaps::ConvertByteBufferToVector(pData, ulDataLen),
      chaps::ConvertByteBufferToVector(pSignature, ulSignatureLen));
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  VLOG(1) << __func__ << " - CKR_OK";
  return CKR_OK;
}

// PKCS #11 v2.20 section 11.12 page 159.
CK_RV C_VerifyUpdate(CK_SESSION_HANDLE hSession,
                     CK_BYTE_PTR pPart,
                     CK_ULONG ulPartLen) {
  LOG_CK_RV_AND_RETURN_IF(!g_is_initialized, CKR_CRYPTOKI_NOT_INITIALIZED);
  LOG_CK_RV_AND_RETURN_IF(!pPart, CKR_ARGUMENTS_BAD);
  CK_RV result = g_proxy->VerifyUpdate(
      hSession,
      chaps::ConvertByteBufferToVector(pPart, ulPartLen));
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  VLOG(1) << __func__ << " - CKR_OK";
  return CKR_OK;
}

// PKCS #11 v2.20 section 11.12 page 159.
CK_RV C_VerifyFinal(CK_SESSION_HANDLE hSession,
                    CK_BYTE_PTR pSignature,
                    CK_ULONG ulSignatureLen) {
  LOG_CK_RV_AND_RETURN_IF(!g_is_initialized, CKR_CRYPTOKI_NOT_INITIALIZED);
  LOG_CK_RV_AND_RETURN_IF(!pSignature, CKR_ARGUMENTS_BAD);
  CK_RV result = g_proxy->VerifyFinal(
      hSession,
      chaps::ConvertByteBufferToVector(pSignature, ulSignatureLen));
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  VLOG(1) << __func__ << " - CKR_OK";
  return CKR_OK;
}

// PKCS #11 v2.20 section 11.12 page 161.
CK_RV C_VerifyRecoverInit(CK_SESSION_HANDLE hSession,
                          CK_MECHANISM_PTR pMechanism,
                          CK_OBJECT_HANDLE hKey) {
  LOG_CK_RV_AND_RETURN_IF(!g_is_initialized, CKR_CRYPTOKI_NOT_INITIALIZED);
  LOG_CK_RV_AND_RETURN_IF(!pMechanism, CKR_ARGUMENTS_BAD);
  vector<uint8_t> parameter = chaps::ConvertByteBufferToVector(
      reinterpret_cast<CK_BYTE_PTR>(pMechanism->pParameter),
      pMechanism->ulParameterLen);
  CK_RV result = g_proxy->VerifyRecoverInit(hSession,
                                     pMechanism->mechanism,
                                     parameter,
                                     hKey);
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  VLOG(1) << __func__ << " - CKR_OK";
  return CKR_OK;
}

// PKCS #11 v2.20 section 11.12 page 161.
CK_RV C_VerifyRecover(CK_SESSION_HANDLE hSession,
                      CK_BYTE_PTR pSignature,
                      CK_ULONG ulSignatureLen,
                      CK_BYTE_PTR pData,
                      CK_ULONG_PTR pulDataLen) {
  LOG_CK_RV_AND_RETURN_IF(!g_is_initialized, CKR_CRYPTOKI_NOT_INITIALIZED);
  if (!pSignature || !pulDataLen)
    LOG_CK_RV_AND_RETURN(CKR_ARGUMENTS_BAD);
  vector<uint8_t> data_out;
  uint64_t data_out_length;
  uint64_t max_out_length = pData ? static_cast<uint64_t>(*pulDataLen) : 0;
  CK_RV result = g_proxy->VerifyRecover(
      hSession,
      chaps::ConvertByteBufferToVector(pSignature, ulSignatureLen),
      max_out_length,
      &data_out_length,
      &data_out);
  result = HandlePKCS11Output(result,
                              data_out,
                              data_out_length,
                              pData,
                              pulDataLen);
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  VLOG(1) << __func__ << " - CKR_OK";
  return CKR_OK;
}

// PKCS #11 v2.20 section 11.13 page 163.
CK_RV C_DigestEncryptUpdate(CK_SESSION_HANDLE hSession,
                            CK_BYTE_PTR pPart,
                            CK_ULONG ulPartLen,
                            CK_BYTE_PTR pEncryptedPart,
                            CK_ULONG_PTR pulEncryptedPartLen) {
  LOG_CK_RV_AND_RETURN_IF(!g_is_initialized, CKR_CRYPTOKI_NOT_INITIALIZED);
  LOG_CK_RV_AND_RETURN_IF(!pPart || !pulEncryptedPartLen, CKR_ARGUMENTS_BAD);
  vector<uint8_t> data_out;
  uint64_t data_out_length;
  uint64_t max_out_length =
      pEncryptedPart ? static_cast<uint64_t>(*pulEncryptedPartLen) : 0;
  CK_RV result = g_proxy->DigestEncryptUpdate(
      hSession,
      chaps::ConvertByteBufferToVector(pPart, ulPartLen),
      max_out_length,
      &data_out_length,
      &data_out);
  result = HandlePKCS11Output(result,
                              data_out,
                              data_out_length,
                              pEncryptedPart,
                              pulEncryptedPartLen);
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  VLOG(1) << __func__ << " - CKR_OK";
  return CKR_OK;
}

// PKCS #11 v2.20 section 11.13 page 165.
CK_RV C_DecryptDigestUpdate(CK_SESSION_HANDLE hSession,
                            CK_BYTE_PTR pEncryptedPart,
                            CK_ULONG ulEncryptedPartLen,
                            CK_BYTE_PTR pPart,
                            CK_ULONG_PTR pulPartLen) {
  LOG_CK_RV_AND_RETURN_IF(!g_is_initialized, CKR_CRYPTOKI_NOT_INITIALIZED);
  LOG_CK_RV_AND_RETURN_IF(!pEncryptedPart || !pulPartLen, CKR_ARGUMENTS_BAD);
  vector<uint8_t> data_out;
  uint64_t data_out_length;
  uint64_t max_out_length = pPart ? static_cast<uint64_t>(*pulPartLen) : 0;
  CK_RV result = g_proxy->DecryptDigestUpdate(
      hSession,
      chaps::ConvertByteBufferToVector(pEncryptedPart, ulEncryptedPartLen),
      max_out_length,
      &data_out_length,
      &data_out);
  result = HandlePKCS11Output(result,
                              data_out,
                              data_out_length,
                              pPart,
                              pulPartLen);
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  VLOG(1) << __func__ << " - CKR_OK";
  return CKR_OK;
}

// PKCS #11 v2.20 section 11.13 page 169.
CK_RV C_SignEncryptUpdate(CK_SESSION_HANDLE hSession,
                          CK_BYTE_PTR pPart,
                          CK_ULONG ulPartLen,
                          CK_BYTE_PTR pEncryptedPart,
                          CK_ULONG_PTR pulEncryptedPartLen) {
  LOG_CK_RV_AND_RETURN_IF(!g_is_initialized, CKR_CRYPTOKI_NOT_INITIALIZED);
  LOG_CK_RV_AND_RETURN_IF(!pPart || !pulEncryptedPartLen, CKR_ARGUMENTS_BAD);
  vector<uint8_t> data_out;
  uint64_t data_out_length;
  uint64_t max_out_length =
      pEncryptedPart ? static_cast<uint64_t>(*pulEncryptedPartLen) : 0;
  CK_RV result = g_proxy->SignEncryptUpdate(
      hSession,
      chaps::ConvertByteBufferToVector(pPart, ulPartLen),
      max_out_length,
      &data_out_length,
      &data_out);
  result = HandlePKCS11Output(result,
                              data_out,
                              data_out_length,
                              pEncryptedPart,
                              pulEncryptedPartLen);
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  VLOG(1) << __func__ << " - CKR_OK";
  return CKR_OK;
}

// PKCS #11 v2.20 section 11.13 page 171.
CK_RV C_DecryptVerifyUpdate(CK_SESSION_HANDLE hSession,
                            CK_BYTE_PTR pEncryptedPart,
                            CK_ULONG ulEncryptedPartLen,
                            CK_BYTE_PTR pPart,
                            CK_ULONG_PTR pulPartLen) {
  LOG_CK_RV_AND_RETURN_IF(!g_is_initialized, CKR_CRYPTOKI_NOT_INITIALIZED);
  LOG_CK_RV_AND_RETURN_IF(!pEncryptedPart || !pulPartLen, CKR_ARGUMENTS_BAD);
  vector<uint8_t> data_out;
  uint64_t data_out_length;
  uint64_t max_out_length = pPart ? static_cast<uint64_t>(*pulPartLen) : 0;
  CK_RV result = g_proxy->DecryptVerifyUpdate(
      hSession,
      chaps::ConvertByteBufferToVector(pEncryptedPart, ulEncryptedPartLen),
      max_out_length,
      &data_out_length,
      &data_out);
  result = HandlePKCS11Output(result,
                              data_out,
                              data_out_length,
                              pPart,
                              pulPartLen);
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  VLOG(1) << __func__ << " - CKR_OK";
  return CKR_OK;
}

// PKCS #11 v2.20 section 11.14 page 175.
CK_RV C_GenerateKey(CK_SESSION_HANDLE hSession,
                    CK_MECHANISM_PTR pMechanism,
                    CK_ATTRIBUTE_PTR pTemplate,
                    CK_ULONG ulCount,
                    CK_OBJECT_HANDLE_PTR phKey) {
  LOG_CK_RV_AND_RETURN_IF(!g_is_initialized, CKR_CRYPTOKI_NOT_INITIALIZED);
  if (!pMechanism || (!pTemplate && ulCount > 0) || !phKey)
    LOG_CK_RV_AND_RETURN(CKR_ARGUMENTS_BAD);
  chaps::Attributes attributes(pTemplate, ulCount);
  vector<uint8_t> serialized;
  if (!attributes.Serialize(&serialized))
    LOG_CK_RV_AND_RETURN(CKR_TEMPLATE_INCONSISTENT);
  CK_RV result = g_proxy->GenerateKey(
      hSession,
      pMechanism->mechanism,
      chaps::ConvertByteBufferToVector(
          reinterpret_cast<CK_BYTE_PTR>(pMechanism->pParameter),
          pMechanism->ulParameterLen),
      serialized,
      chaps::PreservedCK_ULONG(phKey));
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  VLOG(1) << __func__ << " - CKR_OK";
  return CKR_OK;
}

// PKCS #11 v2.20 section 11.14 page 176.
CK_RV C_GenerateKeyPair(CK_SESSION_HANDLE hSession,
                        CK_MECHANISM_PTR pMechanism,
                        CK_ATTRIBUTE_PTR pPublicKeyTemplate,
                        CK_ULONG ulPublicKeyAttributeCount,
                        CK_ATTRIBUTE_PTR pPrivateKeyTemplate,
                        CK_ULONG ulPrivateKeyAttributeCount,
                        CK_OBJECT_HANDLE_PTR phPublicKey,
                        CK_OBJECT_HANDLE_PTR phPrivateKey) {
  LOG_CK_RV_AND_RETURN_IF(!g_is_initialized, CKR_CRYPTOKI_NOT_INITIALIZED);
  if (!pMechanism ||
      (!pPublicKeyTemplate && ulPublicKeyAttributeCount > 0) ||
      (!pPrivateKeyTemplate && ulPrivateKeyAttributeCount > 0) ||
      !phPublicKey ||
      !phPrivateKey)
    LOG_CK_RV_AND_RETURN(CKR_ARGUMENTS_BAD);
  chaps::Attributes public_attributes(pPublicKeyTemplate,
                                      ulPublicKeyAttributeCount);
  chaps::Attributes private_attributes(pPrivateKeyTemplate,
                                       ulPrivateKeyAttributeCount);
  vector<uint8_t> public_serialized, private_serialized;
  if (!public_attributes.Serialize(&public_serialized) ||
      !private_attributes.Serialize(&private_serialized))
    LOG_CK_RV_AND_RETURN(CKR_TEMPLATE_INCONSISTENT);
  CK_RV result = g_proxy->GenerateKeyPair(
      hSession,
      pMechanism->mechanism,
      chaps::ConvertByteBufferToVector(
          reinterpret_cast<CK_BYTE_PTR>(pMechanism->pParameter),
          pMechanism->ulParameterLen),
      public_serialized,
      private_serialized,
      chaps::PreservedCK_ULONG(phPublicKey),
      chaps::PreservedCK_ULONG(phPrivateKey));
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  VLOG(1) << __func__ << " - CKR_OK";
  return CKR_OK;
}

// PKCS #11 v2.20 section 11.14 page 178.
CK_RV C_WrapKey(CK_SESSION_HANDLE hSession,
                CK_MECHANISM_PTR pMechanism,
                CK_OBJECT_HANDLE hWrappingKey,
                CK_OBJECT_HANDLE hKey,
                CK_BYTE_PTR pWrappedKey,
                CK_ULONG_PTR pulWrappedKeyLen) {
  LOG_CK_RV_AND_RETURN_IF(!g_is_initialized, CKR_CRYPTOKI_NOT_INITIALIZED);
  if (!pMechanism || !pulWrappedKeyLen)
    LOG_CK_RV_AND_RETURN(CKR_ARGUMENTS_BAD);
  vector<uint8_t> data_out;
  uint64_t data_out_length;
  uint64_t max_out_length =
      pWrappedKey ? static_cast<uint64_t>(*pulWrappedKeyLen) : 0;
  uint32_t result = g_proxy->WrapKey(
      hSession,
      pMechanism->mechanism,
      chaps::ConvertByteBufferToVector(
          reinterpret_cast<CK_BYTE_PTR>(pMechanism->pParameter),
          pMechanism->ulParameterLen),
      hWrappingKey,
      hKey,
      max_out_length,
      &data_out_length,
      &data_out);
  result = HandlePKCS11Output(result,
                              data_out,
                              data_out_length,
                              pWrappedKey,
                              pulWrappedKeyLen);
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  VLOG(1) << __func__ << " - CKR_OK";
  return CKR_OK;
}

// PKCS #11 v2.20 section 11.14 page 180.
CK_RV C_UnwrapKey(CK_SESSION_HANDLE hSession,
                  CK_MECHANISM_PTR pMechanism,
                  CK_OBJECT_HANDLE hUnwrappingKey,
                  CK_BYTE_PTR pWrappedKey,
                  CK_ULONG ulWrappedKeyLen,
                  CK_ATTRIBUTE_PTR pTemplate,
                  CK_ULONG ulAttributeCount,
                  CK_OBJECT_HANDLE_PTR phKey) {
  LOG_CK_RV_AND_RETURN_IF(!g_is_initialized, CKR_CRYPTOKI_NOT_INITIALIZED);
  if (!pMechanism || !pWrappedKey || !phKey)
    LOG_CK_RV_AND_RETURN(CKR_ARGUMENTS_BAD);
  chaps::Attributes attributes(pTemplate, ulAttributeCount);
  vector<uint8_t> serialized;
  if (!attributes.Serialize(&serialized))
    LOG_CK_RV_AND_RETURN(CKR_TEMPLATE_INCONSISTENT);
  uint32_t result = g_proxy->UnwrapKey(
      hSession,
      pMechanism->mechanism,
      chaps::ConvertByteBufferToVector(
          reinterpret_cast<CK_BYTE_PTR>(pMechanism->pParameter),
          pMechanism->ulParameterLen),
      hUnwrappingKey,
      chaps::ConvertByteBufferToVector(pWrappedKey, ulWrappedKeyLen),
      serialized,
      chaps::PreservedCK_ULONG(phKey));
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  VLOG(1) << __func__ << " - CKR_OK";
  return CKR_OK;
}

// PKCS #11 v2.20 section 11.14 page 182.
CK_RV C_DeriveKey(CK_SESSION_HANDLE hSession,
                  CK_MECHANISM_PTR pMechanism,
                  CK_OBJECT_HANDLE hBaseKey,
                  CK_ATTRIBUTE_PTR pTemplate,
                  CK_ULONG ulAttributeCount,
                  CK_OBJECT_HANDLE_PTR phKey) {
  LOG_CK_RV_AND_RETURN_IF(!g_is_initialized, CKR_CRYPTOKI_NOT_INITIALIZED);
  if (!pMechanism || !phKey)
    LOG_CK_RV_AND_RETURN(CKR_ARGUMENTS_BAD);
  chaps::Attributes attributes(pTemplate, ulAttributeCount);
  vector<uint8_t> serialized;
  if (!attributes.Serialize(&serialized))
    LOG_CK_RV_AND_RETURN(CKR_TEMPLATE_INCONSISTENT);
  uint32_t result = g_proxy->DeriveKey(
      hSession,
      pMechanism->mechanism,
      chaps::ConvertByteBufferToVector(
          reinterpret_cast<CK_BYTE_PTR>(pMechanism->pParameter),
          pMechanism->ulParameterLen),
      hBaseKey,
      serialized,
      chaps::PreservedCK_ULONG(phKey));
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  VLOG(1) << __func__ << " - CKR_OK";
  return CKR_OK;
}

// PKCS #11 v2.20 section 11.15 page 184.
CK_RV C_SeedRandom(CK_SESSION_HANDLE hSession,
                   CK_BYTE_PTR pSeed,
                   CK_ULONG ulSeedLen) {
  LOG_CK_RV_AND_RETURN_IF(!g_is_initialized, CKR_CRYPTOKI_NOT_INITIALIZED);
  if (!pSeed || ulSeedLen == 0)
    LOG_CK_RV_AND_RETURN(CKR_ARGUMENTS_BAD);
  uint32_t result = g_proxy->SeedRandom(
      hSession,
      chaps::ConvertByteBufferToVector(pSeed, ulSeedLen));
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  VLOG(1) << __func__ << " - CKR_OK";
  return CKR_OK;
}

// PKCS #11 v2.20 section 11.15 page 184.
CK_RV C_GenerateRandom(CK_SESSION_HANDLE hSession,
                       CK_BYTE_PTR RandomData,
                       CK_ULONG ulRandomLen) {
  LOG_CK_RV_AND_RETURN_IF(!g_is_initialized, CKR_CRYPTOKI_NOT_INITIALIZED);
  if (!RandomData || ulRandomLen == 0)
    LOG_CK_RV_AND_RETURN(CKR_ARGUMENTS_BAD);
  vector<uint8_t> data_out;
  uint32_t result = g_proxy->GenerateRandom(hSession, ulRandomLen, &data_out);
  LOG_CK_RV_AND_RETURN_IF_ERR(result);
  LOG_CK_RV_AND_RETURN_IF(data_out.size() != ulRandomLen, CKR_GENERAL_ERROR);
  memcpy(RandomData, &data_out.front(), ulRandomLen);
  VLOG(1) << __func__ << " - CKR_OK";
  return CKR_OK;
}

// PKCS #11 v2.20 section 11.16 page 185.
CK_RV C_GetFunctionStatus(CK_SESSION_HANDLE hSession) {
  return CKR_FUNCTION_NOT_PARALLEL;
}

// PKCS #11 v2.20 section 11.16 page 186.
CK_RV C_CancelFunction(CK_SESSION_HANDLE hSession) {
  return CKR_FUNCTION_NOT_PARALLEL;
}
