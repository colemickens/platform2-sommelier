// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHAPS_CHAPS_UTILITY_H
#define CHAPS_CHAPS_UTILITY_H

#include <base/basictypes.h>

#include "pkcs11/cryptoki.h"

namespace chaps {

// Type conversions for Cryptoki types.
inline size_t UlongToSize(CK_ULONG ul) {
  COMPILE_ASSERT(sizeof(size_t) == sizeof(CK_ULONG), size_t_mismatch);
  return ul;
}

inline CK_ULONG SizeToUlong(size_t s) {
  COMPILE_ASSERT(sizeof(size_t) == sizeof(CK_ULONG), size_t_mismatch);
  return s;
}

// The BytePtrToRef8 function is used to help provide output arguments in the
// format expected by dbus-c++ generated code.
inline uint8_t& BytePtrToRef8(CK_BYTE* pointer) {
  COMPILE_ASSERT(sizeof(uint8_t) == sizeof(CK_BYTE), uint8_t_size_mismatch);
  return *reinterpret_cast<uint8_t*>(pointer);
}

// CopyToCharBuffer copies a NULL-terminated string to a space-padded
// CK_UTF8CHAR buffer (not NULL-terminated).
inline void CopyToCharBuffer(const char* source, CK_UTF8CHAR_PTR buffer,
                             size_t buffer_size) {
  size_t copy_size = strlen(source);
  if (copy_size > buffer_size)
    copy_size = buffer_size;
  memset(buffer, ' ', buffer_size);
  memcpy(buffer, source, copy_size);
}

// RVToString stringifies a PKCS #11 return value.  E.g. CKR_OK --> "CKR_OK".
const char* RVToString(CK_RV rv);

}  // namespace
#endif  // CHAPS_CHAPS_UTILITY_H
