// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Contains the implementation of class Pkcs11Init.

#include "pkcs11_init.h"

#include <string.h>

#include <base/logging.h>
#include <base/string_util.h>
#include <base/stringprintf.h>
#include <chaps/isolate.h>
#include <chaps/token_manager_client.h>
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

Pkcs11Init::Pkcs11Init() : default_platform_(new Platform),
                           platform_(default_platform_.get()) {
}

Pkcs11Init::~Pkcs11Init() {
}

void Pkcs11Init::GetTpmTokenInfo(gchar **OUT_label,
                                 gchar **OUT_user_pin) {
  *OUT_label = g_strdup(reinterpret_cast<const gchar *>(kDefaultLabel));
  *OUT_user_pin = g_strdup(reinterpret_cast<const gchar *>(kDefaultUserPin));
}

void Pkcs11Init::GetTpmTokenInfoForUser(gchar *username,
                                        gchar **OUT_label,
                                        gchar **OUT_user_pin) {
  // All tokens get the same label.  There will be only one per profile so there
  // is no need for users to differentiate.
  GetTpmTokenInfo(OUT_label, OUT_user_pin);
}

std::string Pkcs11Init::GetTpmTokenLabelForUser(const std::string& username) {
  return std::string(reinterpret_cast<const char*>(kDefaultLabel));
}

bool Pkcs11Init::GetTpmTokenSlotForPath(const base::FilePath& path,
                                        CK_SLOT_ID_PTR slot) {
  CK_RV rv;
  rv = C_Initialize(NULL);
  if (rv != CKR_OK && rv != CKR_CRYPTOKI_ALREADY_INITIALIZED) {
    LOG(WARNING) << __func__ << ": C_Initialize failed.";
    return false;
  }
  CK_ULONG num_slots = 0;
  rv = C_GetSlotList(CK_TRUE, NULL, &num_slots);
  if (rv != CKR_OK) {
    LOG(WARNING) << __func__ << ": C_GetSlotList(NULL) failed.";
    return false;
  }
  scoped_array<CK_SLOT_ID> slot_list(new CK_SLOT_ID[num_slots]);
  rv = C_GetSlotList(CK_TRUE, slot_list.get(), &num_slots);
  if (rv != CKR_OK) {
    LOG(WARNING) << __func__ << ": C_GetSlotList failed.";
    return false;
  }
  chaps::TokenManagerClient token_manager;
  for (CK_ULONG i = 0; i < num_slots; ++i) {
    FilePath slot_path;
    if (token_manager.GetTokenPath(
        chaps::IsolateCredentialManager::GetDefaultIsolateCredential(),
        slot_list[i],
        &slot_path) && (path == slot_path)) {
      *slot = slot_list[i];
      return true;
    }
  }
  LOG(WARNING) << __func__ << ": Path not found.";
  return false;
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
