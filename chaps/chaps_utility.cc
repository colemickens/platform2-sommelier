// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chaps/chaps_utility.h"

#include <sstream>
#include <string>
#include <vector>

#include <brillo/secure_blob.h>
#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>
#include <openssl/sha.h>

#include "chaps/attributes.h"
#include "chaps/chaps.h"
#include "pkcs11/cryptoki.h"

using brillo::SecureBlob;
using std::string;
using std::stringstream;
using std::vector;
using ScopedASN1_OCTET_STRING =
    crypto::ScopedOpenSSL<ASN1_OCTET_STRING, ASN1_OCTET_STRING_free>;

namespace {

template <typename OpenSSLType,
          int (*openssl_func)(OpenSSLType*, unsigned char**)>
string ConvertOpenSSLObjectToString(OpenSSLType* type) {
  string output;

  int expected_size = openssl_func(type, nullptr);
  if (expected_size < 0) {
    return string();
  }

  output.resize(expected_size, '\0');

  unsigned char* buf = chaps::ConvertStringToByteBuffer(output.data());
  int real_size = openssl_func(type, &buf);
  CHECK_EQ(expected_size, real_size);

  return output;
}

}  // namespace

namespace chaps {

const size_t kTokenLabelSize = 32;
const CK_ATTRIBUTE_TYPE kKeyBlobAttribute = CKA_VENDOR_DEFINED + 1;
const CK_ATTRIBUTE_TYPE kAuthDataAttribute = CKA_VENDOR_DEFINED + 2;
const CK_ATTRIBUTE_TYPE kLegacyAttribute = CKA_VENDOR_DEFINED + 3;

// Some NSS-specific constants (from NSS' pkcs11n.h).
#define NSSCK_VENDOR_NSS 0x4E534350
#define CKA_NSS (CKA_VENDOR_DEFINED|NSSCK_VENDOR_NSS)
#define CKA_NSS_URL                (CKA_NSS +  1)
#define CKA_NSS_EMAIL              (CKA_NSS +  2)
#define CKA_NSS_SMIME_INFO         (CKA_NSS +  3)
#define CKA_NSS_SMIME_TIMESTAMP    (CKA_NSS +  4)
#define CKA_NSS_PKCS8_SALT         (CKA_NSS +  5)
#define CKA_NSS_PASSWORD_CHECK     (CKA_NSS +  6)
#define CKA_NSS_EXPIRES            (CKA_NSS +  7)
#define CKA_NSS_KRL                (CKA_NSS +  8)

// This value is defined in the latest PKCS#11 header, but we are on an older
// version, thus we leave it here temporarily.
// TODO(crbug/922334): Remove this once we upgrade to the latest version of
// PKCS#11 header.
#define CKA_PUBLIC_KEY_INFO 0x00000129

const char* CK_RVToString(CK_RV value) {
  switch (value) {
    case CKR_OK:
      return "CKR_OK";
    case CKR_CANCEL:
      return "CKR_CANCEL";
    case CKR_HOST_MEMORY:
      return "CKR_HOST_MEMORY";
    case CKR_SLOT_ID_INVALID:
      return "CKR_SLOT_ID_INVALID";
    case CKR_GENERAL_ERROR:
      return "CKR_GENERAL_ERROR";
    case CKR_FUNCTION_FAILED:
      return "CKR_FUNCTION_FAILED";
    case CKR_ARGUMENTS_BAD:
      return "CKR_ARGUMENTS_BAD";
    case CKR_NO_EVENT:
      return "CKR_NO_EVENT";
    case CKR_NEED_TO_CREATE_THREADS:
      return "CKR_NEED_TO_CREATE_THREADS";
    case CKR_CANT_LOCK:
      return "CKR_CANT_LOCK";
    case CKR_ATTRIBUTE_READ_ONLY:
      return "CKR_ATTRIBUTE_READ_ONLY";
    case CKR_ATTRIBUTE_SENSITIVE:
      return "CKR_ATTRIBUTE_SENSITIVE";
    case CKR_ATTRIBUTE_TYPE_INVALID:
      return "CKR_ATTRIBUTE_TYPE_INVALID";
    case CKR_ATTRIBUTE_VALUE_INVALID:
      return "CKR_ATTRIBUTE_VALUE_INVALID";
    case CKR_DATA_INVALID:
      return "CKR_DATA_INVALID";
    case CKR_DATA_LEN_RANGE:
      return "CKR_DATA_LEN_RANGE";
    case CKR_DEVICE_ERROR:
      return "CKR_DEVICE_ERROR";
    case CKR_DEVICE_MEMORY:
      return "CKR_DEVICE_MEMORY";
    case CKR_DEVICE_REMOVED:
      return "CKR_DEVICE_REMOVED";
    case CKR_ENCRYPTED_DATA_INVALID:
      return "CKR_ENCRYPTED_DATA_INVALID";
    case CKR_ENCRYPTED_DATA_LEN_RANGE:
      return "CKR_ENCRYPTED_DATA_LEN_RANGE";
    case CKR_FUNCTION_CANCELED:
      return "CKR_FUNCTION_CANCELED";
    case CKR_FUNCTION_NOT_PARALLEL:
      return "CKR_FUNCTION_NOT_PARALLEL";
    case CKR_FUNCTION_NOT_SUPPORTED:
      return "CKR_FUNCTION_NOT_SUPPORTED";
    case CKR_KEY_HANDLE_INVALID:
      return "CKR_KEY_HANDLE_INVALID";
    case CKR_KEY_SIZE_RANGE:
      return "CKR_KEY_SIZE_RANGE";
    case CKR_KEY_TYPE_INCONSISTENT:
      return "CKR_KEY_TYPE_INCONSISTENT";
    case CKR_KEY_NOT_NEEDED:
      return "CKR_KEY_NOT_NEEDED";
    case CKR_KEY_CHANGED:
      return "CKR_KEY_CHANGED";
    case CKR_KEY_NEEDED:
      return "CKR_KEY_NEEDED";
    case CKR_KEY_INDIGESTIBLE:
      return "CKR_KEY_INDIGESTIBLE";
    case CKR_KEY_FUNCTION_NOT_PERMITTED:
      return "CKR_KEY_FUNCTION_NOT_PERMITTED";
    case CKR_KEY_NOT_WRAPPABLE:
      return "CKR_KEY_NOT_WRAPPABLE";
    case CKR_KEY_UNEXTRACTABLE:
      return "CKR_KEY_UNEXTRACTABLE";
    case CKR_MECHANISM_INVALID:
      return "CKR_MECHANISM_INVALID";
    case CKR_MECHANISM_PARAM_INVALID:
      return "CKR_MECHANISM_PARAM_INVALID";
    case CKR_OBJECT_HANDLE_INVALID:
      return "CKR_OBJECT_HANDLE_INVALID";
    case CKR_OPERATION_ACTIVE:
      return "CKR_OPERATION_ACTIVE";
    case CKR_OPERATION_NOT_INITIALIZED:
      return "CKR_OPERATION_NOT_INITIALIZED";
    case CKR_PIN_INCORRECT:
      return "CKR_PIN_INCORRECT";
    case CKR_PIN_INVALID:
      return "CKR_PIN_INVALID";
    case CKR_PIN_LEN_RANGE:
      return "CKR_PIN_LEN_RANGE";
    case CKR_PIN_EXPIRED:
      return "CKR_PIN_EXPIRED";
    case CKR_PIN_LOCKED:
      return "CKR_PIN_LOCKED";
    case CKR_SESSION_CLOSED:
      return "CKR_SESSION_CLOSED";
    case CKR_SESSION_COUNT:
      return "CKR_SESSION_COUNT";
    case CKR_SESSION_HANDLE_INVALID:
      return "CKR_SESSION_HANDLE_INVALID";
    case CKR_SESSION_PARALLEL_NOT_SUPPORTED:
      return "CKR_SESSION_PARALLEL_NOT_SUPPORTED";
    case CKR_SESSION_READ_ONLY:
      return "CKR_SESSION_READ_ONLY";
    case CKR_SESSION_EXISTS:
      return "CKR_SESSION_EXISTS";
    case CKR_SESSION_READ_ONLY_EXISTS:
      return "CKR_SESSION_READ_ONLY_EXISTS";
    case CKR_SESSION_READ_WRITE_SO_EXISTS:
      return "CKR_SESSION_READ_WRITE_SO_EXISTS";
    case CKR_SIGNATURE_INVALID:
      return "CKR_SIGNATURE_INVALID";
    case CKR_SIGNATURE_LEN_RANGE:
      return "CKR_SIGNATURE_LEN_RANGE";
    case CKR_TEMPLATE_INCOMPLETE:
      return "CKR_TEMPLATE_INCOMPLETE";
    case CKR_TEMPLATE_INCONSISTENT:
      return "CKR_TEMPLATE_INCONSISTENT";
    case CKR_TOKEN_NOT_PRESENT:
      return "CKR_TOKEN_NOT_PRESENT";
    case CKR_TOKEN_NOT_RECOGNIZED:
      return "CKR_TOKEN_NOT_RECOGNIZED";
    case CKR_TOKEN_WRITE_PROTECTED:
      return "CKR_TOKEN_WRITE_PROTECTED";
    case CKR_UNWRAPPING_KEY_HANDLE_INVALID:
      return "CKR_UNWRAPPING_KEY_HANDLE_INVALID";
    case CKR_UNWRAPPING_KEY_SIZE_RANGE:
      return "CKR_UNWRAPPING_KEY_SIZE_RANGE";
    case CKR_UNWRAPPING_KEY_TYPE_INCONSISTENT:
      return "CKR_UNWRAPPING_KEY_TYPE_INCONSISTENT";
    case CKR_USER_ALREADY_LOGGED_IN:
      return "CKR_USER_ALREADY_LOGGED_IN";
    case CKR_USER_NOT_LOGGED_IN:
      return "CKR_USER_NOT_LOGGED_IN";
    case CKR_USER_PIN_NOT_INITIALIZED:
      return "CKR_USER_PIN_NOT_INITIALIZED";
    case CKR_USER_TYPE_INVALID:
      return "CKR_USER_TYPE_INVALID";
    case CKR_USER_ANOTHER_ALREADY_LOGGED_IN:
      return "CKR_USER_ANOTHER_ALREADY_LOGGED_IN";
    case CKR_USER_TOO_MANY_TYPES:
      return "CKR_USER_TOO_MANY_TYPES";
    case CKR_WRAPPED_KEY_INVALID:
      return "CKR_WRAPPED_KEY_INVALID";
    case CKR_WRAPPED_KEY_LEN_RANGE:
      return "CKR_WRAPPED_KEY_LEN_RANGE";
    case CKR_WRAPPING_KEY_HANDLE_INVALID:
      return "CKR_WRAPPING_KEY_HANDLE_INVALID";
    case CKR_WRAPPING_KEY_SIZE_RANGE:
      return "CKR_WRAPPING_KEY_SIZE_RANGE";
    case CKR_WRAPPING_KEY_TYPE_INCONSISTENT:
      return "CKR_WRAPPING_KEY_TYPE_INCONSISTENT";
    case CKR_RANDOM_SEED_NOT_SUPPORTED:
      return "CKR_RANDOM_SEED_NOT_SUPPORTED";
    case CKR_RANDOM_NO_RNG:
      return "CKR_RANDOM_NO_RNG";
    case CKR_DOMAIN_PARAMS_INVALID:
      return "CKR_DOMAIN_PARAMS_INVALID";
    case CKR_BUFFER_TOO_SMALL:
      return "CKR_BUFFER_TOO_SMALL";
    case CKR_SAVED_STATE_INVALID:
      return "CKR_SAVED_STATE_INVALID";
    case CKR_INFORMATION_SENSITIVE:
      return "CKR_INFORMATION_SENSITIVE";
    case CKR_STATE_UNSAVEABLE:
      return "CKR_STATE_UNSAVEABLE";
    case CKR_CRYPTOKI_NOT_INITIALIZED:
      return "CKR_CRYPTOKI_NOT_INITIALIZED";
    case CKR_CRYPTOKI_ALREADY_INITIALIZED:
      return "CKR_CRYPTOKI_ALREADY_INITIALIZED";
    case CKR_MUTEX_BAD:
      return "CKR_MUTEX_BAD";
    case CKR_MUTEX_NOT_LOCKED:
      return "CKR_MUTEX_NOT_LOCKED";
    case CKR_VENDOR_DEFINED:
      return "CKR_VENDOR_DEFINED";
    case CKR_WOULD_BLOCK_FOR_PRIVATE_OBJECTS:
      return "CKR_WOULD_BLOCK_FOR_PRIVATE_OBJECTS";
  }
  return "Unknown";
}

string AttributeToString(CK_ATTRIBUTE_TYPE attribute) {
  switch (attribute) {
    case CKA_CLASS:
      return "CKA_CLASS";
    case CKA_TOKEN:
      return "CKA_TOKEN";
    case CKA_PRIVATE:
      return "CKA_PRIVATE";
    case CKA_LABEL:
      return "CKA_LABEL";
    case CKA_APPLICATION:
      return "CKA_APPLICATION";
    case CKA_VALUE:
      return "CKA_VALUE";
    case CKA_OBJECT_ID:
      return "CKA_OBJECT_ID";
    case CKA_CERTIFICATE_TYPE:
      return "CKA_CERTIFICATE_TYPE";
    case CKA_ISSUER:
      return "CKA_ISSUER";
    case CKA_SERIAL_NUMBER:
      return "CKA_SERIAL_NUMBER";
    case CKA_AC_ISSUER:
      return "CKA_AC_ISSUER";
    case CKA_OWNER:
      return "CKA_OWNER";
    case CKA_ATTR_TYPES:
      return "CKA_ATTR_TYPES";
    case CKA_TRUSTED:
      return "CKA_TRUSTED";
    case CKA_CERTIFICATE_CATEGORY:
      return "CKA_CERTIFICATE_CATEGORY";
    case CKA_CHECK_VALUE:
      return "CKA_CHECK_VALUE";
    case CKA_JAVA_MIDP_SECURITY_DOMAIN:
      return "CKA_JAVA_MIDP_SECURITY_DOMAIN";
    case CKA_URL:
      return "CKA_URL";
    case CKA_HASH_OF_SUBJECT_PUBLIC_KEY:
      return "CKA_HASH_OF_SUBJECT_PUBLIC_KEY";
    case CKA_HASH_OF_ISSUER_PUBLIC_KEY:
      return "CKA_HASH_OF_ISSUER_PUBLIC_KEY";
    case CKA_KEY_TYPE:
      return "CKA_KEY_TYPE";
    case CKA_SUBJECT:
      return "CKA_SUBJECT";
    case CKA_ID:
      return "CKA_ID";
    case CKA_SENSITIVE:
      return "CKA_SENSITIVE";
    case CKA_ENCRYPT:
      return "CKA_ENCRYPT";
    case CKA_DECRYPT:
      return "CKA_DECRYPT";
    case CKA_WRAP:
      return "CKA_WRAP";
    case CKA_UNWRAP:
      return "CKA_UNWRAP";
    case CKA_SIGN:
      return "CKA_SIGN";
    case CKA_SIGN_RECOVER:
      return "CKA_SIGN_RECOVER";
    case CKA_VERIFY:
      return "CKA_VERIFY";
    case CKA_VERIFY_RECOVER:
      return "CKA_VERIFY_RECOVER";
    case CKA_DERIVE:
      return "CKA_DERIVE";
    case CKA_START_DATE:
      return "CKA_START_DATE";
    case CKA_END_DATE:
      return "CKA_END_DATE";
    case CKA_MODULUS:
      return "CKA_MODULUS";
    case CKA_MODULUS_BITS:
      return "CKA_MODULUS_BITS";
    case CKA_PUBLIC_EXPONENT:
      return "CKA_PUBLIC_EXPONENT";
    case CKA_PRIVATE_EXPONENT:
      return "CKA_PRIVATE_EXPONENT";
    case CKA_PRIME_1:
      return "CKA_PRIME_1";
    case CKA_PRIME_2:
      return "CKA_PRIME_2";
    case CKA_EXPONENT_1:
      return "CKA_EXPONENT_1";
    case CKA_EXPONENT_2:
      return "CKA_EXPONENT_2";
    case CKA_COEFFICIENT:
      return "CKA_COEFFICIENT";
    case CKA_PUBLIC_KEY_INFO:
      return "CKA_PUBLIC_KEY_INFO";
    case CKA_PRIME:
      return "CKA_PRIME";
    case CKA_SUBPRIME:
      return "CKA_SUBPRIME";
    case CKA_BASE:
      return "CKA_BASE";
    case CKA_PRIME_BITS:
      return "CKA_PRIME_BITS";
    case CKA_SUBPRIME_BITS:
      return "CKA_SUBPRIME_BITS";
    case CKA_VALUE_BITS:
      return "CKA_VALUE_BITS";
    case CKA_VALUE_LEN:
      return "CKA_VALUE_LEN";
    case CKA_EXTRACTABLE:
      return "CKA_EXTRACTABLE";
    case CKA_LOCAL:
      return "CKA_LOCAL";
    case CKA_NEVER_EXTRACTABLE:
      return "CKA_NEVER_EXTRACTABLE";
    case CKA_ALWAYS_SENSITIVE:
      return "CKA_ALWAYS_SENSITIVE";
    case CKA_KEY_GEN_MECHANISM:
      return "CKA_KEY_GEN_MECHANISM";
    case CKA_MODIFIABLE:
      return "CKA_MODIFIABLE";
    case CKA_ECDSA_PARAMS:
      return "CKA_ECDSA_PARAMS";
    case CKA_EC_POINT:
      return "CKA_EC_POINT";
    case CKA_SECONDARY_AUTH:
      return "CKA_SECONDARY_AUTH";
    case CKA_AUTH_PIN_FLAGS:
      return "CKA_AUTH_PIN_FLAGS";
    case CKA_ALWAYS_AUTHENTICATE:
      return "CKA_ALWAYS_AUTHENTICATE";
    case CKA_WRAP_WITH_TRUSTED:
      return "CKA_WRAP_WITH_TRUSTED";
    case CKA_WRAP_TEMPLATE:
      return "CKA_WRAP_TEMPLATE";
    case CKA_UNWRAP_TEMPLATE:
      return "CKA_UNWRAP_TEMPLATE";
    case CKA_NSS_URL:
      return "CKA_NSS_URL";
    case CKA_NSS_EMAIL:
      return "CKA_NSS_EMAIL";
    case CKA_NSS_SMIME_INFO:
      return "CKA_NSS_SMIME_INFO";
    case CKA_NSS_SMIME_TIMESTAMP:
      return "CKA_NSS_SMIME_TIMESTAMP";
    case CKA_NSS_PKCS8_SALT:
      return "CKA_NSS_PKCS8_SALT";
    case CKA_NSS_PASSWORD_CHECK:
      return "CKA_NSS_PASSWORD_CHECK";
    case CKA_NSS_EXPIRES:
      return "CKA_NSS_EXPIRES";
    case CKA_NSS_KRL:
      return "CKA_NSS_KRL";
    default:
      stringstream ss;
      ss << attribute;
      return ss.str();
  }
  return string();
}

static CK_ULONG ExtractCK_ULONG(const vector<uint8_t>& value) {
  if (value.size() == 1) {
    return static_cast<CK_ULONG>(value[0]);
  } else if (value.size() == 4) {
    CK_ULONG ulong_value = *reinterpret_cast<const uint32_t*>(value.data());
    // If a value should be 64-bits and is only 32-bits, this will make sure the
    // log reflects the potentially invalid value. Some clients may handle this
    // case robustly but NSS has been noted to keep the 32 most significant bits
    // set. We want to log the worst case value.
    if (sizeof(CK_ULONG) == 8)
      ulong_value |= 0xFFFFFFFF00000000;
    return ulong_value;
  } else if (value.size() == 8) {
    return *reinterpret_cast<const uint64_t*>(value.data());
  }
  return -1;
}

static string PrintClass(const vector<uint8_t>& value) {
  CK_ULONG num_value = ExtractCK_ULONG(value);
  switch (num_value) {
    case CKO_DATA:
      return "CKO_DATA";
    case CKO_CERTIFICATE:
      return "CKO_CERTIFICATE";
    case CKO_PUBLIC_KEY:
      return "CKO_PUBLIC_KEY";
    case CKO_PRIVATE_KEY:
      return "CKO_PRIVATE_KEY";
    case CKO_SECRET_KEY:
      return "CKO_SECRET_KEY";
    case CKO_HW_FEATURE:
      return "CKO_HW_FEATURE";
    case CKO_DOMAIN_PARAMETERS:
      return "CKO_DOMAIN_PARAMETERS";
    case CKO_MECHANISM:
      return "CKO_MECHANISM";
    default:
      stringstream ss;
      ss << num_value;
      return ss.str();
  }
  return string();
}

static string PrintKeyType(const vector<uint8_t>& value) {
  CK_ULONG num_value = ExtractCK_ULONG(value);
  switch (num_value) {
    case CKK_RSA:
      return "CKK_RSA";
    case CKK_DSA:
      return "CKK_DSA";
    case CKK_DH:
      return "CKK_DH";
    case CKK_GENERIC_SECRET:
      return "CKK_GENERIC_SECRET";
    case CKK_RC2:
      return "CKK_RC2";
    case CKK_RC4:
      return "CKK_RC4";
    case CKK_RC5:
      return "CKK_RC5";
    case CKK_DES:
      return "CKK_DES";
    case CKK_DES3:
      return "CKK_DES3";
    case CKK_AES:
      return "CKK_AES";
    default:
      stringstream ss;
      ss << num_value;
      return ss.str();
  }
  return string();
}

static string PrintYesNo(const vector<uint8_t>& value) {
  if (!ExtractCK_ULONG(value))
    return "No";
  return "Yes";
}

string ValueToString(CK_ATTRIBUTE_TYPE attribute,
                     const vector<uint8_t>& value) {
  // Some values are sensitive; take a white-list approach.
  switch (attribute) {
    case CKA_CLASS:
      return PrintClass(value);
    case CKA_KEY_TYPE:
      return PrintKeyType(value);
    case CKA_TOKEN:
    case CKA_PRIVATE:
    case CKA_EXTRACTABLE:
    case CKA_SENSITIVE:
    case CKA_ENCRYPT:
    case CKA_DECRYPT:
    case CKA_WRAP:
    case CKA_UNWRAP:
    case CKA_SIGN:
    case CKA_SIGN_RECOVER:
    case CKA_VERIFY:
    case CKA_VERIFY_RECOVER:
    case CKA_DERIVE:
    case CKA_NEVER_EXTRACTABLE:
    case CKA_ALWAYS_SENSITIVE:
    case CKA_ALWAYS_AUTHENTICATE:
      return PrintYesNo(value);
    case CKA_ID:
      return PrintIntVector(value);
    default:
      return "***";
  }
  return string();
}

string PrintAttributes(const vector<uint8_t>& serialized,
                       bool is_value_enabled) {
  stringstream ss;
  ss << "{";
  Attributes attributes;
  if (attributes.Parse(serialized)) {
    for (CK_ULONG i = 0; i < attributes.num_attributes(); i++) {
      CK_ATTRIBUTE& attribute = attributes.attributes()[i];
      if (i > 0)
        ss << ", ";
      ss << AttributeToString(attribute.type);
      if (is_value_enabled) {
        if (attribute.ulValueLen == static_cast<CK_ULONG>(-1)) {
          ss << "=<invalid>";
        } else if (attribute.pValue) {
          uint8_t* buf = reinterpret_cast<uint8_t*>(attribute.pValue);
          vector<uint8_t> value(&buf[0],
                                &buf[attribute.ulValueLen]);
          ss << "=" << ValueToString(attribute.type, value);
        } else {
          ss << " length=" << attribute.ulValueLen;
        }
      }
    }
  }
  ss << "}";
  return ss.str();
}

string Sha1(const string& input) {
  unsigned char digest[SHA_DIGEST_LENGTH];
  SHA1(ConvertStringToByteBuffer(input.data()), input.length(), digest);
  return ConvertByteBufferToString(digest, SHA_DIGEST_LENGTH);
}

SecureBlob Sha1(const SecureBlob& input) {
  unsigned char digest[SHA_DIGEST_LENGTH];
  SHA1(input.data(), input.size(), digest);
  SecureBlob hash(std::begin(digest), std::end(digest));
  brillo::SecureMemset(digest, 0, SHA_DIGEST_LENGTH);
  return hash;
}

SecureBlob Sha256(const SecureBlob& input) {
  unsigned char digest[SHA256_DIGEST_LENGTH];
  SHA256(input.data(), input.size(), digest);
  SecureBlob hash(std::begin(digest), std::end(digest));
  brillo::SecureMemset(digest, 0, SHA256_DIGEST_LENGTH);
  return hash;
}

SecureBlob Sha512(const SecureBlob& input) {
  unsigned char digest[SHA512_DIGEST_LENGTH];
  SHA512(input.data(), input.size(), digest);
  SecureBlob hash(std::begin(digest), std::end(digest));
  brillo::SecureMemset(digest, 0, SHA512_DIGEST_LENGTH);
  return hash;
}

ScopedOpenSSL::ScopedOpenSSL() {
  OpenSSL_add_all_algorithms();
  ERR_load_crypto_strings();
}

ScopedOpenSSL::~ScopedOpenSSL() {
  EVP_cleanup();
  ERR_free_strings();
}

std::string GetOpenSSLError() {
  BIO* bio = BIO_new(BIO_s_mem());
  ERR_print_errors(bio);
  char* data = NULL;
  int data_len = BIO_get_mem_data(bio, &data);
  string error_string(data, data_len);
  BIO_free(bio);
  return error_string;
}

std::string HmacSha512(const std::string& input,
                       const brillo::SecureBlob& key) {
  const int kSha512OutputSize = 64;
  unsigned char mac[kSha512OutputSize];
  HMAC(EVP_sha512(),
       key.data(), key.size(),
       ConvertStringToByteBuffer(input.data()), input.length(),
       mac, NULL);
  return ConvertByteBufferToString(mac, kSha512OutputSize);
}

bool RunCipherInternal(bool is_encrypt,
                       const SecureBlob& key,
                       const string& iv,
                       const string& input,
                       string* output) {
  const size_t kAESKeySizeBytes = 32;
  const size_t kAESBlockSizeBytes = 16;
  CHECK(key.size() == kAESKeySizeBytes);
  CHECK(iv.size() == kAESBlockSizeBytes);
  EVP_CIPHER_CTX cipher_context;
  EVP_CIPHER_CTX_init(&cipher_context);
  if (!EVP_CipherInit_ex(&cipher_context,
                         EVP_aes_256_cbc(),
                         NULL,
                         key.data(),
                         ConvertStringToByteBuffer(iv.data()),
                         is_encrypt)) {
    LOG(ERROR) << "EVP_CipherInit_ex failed: " << GetOpenSSLError();
    return false;
  }
  EVP_CIPHER_CTX_set_padding(&cipher_context,
                             1);  // Enables PKCS padding.
  // Set the buffer size to be large enough to hold all output. For encryption,
  // this will allow space for padding and, for decryption, this will comply
  // with openssl documentation (even though the final output will be no larger
  // than input.length()).
  output->resize(input.length() + kAESBlockSizeBytes);
  int output_length = 0;
  unsigned char* output_bytes = ConvertStringToByteBuffer(output->data());
  unsigned char* input_bytes = ConvertStringToByteBuffer(input.data());
  if (!EVP_CipherUpdate(&cipher_context,
                        output_bytes,
                        &output_length,  // Will be set to actual output length.
                        input_bytes,
                        input.length())) {
    LOG(ERROR) << "EVP_CipherUpdate failed: " << GetOpenSSLError();
    return false;
  }
  // The final block is yet to be computed. This check ensures we have at least
  // kAESBlockSizeBytes bytes left in the output buffer.
  CHECK(output_length <= static_cast<int>(input.length()));
  int output_length2 = 0;
  if (!EVP_CipherFinal_ex(&cipher_context,
                          output_bytes + output_length,
                          &output_length2)) {
    LOG(ERROR) << "EVP_CipherFinal_ex failed: " << GetOpenSSLError();
    return false;
  }
  // Adjust the output size to the number of bytes actually written.
  output->resize(output_length + output_length2);
  EVP_CIPHER_CTX_cleanup(&cipher_context);
  return true;
}

bool RunCipher(bool is_encrypt,
               const SecureBlob& key,
               const string& iv,
               const string& input,
               string* output) {
  const size_t kIVSizeBytes = 16;
  if (!iv.empty())
    return RunCipherInternal(is_encrypt, key, iv, input, output);
  string random_iv;
  string mutable_input = input;
  if (is_encrypt) {
    // Generate a new random IV.
    random_iv.resize(kIVSizeBytes);
    if (1 != RAND_bytes(ConvertStringToByteBuffer(random_iv.data()),
                        kIVSizeBytes)) {
      LOG(ERROR) << "RAND_bytes failed: " << GetOpenSSLError();
      return false;
    }
  } else {
    // Recover and strip the IV from the cipher-text.
    if (input.length() < kIVSizeBytes) {
      LOG(ERROR) << "Decrypt: Invalid input.";
      return false;
    }
    size_t iv_pos = input.length() - kIVSizeBytes;
    random_iv = input.substr(iv_pos);
    mutable_input = input.substr(0, iv_pos);
  }
  if (!RunCipherInternal(is_encrypt, key, random_iv, mutable_input, output))
    return false;
  if (is_encrypt) {
    // Append the random IV to the cipher-text.
    *output += random_iv;
  }
  return true;
}

bool IsIntegralAttribute(CK_ATTRIBUTE_TYPE type) {
  switch (type) {
    case CKA_CLASS:
    case CKA_KEY_TYPE:
    case CKA_MODULUS_BITS:
    case CKA_VALUE_BITS:
    case CKA_VALUE_LEN:
    case CKA_CERTIFICATE_TYPE:
    case CKA_CERTIFICATE_CATEGORY:
    case CKA_PRIME_BITS:
    case CKA_SUBPRIME_BITS:
    case CKA_KEY_GEN_MECHANISM:
    case CKA_HW_FEATURE_TYPE:
    case CKA_MECHANISM_TYPE:
    case CKA_PIXEL_X:
    case CKA_PIXEL_Y:
    case CKA_RESOLUTION:
    case CKA_CHAR_ROWS:
    case CKA_CHAR_COLUMNS:
    case CKA_BITS_PER_PIXEL:
      return true;
  }
  return false;
}

// Both PKCS #11 and OpenSSL use big-endian binary representations of big
// integers.  To convert we can just use the OpenSSL converters.
string ConvertFromBIGNUM(const BIGNUM* bignum) {
  string big_integer(BN_num_bytes(bignum), 0);
  BN_bn2bin(bignum, chaps::ConvertStringToByteBuffer(big_integer.data()));
  return big_integer;
}

BIGNUM* ConvertToBIGNUM(const string& big_integer) {
  if (big_integer.empty())
    return nullptr;
  BIGNUM* b = BN_bin2bn(chaps::ConvertStringToByteBuffer(big_integer.data()),
                        big_integer.length(), nullptr);
  CHECK(b);
  return b;
}

string GetECParametersAsString(const crypto::ScopedEC_KEY& key) {
  return ConvertOpenSSLObjectToString<EC_KEY, i2d_ECParameters>(key.get());
}

// CKA_EC_POINT is DER-encoding of ANSI X9.62 ECPoint value
// The format should be 04 LEN 04 X Y, where the first 04 is the octet string
// tag, LEN is the the content length, the second 04 identifies the uncompressed
// form, and X and Y are the point coordinates.
//
// i2o_ECPublicKey() returns only the content (04 X Y).
string GetECPointAsString(const crypto::ScopedEC_KEY& key) {
  // Convert EC_KEY* to OCT_STRING
  const string oct_string =
      ConvertOpenSSLObjectToString<EC_KEY, i2o_ECPublicKey>(key.get());
  if (oct_string.empty())
    return string();

  // Put OCT_STRING to ASN1_OCTET_STRING
  ScopedASN1_OCTET_STRING os(ASN1_OCTET_STRING_new());
  ASN1_OCTET_STRING_set(os.get(),
                        chaps::ConvertStringToByteBuffer(oct_string.data()),
                        oct_string.size());

  // DER encode ASN1_OCTET_STRING
  return ConvertOpenSSLObjectToString<ASN1_OCTET_STRING, i2d_ASN1_OCTET_STRING>(
      os.get());
}

// In short, we can use d2i_ECParameters to parse CKA_EC_PARAMS and return
// EC_KEY*
//
// CKA_EC_PARAMS is DER-encoding of an ANSI X9.62 Parameters value.
// Parameters ::= CHOICE {
//    ecParameters   ECParameters,
//    namedCurve     CURVES.&id({CurveNames}),
//    implicitlyCA   NULL
// }
// which is as known as EcPKParameters in OpenSSL and RFC 3279.
// EcPKParameters ::= CHOICE {
//    ecParameters  ECParameters,
//    namedCurve    OBJECT IDENTIFIER,
//    implicitlyCA  NULL
// }
crypto::ScopedEC_KEY CreateECCKeyFromEC_PARAMS(const string& input) {
  const unsigned char* buf = chaps::ConvertStringToByteBuffer(input.data());
  crypto::ScopedEC_KEY key(d2i_ECParameters(nullptr, &buf, input.size()));
  return key;
}

}  // namespace chaps
