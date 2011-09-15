// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is the Chaps client. Essentially it forwards all PKCS #11 calls to the
// Chaps Daemon (chapsd) via D-Bus.

#include <string>
#include <vector>

#include <base/basictypes.h>
#include <base/logging.h>

#include "chaps/chaps_proxy.h"
#include "chaps/chaps_utility.h"
#include "pkcs11/cryptoki.h"

using std::string;
using std::vector;

static const CK_BYTE kChapsLibraryVersionMajor = 0;
static const CK_BYTE kChapsLibraryVersionMinor = 1;

// The global proxy instance. This is valid only when g_is_initialized is true.
static chaps::ChapsProxyInterface* g_proxy = NULL;

// Set to true when using a mock proxy.
static bool g_is_using_mock = false;

// Set to true when C_Initialize has been called successfully.
// When not using a mock proxy this is synonymous with (g_proxy != NULL).
static bool g_is_initialized = false;

// tear down helper
static void TearDown() {
  if (g_is_initialized && !g_is_using_mock) {
    delete g_proxy;
    g_proxy = NULL;
  }
  g_is_initialized = false;
}

// Logging helper macro
#define LOG_CK_RV(rv) LOG(ERROR) << __func__ << " - " << chaps::RVToString(rv);

namespace chaps {

// Helpers to support a mock proxy (useful in testing)
void EnableMockProxy(ChapsProxyInterface* proxy, bool is_initialized) {
  g_proxy = proxy;
  g_is_using_mock = true;
  g_is_initialized = is_initialized;
}

void DisableMockProxy() {
  g_proxy = NULL;
  g_is_using_mock = false;
  g_is_initialized = false;
}

}  // namespace chaps

// The following functions are PKCS #11 entry points. They are intentionally
// in the root namespace and are declared 'extern "C"' in pkcs11.h.

// PKCS #11 v2.20 section 11.4 page 102.
// Connects to the D-Bus service.
CK_RV C_Initialize(CK_VOID_PTR pInitArgs) {
  if (g_is_initialized)
    return CKR_CRYPTOKI_ALREADY_INITIALIZED;
  // If we're not using a mock proxy instance we need to create one.
  if (!g_is_using_mock)
    g_proxy = new chaps::ChapsProxyImpl();
  if (!g_proxy)
    return CKR_HOST_MEMORY;
  g_is_initialized = true;
  // We have a proxy instance, now connect.
  if (!g_proxy->Connect("default")) {
    TearDown();
    return CKR_GENERAL_ERROR;
  }
  return CKR_OK;
}

// PKCS #11 v2.20 section 11.4 page 104.
// Closes the D-Bus service connection.
CK_RV C_Finalize(CK_VOID_PTR pReserved) {
  if (pReserved != NULL_PTR)
    return CKR_ARGUMENTS_BAD;
  if (!g_is_initialized)
    return CKR_CRYPTOKI_NOT_INITIALIZED;
  g_proxy->Disconnect();
  TearDown();
  return CKR_OK;
}

// PKCS #11 v2.20 section 11.4 page 105.
// Provide library info locally.
// TODO(dkrahn): i18n of strings - crosbug.com/20637
CK_RV C_GetInfo(CK_INFO_PTR pInfo) {
  if (!g_is_initialized)
    return CKR_CRYPTOKI_NOT_INITIALIZED;
  if (pInfo == NULL_PTR)
    return CKR_ARGUMENTS_BAD;
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
  if (!g_is_initialized)
    return CKR_CRYPTOKI_NOT_INITIALIZED;
  if (pulCount == NULL_PTR)
    return CKR_ARGUMENTS_BAD;

  CK_RV rv = CKR_OK;
  vector<uint32_t> slot_list;
  rv = g_proxy->GetSlotList((tokenPresent != CK_FALSE), slot_list);
  if (rv != CKR_OK) {
    LOG_CK_RV(rv);
    return rv;
  }
  size_t max_copy = chaps::UlongToSize(*pulCount);
  *pulCount = chaps::SizeToUlong(slot_list.size());
  if (pSlotList == NULL_PTR)
    return CKR_OK;
  for (size_t i = 0; i < slot_list.size(); i++) {
    if (i >= max_copy)
      return CKR_BUFFER_TOO_SMALL;
    pSlotList[i] = slot_list[i];
  }
  return CKR_OK;
}

// PKCS #11 v2.20 section 11.5 page 108.
CK_RV C_GetSlotInfo(CK_SLOT_ID slotID, CK_SLOT_INFO_PTR pInfo) {
  if (!g_is_initialized)
    return CKR_CRYPTOKI_NOT_INITIALIZED;
  if (pInfo == NULL_PTR)
    return CKR_ARGUMENTS_BAD;

  CK_RV rv = CKR_OK;
  string slot_description;
  string manufacturer_id;
  uint32_t flags = 0;
  rv = g_proxy->GetSlotInfo(slotID,
      slot_description,
      manufacturer_id,
      flags,
      chaps::BytePtrToRef8(&pInfo->hardwareVersion.major),
      chaps::BytePtrToRef8(&pInfo->hardwareVersion.minor),
      chaps::BytePtrToRef8(&pInfo->firmwareVersion.major),
      chaps::BytePtrToRef8(&pInfo->firmwareVersion.minor));
  if (rv != CKR_OK) {
    LOG_CK_RV(rv);
    return rv;
  }
  pInfo->flags = flags;
  chaps::CopyToCharBuffer(slot_description.c_str(), pInfo->slotDescription,
                          arraysize(pInfo->slotDescription));
  chaps::CopyToCharBuffer(manufacturer_id.c_str(), pInfo->manufacturerID,
                          arraysize(pInfo->manufacturerID));
  return CKR_OK;
}

