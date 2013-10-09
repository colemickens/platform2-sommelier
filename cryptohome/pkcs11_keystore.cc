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
#include <chromeos/cryptohome.h>
#include <chromeos/secure_blob.h>
#include <openssl/rsa.h>

#include "cryptolib.h"
#include "pkcs11_init.h"

using chromeos::SecureBlob;
using std::string;

namespace cryptohome {

// An arbitrary application ID to identify PKCS #11 objects.
const char kApplicationID[] = "CrOS_d5bbc079d2497110feadfc97c40d718ae46f4658";

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

Pkcs11KeyStore::Pkcs11KeyStore() : default_pkcs11_init_(new Pkcs11Init),
                                   pkcs11_init_(default_pkcs11_init_.get()) {}

Pkcs11KeyStore::Pkcs11KeyStore(Pkcs11Init* pkcs11_init)
    : pkcs11_init_(pkcs11_init) {}

Pkcs11KeyStore::~Pkcs11KeyStore() {}

bool Pkcs11KeyStore::Read(const string& username,
                          const string& key_name,
                          SecureBlob* key_data) {
  CK_SLOT_ID slot;
  if (!GetUserSlot(username, &slot))
    return false;
  ScopedSession session(slot);
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

bool Pkcs11KeyStore::Write(const string& username,
                           const string& key_name,
                           const SecureBlob& key_data) {
  // Delete any existing key with the same name.
  if (!Delete(username, key_name))
    return false;
  CK_SLOT_ID slot;
  if (!GetUserSlot(username, &slot))
    return false;
  ScopedSession session(slot);
  if (!session.IsValid())
    return false;
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
  CK_OBJECT_HANDLE key_handle = CK_INVALID_HANDLE;
  if (C_CreateObject(session.handle(),
                     attributes,
                     arraysize(attributes),
                     &key_handle) != CKR_OK) {
    LOG(ERROR) << "Pkcs11KeyStore: Failed to write key data: " << key_name;
    return false;
  }
  return true;
}

bool Pkcs11KeyStore::Delete(const string& username,
                            const std::string& key_name) {
  CK_SLOT_ID slot;
  if (!GetUserSlot(username, &slot))
    return false;
  ScopedSession session(slot);
  if (!session.IsValid())
    return false;
  CK_OBJECT_HANDLE key_handle = FindObject(session.handle(), key_name);
  if (key_handle != CK_INVALID_HANDLE) {
    if (C_DestroyObject(session.handle(), key_handle) != CKR_OK) {
      LOG(ERROR) << "Pkcs11KeyStore: Failed to delete key data.";
      return false;
    }
  }
  return true;
}

bool Pkcs11KeyStore::Register(const string& username,
                              const chromeos::SecureBlob& private_key_blob,
                              const chromeos::SecureBlob& public_key_der) {
  const CK_ATTRIBUTE_TYPE kKeyBlobAttribute = CKA_VENDOR_DEFINED + 1;

  CK_SLOT_ID slot;
  if (!GetUserSlot(username, &slot))
    return false;
  ScopedSession session(slot);
  if (!session.IsValid())
    return false;

  // Extract the modulus from the public key.
  const unsigned char* asn1_ptr = &public_key_der.front();
  RSA* public_key = d2i_RSAPublicKey(NULL, &asn1_ptr, public_key_der.size());
  if (!public_key) {
    LOG(ERROR) << "Pkcs11KeyStore: Failed to decode public key.";
    return false;
  }
  SecureBlob modulus(BN_num_bytes(public_key->n));
  int length = BN_bn2bin(public_key->n, &modulus.front());
  if (length <= 0) {
    LOG(ERROR) << "Pkcs11KeyStore: Failed to extract public key modulus.";
    RSA_free(public_key);
    return false;
  }
  modulus.resize(length);
  RSA_free(public_key);

  // Construct a PKCS #11 template for the public key object.
  CK_BBOOL true_value = CK_TRUE;
  CK_BBOOL false_value = CK_FALSE;
  CK_KEY_TYPE key_type = CKK_RSA;
  CK_OBJECT_CLASS public_key_class = CKO_PUBLIC_KEY;
  SecureBlob id = CryptoLib::Sha1(modulus);
  CK_ULONG modulus_bits = modulus.size() * 8;
  unsigned char public_exponent[] = {1, 0, 1};
  CK_ATTRIBUTE public_key_attributes[] = {
    {CKA_CLASS, &public_key_class, sizeof(CK_OBJECT_CLASS)},
    {CKA_TOKEN, &true_value, sizeof(CK_BBOOL)},
    {CKA_DERIVE, &false_value, sizeof(CK_BBOOL)},
    {CKA_WRAP, &false_value, sizeof(CK_BBOOL)},
    {CKA_VERIFY, &true_value, sizeof(CK_BBOOL)},
    {CKA_VERIFY_RECOVER, &false_value, sizeof(CK_BBOOL)},
    {CKA_ENCRYPT, &false_value, sizeof(CK_BBOOL)},
    {CKA_KEY_TYPE, &key_type, sizeof(CK_KEY_TYPE)},
    {CKA_ID, id.data(), id.size()},
    {CKA_MODULUS_BITS, &modulus_bits, sizeof(CK_ULONG)},
    {CKA_PUBLIC_EXPONENT, public_exponent, arraysize(public_exponent)},
    {CKA_MODULUS, modulus.data(), modulus.size()}
  };

  CK_OBJECT_HANDLE object_handle = CK_INVALID_HANDLE;
  if (C_CreateObject(session.handle(),
                     public_key_attributes,
                     arraysize(public_key_attributes),
                     &object_handle) != CKR_OK) {
    LOG(ERROR) << "Pkcs11KeyStore: Failed to create public key object.";
    return false;
  }

  // Construct a PKCS #11 template for the private key object.
  CK_OBJECT_CLASS private_key_class = CKO_PRIVATE_KEY;
  CK_ATTRIBUTE private_key_attributes[] = {
    {CKA_CLASS, &private_key_class, sizeof(CK_OBJECT_CLASS)},
    {CKA_TOKEN, &true_value, sizeof(CK_BBOOL)},
    {CKA_PRIVATE, &true_value, sizeof(CK_BBOOL)},
    {CKA_SENSITIVE, &true_value, sizeof(CK_BBOOL)},
    {CKA_EXTRACTABLE, &false_value, sizeof(CK_BBOOL)},
    {CKA_DERIVE, &false_value, sizeof(CK_BBOOL)},
    {CKA_UNWRAP, &false_value, sizeof(CK_BBOOL)},
    {CKA_SIGN, &true_value, sizeof(CK_BBOOL)},
    {CKA_SIGN_RECOVER, &false_value, sizeof(CK_BBOOL)},
    {CKA_DECRYPT, &false_value, sizeof(CK_BBOOL)},
    {CKA_KEY_TYPE, &key_type, sizeof(CK_KEY_TYPE)},
    {CKA_ID, id.data(), id.size()},
    {CKA_PUBLIC_EXPONENT, public_exponent, arraysize(public_exponent)},
    {CKA_MODULUS, modulus.data(), modulus.size()},
    {
      kKeyBlobAttribute,
      const_cast<CK_VOID_PTR>(private_key_blob.const_data()),
      private_key_blob.size()
    }
  };

  if (C_CreateObject(session.handle(),
                     private_key_attributes,
                     arraysize(private_key_attributes),
                     &object_handle) != CKR_OK) {
    LOG(ERROR) << "Pkcs11KeyStore: Failed to create private key object.";
    return false;
  }

  // Close all sessions in an attempt to trigger other modules to find the new
  // objects.
  C_CloseAllSessions(slot);

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

bool Pkcs11KeyStore::GetUserSlot(const string& username, CK_SLOT_ID_PTR slot) {
  if (username.empty()) {
    LOG(WARNING) << "Warning: No username: Using default PKCS #11 slot.";
    *slot = Pkcs11Init::kDefaultTpmSlotId;
    return true;
  }
  const char *kChapsDaemonName = "chaps";
  FilePath token_path =
      chromeos::cryptohome::home::GetDaemonPath(username, kChapsDaemonName);
  return pkcs11_init_->GetTpmTokenSlotForPath(token_path, slot);
}

}  // namespace cryptohome
