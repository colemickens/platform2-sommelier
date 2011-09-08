// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is the Chaps client. Essentially it forwards all PKCS #11 calls to the
// Chaps Daemon (chapsd) via D-Bus.

#include "chaps/chaps_proxy.h"
#include "pkcs11/cryptoki.h"

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

// PKCS #11 v2.20 section 11.4.
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

// PKCS #11 v2.20 section 11.4.
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

