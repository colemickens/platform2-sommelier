/* Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Private header file for mount-encrypted helper tool.
 */
#ifndef CRYPTOHOME_MOUNT_ENCRYPTED_MOUNT_ENCRYPTED_H_
#define CRYPTOHOME_MOUNT_ENCRYPTED_MOUNT_ENCRYPTED_H_

#include <sys/types.h>
#include <unistd.h>

#include <openssl/err.h>
#include <openssl/sha.h>

#include <base/files/file_path.h>
#include <base/macros.h>

#define DIGEST_LENGTH SHA256_DIGEST_LENGTH

enum result_code {
  RESULT_SUCCESS = 0,
  RESULT_FAIL_FATAL = 1,
};

// Simplify logging of base::FilePath. Note that this has appeared in upstream
// chromium base/ already and can be removed once it has propagated to Chrome
// OS' base copy.
static inline std::ostream& operator<<(std::ostream& out,
                                       const base::FilePath& file_path) {
  return out << file_path.value();
}

#endif  // CRYPTOHOME_MOUNT_ENCRYPTED_MOUNT_ENCRYPTED_H_
