// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHAPS_CHAPS_UTILITY_H_
#define CHAPS_CHAPS_UTILITY_H_

#include <string.h>

#include <sstream>
#include <string>
#include <vector>

#include <base/logging.h>
#include <base/stl_util.h>
#include <brillo/secure_blob.h>
#include <crypto/scoped_openssl_types.h>

#include "chaps/chaps.h"
#include "pkcs11/cryptoki.h"

namespace chaps {

enum class DigestAlgorithm {
  MD5 = 0,
  SHA1 = 1,
  SHA256 = 2,
  SHA384 = 3,
  SHA512 = 4,
};

// These strings are the DER encodings of the DigestInfo values for the
// supported digest algorithms.  See PKCS #1 v2.1: 9.2.
const struct {
  const char* encoding;
  size_t encoding_len;
} kDigestAlgorithmEncoding[] = {
  { /* MD5 = 0 */
    "\x30\x20\x30\x0c\x06\x08\x2a\x86\x48\x86\xf7\x0d\x02\x05\x05\x00\x04"
    "\x10",
    18 },
  { /* SHA1 = 1 */
    "\x30\x21\x30\x09\x06\x05\x2b\x0e\x03\x02\x1a\x05\x00\x04\x14",
    15 },
  { /* SHA256 = 2 */
    "\x30\x31\x30\x0d\x06\x09\x60\x86\x48\x01\x65\x03\x04\x02\x01\x05\x00\x04"
    "\x20",
    19 },
  { /* SHA384 = 3 */
    "\x30\x41\x30\x0d\x06\x09\x60\x86\x48\x01\x65\x03\x04\x02\x02\x05\x00\x04"
    "\x30",
    19 },
  { /* SHA512 = 4 */
    "\x30\x51\x30\x0d\x06\x09\x60\x86\x48\x01\x65\x03\x04\x02\x03\x05\x00\x04"
    "\x40",
    19 },
};

// Get the algorithm ID for DigestInfo structure
inline std::string GetDigestAlgorithmEncoding(DigestAlgorithm alg) {
  size_t alg_index = static_cast<size_t>(alg);
  if (alg_index >= arraysize(kDigestAlgorithmEncoding)) {
    return std::string();
  }
  return std::string(kDigestAlgorithmEncoding[alg_index].encoding,
                     kDigestAlgorithmEncoding[alg_index].encoding_len);
}

// Copy*ToCharBuffer copies to a space-padded CK_UTF8CHAR buffer (not
// NULL-terminated).
inline void CopyStringToCharBuffer(const std::string& source,
                                   CK_UTF8CHAR_PTR buffer,
                                   size_t buffer_size) {
  size_t copy_size = source.length();
  if (copy_size > buffer_size)
    copy_size = buffer_size;
  memset(buffer, ' ', buffer_size);
  if (copy_size > 0)
    memcpy(buffer, source.data(), copy_size);
}

inline void CopyVectorToCharBuffer(const std::vector<uint8_t>& source,
                                   CK_UTF8CHAR_PTR buffer,
                                   size_t buffer_size) {
  size_t copy_size = source.size();
  if (copy_size > buffer_size)
    copy_size = buffer_size;
  memset(buffer, ' ', buffer_size);
  if (copy_size > 0)
    memcpy(buffer, source.data(), copy_size);
}

// RVToString stringifies a PKCS #11 return value.  E.g. CKR_OK --> "CKR_OK".
EXPORT_SPEC const char* CK_RVToString(CK_RV value);

// AttributeToString stringifies a PKCS #11 attribute type.
std::string AttributeToString(CK_ATTRIBUTE_TYPE attribute);

// ValueToString stringifies a PKCS #11 attribute value.
std::string ValueToString(CK_ATTRIBUTE_TYPE attribute,
                          const std::vector<uint8_t>& value);

// PrintAttributes parses serialized attributes and prints in the form:
// "{attribute1[=value1], attribute2[=value2]}".
std::string PrintAttributes(const std::vector<uint8_t>& serialized,
                            bool is_value_enabled);

// PrintIntVector prints a vector in array literal form.  E.g. "{0, 1, 2}".
// ** A static cast to 'int' must be possible for type T.
template <class T>
std::string PrintIntVector(const std::vector<T>& v) {
  std::stringstream ss;
  ss << "{";
  for (size_t i = 0; i < v.size(); i++) {
    if (i > 0)
      ss << ", ";
    ss << static_cast<int>(v[i]);
  }
  ss << "}";
  return ss.str();
}

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

// This function converts a C string to a CK_UTF8CHAR_PTR which points to the
// same buffer.
inline CK_UTF8CHAR_PTR ConvertStringToCharBuffer(const char* str) {
  return reinterpret_cast<CK_UTF8CHAR_PTR>(const_cast<char*>(str));
}

// This function converts a C string to a uint8_t* which points to the same
// buffer.
inline uint8_t* ConvertStringToByteBuffer(const char* str) {
  return reinterpret_cast<uint8_t*>(const_cast<char*>(str));
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
  const char* front = reinterpret_cast<const char*>(v.data());
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

// This function returns a value composed of the bytes in the given string.
// It only accepts strings whose length is the same as the size of the given
// type. The type must be default-constructible.
template <typename T>
T ExtractFromByteString(const std::string& s) {
  CHECK(s.length() == sizeof(T));
  T ret;
  memcpy(&ret, s.data(), sizeof(ret));
  return ret;
}

// This class preserves a variable that needs to be temporarily converted to
// another type.
template <class PreservedType, class TempType>
class PreservedValue {
 public:
  explicit PreservedValue(PreservedType* value) {
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

typedef PreservedValue<CK_ULONG, uint64_t> PreservedCK_ULONG;
typedef PreservedValue<uint64_t, CK_ULONG> PreservedUint64_t;

class PreservedByteVector {
 public:
  explicit PreservedByteVector(std::vector<uint8_t>* value) {
    CHECK(value);
    preserved_ = value;
    temp_ = ConvertByteVectorToString(*value);
  }
  ~PreservedByteVector() {
    *preserved_ = ConvertByteStringToVector(temp_);
  }
  // Allow an implicit cast to a string pointer.
  operator std::string*() {
    return &temp_;
  }
 private:
  std::vector<uint8_t>* preserved_;
  std::string temp_;
};

// Computes and returns a SHA-1 hash of the given input.
std::string Sha1(const std::string& input);
brillo::SecureBlob Sha1(const brillo::SecureBlob& input);

// Computes and returns a SHA-256 hash of the given input.
brillo::SecureBlob Sha256(const brillo::SecureBlob& input);

// Computes and returns a SHA-512 hash of the given input.
brillo::SecureBlob Sha512(const brillo::SecureBlob& input);

// Initializes the OpenSSL library on construction and terminates the library on
// destruction.
class ScopedOpenSSL {
 public:
  ScopedOpenSSL();
  ~ScopedOpenSSL();
};

// Returns a description of the OpenSSL error stack.
std::string GetOpenSSLError();

// Computes a message authentication code using HMAC and SHA-512.
std::string HmacSha512(const std::string& input,
                       const brillo::SecureBlob& key);

// Performs AES-256 encryption / decryption in CBC mode with PKCS padding. If
// 'iv' is left empty, a random IV will be generated and appended to the cipher-
// text on encryption.
bool RunCipher(bool is_encrypt,
               const brillo::SecureBlob& key,
               const std::string& iv,
               const std::string& input,
               std::string* output);

// Returns true if the given attribute type has an integral value.
bool IsIntegralAttribute(CK_ATTRIBUTE_TYPE type);

inline void ClearString(std::string* str) {
  brillo::SecureMemset(base::string_as_array(str), 0, str->length());
}

inline void ClearVector(std::vector<uint8_t>* vector) {
  brillo::SecureMemset(vector->data(), 0, vector->size());
}

// TODO(crbug/916023): Move pure OpenSSL conversion wrappers to a cross daemon
// library.
//
// OpenSSL type <--> raw data string
//

// Convert OpenSSL BIGNUM |bignum| to string.
// Padding the result to at least |pad_to_length| bytes long.
// Return empty string on error.
std::string ConvertFromBIGNUM(const BIGNUM* bignum, int pad_to_length = 0);

// Convert string |big_integer| back to a new OpenSSL BIGNUM.
// Caller takes the ownership. Returns nullptr if big_integer is empty.
BIGNUM* ConvertToBIGNUM(const std::string& big_integer);

//
// OpenSSL type <--> DER-encoded string
//

// Get the ECParamters from |key| and DER-encode to a string.
std::string GetECParametersAsString(const crypto::ScopedEC_KEY& key);
// Get the EC_Point from |key| and DER-encode to a string.
std::string GetECPointAsString(const crypto::ScopedEC_KEY& key);

//
// OpenSSL type <--> PKCS #11 Attributes
//

// Create a OpenSSL EC_KEY from a CKA_EC_PARAMS string (which is compatible
// with DER-encoded OpenSSL ECParameters)
crypto::ScopedEC_KEY CreateECCKeyFromEC_PARAMS(const std::string& ec_params);

}  // namespace chaps

#endif  // CHAPS_CHAPS_UTILITY_H_
