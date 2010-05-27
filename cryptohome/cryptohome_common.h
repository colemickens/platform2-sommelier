// Copyright (c) 2009-2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTOHOME_COMMON_H_
#define CRYPTOHOME_COMMON_H_

extern "C" {
#include <ecryptfs.h>
}

namespace cryptohome {

// The default symmetric key size for cryptohome is the ecryptfs default
#define CRYPTOHOME_DEFAULT_KEY_SIZE ECRYPTFS_MAX_KEY_BYTES
#define CRYPTOHOME_DEFAULT_KEY_SIGNATURE_SIZE ECRYPTFS_SIG_SIZE
#define CRYPTOHOME_DEFAULT_KEY_SALT_SIZE ECRYPTFS_SALT_SIZE
#define CRYPTOHOME_AES_KEY_BYTES ECRYPTFS_AES_KEY_BYTES
// The default salt length for the user salt
#define CRYPTOHOME_DEFAULT_SALT_LENGTH 16
// Macros for min and max
#define CRYPTOHOME_MIN(X, Y) (((X) < (Y)) ? (X) : (Y))
#define CRYPTOHOME_MAX(X, Y) (((X) > (Y)) ? (X) : (Y))

struct VaultKeysetHeader {
  char signature[2];
  unsigned char major_version;
  unsigned char minor_version;
} __attribute__((__packed__));
typedef struct VaultKeysetHeader VaultKeysetHeader;

struct VaultKeysetKeys {
  unsigned char fek[CRYPTOHOME_DEFAULT_KEY_SIZE];
  unsigned char fek_sig[CRYPTOHOME_DEFAULT_KEY_SIGNATURE_SIZE];
  unsigned char fek_salt[CRYPTOHOME_DEFAULT_KEY_SALT_SIZE];
  unsigned char fnek[CRYPTOHOME_DEFAULT_KEY_SIZE];
  unsigned char fnek_sig[CRYPTOHOME_DEFAULT_KEY_SIGNATURE_SIZE];
  unsigned char fnek_salt[CRYPTOHOME_DEFAULT_KEY_SALT_SIZE];
} __attribute__((__packed__));
typedef struct VaultKeysetKeys VaultKeysetKeys;

}  // cryptohome

#endif  // CRYPTOHOME_COMMON_H_
