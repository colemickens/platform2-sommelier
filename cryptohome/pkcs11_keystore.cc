// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pkcs11_keystore.h"

#include <string>

#include <base/basictypes.h>
#include <base/logging.h>
#include <base/memory/scoped_ptr.h>
#include <base/stl_util.h>
#include <chaps/pkcs11/cryptoki.h>
#include <chromeos/secure_blob.h>

using chromeos::SecureBlob;
using std::string;

namespace cryptohome {

// An arbitrary application ID to identify PKCS #11 objects.
const char kApplicationID[] = "CrOS_d5bbc079d2497110feadfc97c40d718ae46f4658";

// TODO(dkrahn): crosbug.com/23835 - Make this multi-user friendly.  For now,
// only slot 0 exists.
const CK_SLOT_ID kDefaultSlotID = 0;

// A helper class to scope a PKCS #11 session.
class ScopedSession {
 public:
  ScopedSession(CK_SLOT_ID slot)
      : handle_(CK_INVALID_HANDLE) {
    CK_RV rv = C_Initialize(NULL);
    if (rv != CKR_OK && rv != CKR_CRYPTOKI_ALREADY_INITIALIZED) {
      // This may be normal in a test environment.
      LOG(INFO) << "PKCS #11 is not available.";
      return;
    }
    CK_FLAGS flags = CKF_RW_SESSION | CKF_SERIAL_SESSION;
    if (C_OpenSession(slot, flags, NULL, NULL, &handle_) != CKR_OK) {
      LOG(ERROR) << "Failed to open PKCS #11 session.";
      return;
    }
  };

  ~ScopedSession() {
    if (IsValid() && (C_CloseSession(handle_) != CKR_OK)) {
      LOG(WARNING) << "Failed to close PKCS #11 session.";
    handle_ = CK_INVALID_HANDLE;
    }
  };

  CK_SESSION_HANDLE handle() {
    return handle_;
  };

  bool IsValid() {
    return (handle_ != CK_INVALID_HANDLE);
  };

 private:
  CK_SESSION_HANDLE handle_;

  DISALLOW_COPY_AND_ASSIGN(ScopedSession);
};

Pkcs11KeyStore::Pkcs11KeyStore() {}

Pkcs11KeyStore::~Pkcs11KeyStore() {}

bool Pkcs11KeyStore::Read(const string& key_name,
                          SecureBlob* key_data) {
  ScopedSession session(kDefaultSlotID);
  if (!session.IsValid())
    return false;
  CK_OBJECT_HANDLE key_handle = FindObject(session.handle(), key_name);
  if (key_handle == CK_INVALID_HANDLE) {
    LOG(INFO) << "Pkcs11KeyStore: Key not found: " << key_name;
    return false;
  }
  // First get the attribute with a NULL buffer which will give us the length.
  CK_ATTRIBUTE attribute = {CKA_VALUE, NULL, 0};
  if (C_GetAttributeValue(session.handle(),
                          key_handle,
                          &attribute, 1) != CKR_OK) {
    LOG(ERROR) << "Pkcs11KeyStore: Failed to read key data: " << key_name;
    return false;
  }
  SecureBlob value_buffer(attribute.ulValueLen);
  attribute.pValue = value_buffer.data();
  if (C_GetAttributeValue(session.handle(),
                          key_handle,
                          &attribute, 1) != CKR_OK) {
    LOG(ERROR) << "Pkcs11KeyStore: Failed to read key data: " << key_name;
    return false;
  }
  key_data->swap(value_buffer);
  return true;
}

bool Pkcs11KeyStore::Write(const string& key_name,
                           const SecureBlob& key_data) {
  ScopedSession session(kDefaultSlotID);
  if (!session.IsValid())
    return false;
  CK_OBJECT_HANDLE key_handle = FindObject(session.handle(), key_name);
  if (key_handle != CK_INVALID_HANDLE) {
    // The key already exists, just replace the key data.
    CK_ATTRIBUTE attribute = {CKA_VALUE,
                              const_cast<CK_VOID_PTR>(key_data.const_data()),
                              key_data.size()};
    if (C_SetAttributeValue(session.handle(),
                            key_handle,
                            &attribute, 1) != CKR_OK) {
      LOG(ERROR) << "Pkcs11KeyStore: Failed to write key data: " << key_name;
      return false;
    }
    return true;
  }
  // Create a new data object for the key.
  CK_OBJECT_CLASS object_class = CKO_DATA;
  CK_BBOOL true_value = CK_TRUE;
  CK_BBOOL false_value = CK_FALSE;
  CK_ATTRIBUTE attributes[] = {
    {CKA_CLASS, &object_class, sizeof(CK_OBJECT_CLASS)},
    {
      CKA_LABEL,
      string_as_array(const_cast<string*>(&key_name)),
      key_name.size()
    },
    {
      CKA_VALUE,
      vector_as_array(const_cast<SecureBlob*>(&key_data)),
      key_data.size()
    },
    {
      CKA_APPLICATION,
      const_cast<char*>(kApplicationID),
      arraysize(kApplicationID)
    },
    {CKA_TOKEN, &true_value, sizeof(CK_BBOOL)},
    {CKA_PRIVATE, &true_value, sizeof(CK_BBOOL)},
    {CKA_MODIFIABLE, &false_value, sizeof(CK_BBOOL)}
  };
  if (C_CreateObject(session.handle(),
                     attributes,
                     arraysize(attributes),
                     &key_handle) != CKR_OK) {
    LOG(ERROR) << "Pkcs11KeyStore: Failed to write key data: " << key_name;
    return false;
  }
  return true;
}

CK_OBJECT_HANDLE Pkcs11KeyStore::FindObject(CK_SESSION_HANDLE session_handle,
                                            const string& key_name) {
  // Assemble a search template.
  CK_OBJECT_CLASS object_class = CKO_DATA;
  CK_BBOOL true_value = CK_TRUE;
  CK_BBOOL false_value = CK_FALSE;
  CK_ATTRIBUTE attributes[] = {
    {CKA_CLASS, &object_class, sizeof(CK_OBJECT_CLASS)},
    {
      CKA_LABEL,
      string_as_array(const_cast<string*>(&key_name)),
      key_name.size()
    },
    {
      CKA_APPLICATION,
      const_cast<char*>(kApplicationID),
      arraysize(kApplicationID)
    },
    {CKA_TOKEN, &true_value, sizeof(CK_BBOOL)},
    {CKA_PRIVATE, &true_value, sizeof(CK_BBOOL)},
    {CKA_MODIFIABLE, &false_value, sizeof(CK_BBOOL)}
  };
  CK_OBJECT_HANDLE key_handle = CK_INVALID_HANDLE;
  CK_ULONG count = 0;
  if ((C_FindObjectsInit(session_handle,
                         attributes,
                         arraysize(attributes)) != CKR_OK) ||
      (C_FindObjects(session_handle, &key_handle, 1, &count) != CKR_OK) ||
      (C_FindObjectsFinal(session_handle) != CKR_OK)) {
    LOG(ERROR) << "Key search failed: " << key_name;
    return CK_INVALID_HANDLE;
  }
  if (count == 1)
    return key_handle;
  return CK_INVALID_HANDLE;
}

}  // namespace cryptohome
