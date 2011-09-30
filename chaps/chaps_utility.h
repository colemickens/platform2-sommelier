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
const char* CK_RVToString(CK_RV value);

// This macro logs the current function name and the CK_RV value provided.
#define LOG_CK_RV(value) LOG(ERROR) << __func__ << " - " << \
    chaps::CK_RVToString(value);

// This macro is a conditional version of LOG_CK_RV which will log only if the
// value is not CKR_OK.
#define LOG_CK_RV_ERR(value) LOG_IF(ERROR, ((value) != CKR_OK)) << __func__ << \
    " - " << chaps::CK_RVToString(value);

// This macro logs and returns the given CK_RV value.
#define LOG_CK_RV_AND_RETURN(value) {LOG_CK_RV(value); return (value);}

// This macro logs and returns the given CK_RV value if the given condition is
// true.
#define LOG_CK_RV_AND_RETURN_IF(condition, value) if (condition) \
    LOG_CK_RV_AND_RETURN(value)

// This macro logs and returns the given CK_RV value if the value is not CKR_OK.
#define LOG_CK_RV_AND_RETURN_IF_ERR(value) \
    LOG_CK_RV_AND_RETURN_IF((value) != CKR_OK, value)

// This function constructs a string object from a CK_UTF8CHAR array.  The array
// does not need to be NULL-terminated.
inline std::string CharBufferToString(CK_UTF8CHAR_PTR buffer,
                                      size_t buffer_size) {
  return std::string(reinterpret_cast<char*>(buffer), buffer_size);
}

// This class preserves a variable that needs to be temporarily converted to
// another type.
template <class PreservedType, class TempType>
class PreservedValue {
 public:
  PreservedValue(PreservedType* value) {
    CHECK(value);
    preserved_ = value;
    temp_ = static_cast<TempType>(*value);
  }
  ~PreservedValue() {
    *preserved_ = static_cast<PreservedType>(temp_);
  }
  // Allow an implicit cast to a pointer to the temporary value.
  operator TempType*() {
    return &temp_;
  }
 private:
  PreservedType* preserved_;
  TempType temp_;
};

typedef PreservedValue<CK_ULONG, uint32_t> PreservedCK_ULONG;

}  // namespace
#endif  // CHAPS_CHAPS_UTILITY_H
