// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A PKCS #11 backed KeyStore implementation.

#ifndef CRYPTOHOME_PKCS11_KEYSTORE_H_
#define CRYPTOHOME_PKCS11_KEYSTORE_H_

#include "cryptohome/keystore.h"

#include <string>

#include <base/basictypes.h>
#include <base/callback_forward.h>
#include <base/memory/scoped_ptr.h>
#include <chaps/pkcs11/cryptoki.h>
#include <chromeos/secure_blob.h>

namespace cryptohome {

class Pkcs11Init;

// This class uses a PKCS #11 token as storage for key data.  The key data is
// stored in data objects with the following attributes:
// CKA_CLASS - CKO_DATA
// CKA_LABEL - A key name.
// CKA_VALUE - Binary key data (opaque to this class and the PKCS #11 token).
// CKA_APPLICATION - A constant value associated with this class.
// CKA_TOKEN - True
// CKA_PRIVATE - True
// CKA_MODIFIABLE - False
// There is no barrier between the objects created by this class and any other
// objects residing in the same token.  In practice, this means that any
// component with access to the PKCS #11 token also has access to read or delete
// key data.
class Pkcs11KeyStore : public KeyStore {
 public:
  Pkcs11KeyStore();
  explicit Pkcs11KeyStore(Pkcs11Init* pkcs11_init);
  virtual ~Pkcs11KeyStore();

  // KeyStore interface.
  virtual bool Read(const std::string& username,
                    const std::string& key_name,
                    chromeos::SecureBlob* key_data);
  virtual bool Write(const std::string& username,
                     const std::string& key_name,
                     const chromeos::SecureBlob& key_data);
  virtual bool Delete(const std::string& username,
                      const std::string& key_name);
  virtual bool DeleteByPrefix(const std::string& username,
                              const std::string& key_prefix);
  virtual bool Register(const std::string& username,
                        const chromeos::SecureBlob& private_key_blob,
                        const chromeos::SecureBlob& public_key_der);

 private:
  typedef base::Callback<bool(const std::string& key_name,
                              CK_OBJECT_HANDLE object_handle)>
      EnumObjectsCallback;

  scoped_ptr<Pkcs11Init> default_pkcs11_init_;
  Pkcs11Init* pkcs11_init_;

  // Searches for a PKCS #11 object for a given key name.  If one exists, the
  // object handle is returned, otherwise CK_INVALID_HANDLE is returned.
  CK_OBJECT_HANDLE FindObject(CK_SESSION_HANDLE session_handle,
                              const std::string& key_name);

  // Gets a slot for the given user.  If |username| is empty then the default
  // slot (0) is provided.  Returns false if no slot is found for |username|.
  bool GetUserSlot(const std::string& username, CK_SLOT_ID_PTR slot);

  // Enumerates all PKCS #11 objects associated with keys.  The |callback| is
  // called once for each object.
  bool EnumObjects(CK_SESSION_HANDLE session_handle,
                   const EnumObjectsCallback& callback);

  // Looks up the key name for the given |object_handle| which is associated
  // with a key.  Returns true on success.
  bool GetKeyName(CK_SESSION_HANDLE session_handle,
                  CK_OBJECT_HANDLE object_handle,
                  std::string* key_name);

  // An EnumObjectsCallback for use with DeleteByPrefix.  Destroys the key
  // object identified by |object_handle| if |key_name| matches |key_prefix|.
  // Returns true on success.
  bool DeleteIfMatchesPrefix(CK_SESSION_HANDLE session_handle,
                             const std::string& key_prefix,
                             const std::string& key_name,
                             CK_OBJECT_HANDLE object_handle);

  DISALLOW_COPY_AND_ASSIGN(Pkcs11KeyStore);
};

}  // namespace cryptohome

#endif  // CRYPTOHOME_PKCS11_KEYSTORE_H_
