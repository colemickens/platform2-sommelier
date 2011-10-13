// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHAPS_CHAPS_UTILITY_H
#define CHAPS_CHAPS_UTILITY_H

#include <string>
#include <vector>

#include <base/basictypes.h>

#include "chaps/chaps.h"
#include "pkcs11/cryptoki.h"

namespace chaps {

// CopyToCharBuffer copies a NULL-terminated string to a space-padded
// CK_UTF8CHAR buffer (not NULL-terminated).
inline void CopyToCharBuffer(const std::string& source, CK_UTF8CHAR_PTR buffer,
                             size_t buffer_size) {
  size_t copy_size = source.length();
  if (copy_size > buffer_size)
    copy_size = buffer_size;
  memset(buffer, ' ', buffer_size);
  memcpy(buffer, source.data(), copy_size);
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
// does not need to be NULL-terminated. If buffer is NULL, an empty string will
// be returned.
inline std::string ConvertCharBufferToString(CK_UTF8CHAR_PTR buffer,
                                             size_t buffer_size) {
  if (!buffer)
    return std::string();
  return std::string(reinterpret_cast<char*>(buffer), buffer_size);
}

// This function constructs a string object from a CK_BYTE array.  The array
// does not need to be NULL-terminated. If buffer is NULL, an empty string will
// be returned.
inline std::string ConvertByteBufferToString(CK_BYTE_PTR buffer,
                                             CK_ULONG buffer_size) {
  if (!buffer)
    return std::string();
  return std::string(reinterpret_cast<char*>(buffer), buffer_size);
}

// This functions converts a string to a CK_UTF8CHAR_PTR which points to the
// same buffer.
inline CK_UTF8CHAR_PTR ConvertStringToCharBuffer(const char* str) {
  return reinterpret_cast<CK_UTF8CHAR_PTR>(const_cast<char*>(str));
}

// This function changes the container class for an array of bytes from string
// to vector.
inline std::vector<uint8_t> ConvertByteStringToVector(const std::string& s) {
  const uint8_t* front = reinterpret_cast<const uint8_t*>(s.data());
  return std::vector<uint8_t>(front, front + s.length());
}

// This function changes the container class for an array of bytes from vector
// to string.
inline std::string ConvertByteVectorToString(const std::vector<uint8_t>& v) {
  const char* front = reinterpret_cast<const char*>(&v.front());
  return std::string(front, v.size());
}

// This function constructs a vector object from a CK_BYTE array.If buffer is
// NULL, an empty vector will be returned.
inline std::vector<uint8_t> ConvertByteBufferToVector(CK_BYTE_PTR buffer,
                                                      CK_ULONG buffer_size) {
  if (!buffer)
    return std::vector<uint8_t>();
  return std::vector<uint8_t>(buffer, buffer + buffer_size);
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
typedef PreservedValue<uint32_t, CK_ULONG> PreservedUint32_t;

}  // namespace
#endif  // CHAPS_CHAPS_UTILITY_H
