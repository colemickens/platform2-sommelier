// Copyright (c) 2009-2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTOHOME_OLD_VAULT_KEYSET_H_
#define CRYPTOHOME_OLD_VAULT_KEYSET_H_

#include <base/basictypes.h>

#include "entropy_source.h"
#include "secure_blob.h"
#include "vault_keyset.h"

namespace cryptohome {

const char kVaultKeysetSignature[] = "ch";

struct OldVaultKeysetHeader {
  char signature[2];
  unsigned char major_version;
  unsigned char minor_version;
} __attribute__((__packed__));
typedef struct VaultKeysetHeader VaultKeysetHeader;

// OldVaultKeyset holds the File Encryption Key (FEK) and File Name Encryption
// Key (FNEK) and their corresponding signatures in the old style
class OldVaultKeyset : public VaultKeyset {
 public:
  OldVaultKeyset();

  bool AssignBuffer(const SecureBlob& buffer);
  bool ToBuffer(SecureBlob* buffer) const;

  static unsigned int SerializedSize();

 private:
  unsigned short major_version_;
  unsigned short minor_version_;

  DISALLOW_COPY_AND_ASSIGN(OldVaultKeyset);
};

}  // namespace cryptohome

#endif  // CRYPTOHOME_OLD_VAULT_KEYSET_H_
