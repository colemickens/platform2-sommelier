// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Contains the implementation of class Pkcs11Init.

#include "pkcs11_init.h"

#include <string.h>

#include <base/logging.h>
#include <base/string_util.h>
#include <base/stringprintf.h>
#include <chromeos/cryptohome.h>
#include <errno.h>
#include <glib.h>


namespace cryptohome {

// static
const CK_SLOT_ID Pkcs11Init::kDefaultTpmSlotId = 0;
const CK_ULONG Pkcs11Init::kMaxLabelLen = 32;
const CK_CHAR Pkcs11Init::kDefaultUserPin[] = "111111";
const CK_CHAR Pkcs11Init::kDefaultLabel[] = "User-Specific TPM Token";

extern const char* kTpmOwnedFile;

void Pkcs11Init::GetTpmTokenInfo(gchar **OUT_label,
                                 gchar **OUT_user_pin) {
  *OUT_label = g_strdup(reinterpret_cast<const gchar *>(kDefaultLabel));
  *OUT_user_pin = g_strdup(reinterpret_cast<const gchar *>(kDefaultUserPin));
}

void Pkcs11Init::GetTpmTokenInfoForUser(gchar *username,
                                        gchar **OUT_label,
                                        gchar **OUT_user_pin) {
  // TODO(ellyjones): make this work for real, perhaps? crosbug.com/22127
  *OUT_label = g_strdup(reinterpret_cast<const gchar *>(kDefaultLabel));
  *OUT_user_pin = g_strdup(reinterpret_cast<const gchar *>(kDefaultUserPin));
}

bool Pkcs11Init::IsUserTokenBroken() {
  if (!platform_->FileExists(kTpmOwnedFile)) {
    LOG(WARNING) << "TPM is not owned, token can not be valid.";
    return true;
  }

  if (!CheckTokenInSlot(kDefaultTpmSlotId, kDefaultLabel)) {
    LOG(WARNING) << "Token failed basic sanity checks. Can not be valid.";
    return true;
  }

  LOG(INFO) << "PKCS#11 token looks ok.";
  return false;
}

bool Pkcs11Init::CheckTokenInSlot(CK_SLOT_ID slot_id,
                                  const CK_CHAR* expected_label) {
  CK_RV rv;
  CK_SESSION_HANDLE session_handle = 0;
  CK_SESSION_INFO session_info;
  CK_TOKEN_INFO token_info;

  rv = C_Initialize(NULL);
  if (rv != CKR_OK && rv != CKR_CRYPTOKI_ALREADY_INITIALIZED) {
    LOG(WARNING) << "C_Initialize failed while checking token: "
                 << std::hex << rv;
    return false;
  }

  rv = C_OpenSession(slot_id, CKF_RW_SESSION | CKF_SERIAL_SESSION,
                     NULL, NULL,
                     &session_handle);
  if (rv != CKR_OK) {
    LOG(WARNING) << "Could not open session on slot " << slot_id
                 << " while checking token." << std::hex << rv;
    C_CloseAllSessions(slot_id);
    return false;
  }

  memset(&session_info, 0, sizeof(session_info));
  rv = C_GetSessionInfo(session_handle, &session_info);
  if (rv != CKR_OK || session_info.slotID != slot_id) {
    LOG(WARNING) << "Could not get session info on " << slot_id
                 << " while checking token: " << std::hex << rv;
    C_CloseAllSessions(slot_id);
    return false;
  }

  rv = C_GetTokenInfo(slot_id, &token_info);
  if (rv != CKR_OK || !(token_info.flags & CKF_TOKEN_INITIALIZED)) {
    LOG(WARNING) << "Could not get token info on " << slot_id
                 << " while checking token: " << std::hex << rv;
    C_CloseAllSessions(slot_id);
    return false;
  }

  if (strncmp(reinterpret_cast<const char*>(expected_label),
              reinterpret_cast<const char*>(token_info.label),
              kMaxLabelLen)) {
    LOG(WARNING) << "Token Label (" << token_info.label << ") does not match "
                 << "expected label (" << expected_label << ")";
    return false;
  }

  C_CloseAllSessions(slot_id);
  return true;
}

}  // namespace cryptohome
