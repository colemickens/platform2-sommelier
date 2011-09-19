// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHAPS_CHAPS_UTILITY_H
#define CHAPS_CHAPS_UTILITY_H

#include <string>

#include <base/basictypes.h>

#include "pkcs11/cryptoki.h"

namespace chaps {

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
const char* RVToString(CK_RV value);

// This macro logs the current function name and the CK_RV value provided.
#define LOG_CK_RV(value) LOG(ERROR) << __func__ << " - " << \
    chaps::RVToString(value);

// This macro is a conditional version of LOG_CK_RV which will log only if the
// value is not CKR_OK.
#define LOG_CK_RV_ERR(value) LOG_IF(ERROR, ((value) != CKR_OK)) << __func__ << \
    " - " << chaps::RVToString(value);

// This macro is a like LOG_CK_RV_IF but also returns the value when it is not
// CKR_OK.
#define LOG_CK_RV_ERR_RETURN(value) LOG_CK_RV_ERR(value); \
    if ((value) != CKR_OK) {return (value);}

// This function constructs a string object from a CK_UTF8CHAR array.  The array
// does not need to be NULL-terminated.
inline std::string CharBufferToString(CK_UTF8CHAR_PTR buffer,
                                      size_t buffer_size) {
  return std::string(reinterpret_cast<char*>(buffer), buffer_size);
}

}  // namespace
#endif  // CHAPS_CHAPS_UTILITY_H
