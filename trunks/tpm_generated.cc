// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// THIS CODE IS GENERATED - DO NOT MODIFY!

#include "trunks/tpm_generated.h"


#include <string>

#include <base/basictypes.h>
#include <base/bind.h>
#include <base/callback.h>
#include <base/logging.h>
#include <base/stl_util.h>
#include <base/sys_byteorder.h>
#include <crypto/secure_hash.h>

#include "trunks/authorization_delegate.h"
#include "trunks/command_transceiver.h"
#include "trunks/error_codes.h"


namespace trunks {

TPM_RC Serialize_uint8_t(const uint8_t& value, std::string* buffer) {
  VLOG(2) << __func__;
  uint8_t value_net = value;
  switch (sizeof(uint8_t)) {
    case 2:
      value_net = base::HostToNet16(value);
      break;
    case 4:
      value_net = base::HostToNet32(value);
      break;
    case 8:
      value_net = base::HostToNet64(value);
      break;
    default:
      break;
  }
  const char* value_bytes = reinterpret_cast<const char*>(&value_net);
  buffer->append(value_bytes, sizeof(uint8_t));
  return TPM_RC_SUCCESS;
}

TPM_RC Parse_uint8_t(
    std::string* buffer,
    uint8_t* value,
    std::string* value_bytes) {
  VLOG(2) << __func__;
  if (buffer->size() < sizeof(uint8_t))
    return TPM_RC_INSUFFICIENT;
  uint8_t value_net = 0;
  memcpy(&value_net, buffer->data(), sizeof(uint8_t));
  switch (sizeof(uint8_t)) {
    case 2:
      *value = base::NetToHost16(value_net);
      break;
    case 4:
      *value = base::NetToHost32(value_net);
      break;
    case 8:
      *value = base::NetToHost64(value_net);
      break;
    default:
      *value = value_net;
  }
  if (value_bytes) {
    value_bytes->append(buffer->substr(0, sizeof(uint8_t)));
  }
  buffer->erase(0, sizeof(uint8_t));
  return TPM_RC_SUCCESS;
}

TPM_RC Serialize_int8_t(const int8_t& value, std::string* buffer) {
  VLOG(2) << __func__;
  int8_t value_net = value;
  switch (sizeof(int8_t)) {
    case 2:
      value_net = base::HostToNet16(value);
      break;
    case 4:
      value_net = base::HostToNet32(value);
      break;
    case 8:
      value_net = base::HostToNet64(value);
      break;
    default:
      break;
  }
  const char* value_bytes = reinterpret_cast<const char*>(&value_net);
  buffer->append(value_bytes, sizeof(int8_t));
  return TPM_RC_SUCCESS;
}

TPM_RC Parse_int8_t(
    std::string* buffer,
    int8_t* value,
    std::string* value_bytes) {
  VLOG(2) << __func__;
  if (buffer->size() < sizeof(int8_t))
    return TPM_RC_INSUFFICIENT;
  int8_t value_net = 0;
  memcpy(&value_net, buffer->data(), sizeof(int8_t));
  switch (sizeof(int8_t)) {
    case 2:
      *value = base::NetToHost16(value_net);
      break;
    case 4:
      *value = base::NetToHost32(value_net);
      break;
    case 8:
      *value = base::NetToHost64(value_net);
      break;
    default:
      *value = value_net;
  }
  if (value_bytes) {
    value_bytes->append(buffer->substr(0, sizeof(int8_t)));
  }
  buffer->erase(0, sizeof(int8_t));
  return TPM_RC_SUCCESS;
}

TPM_RC Serialize_int(const int& value, std::string* buffer) {
  VLOG(2) << __func__;
  int value_net = value;
  switch (sizeof(int)) {
    case 2:
      value_net = base::HostToNet16(value);
      break;
    case 4:
      value_net = base::HostToNet32(value);
      break;
    case 8:
      value_net = base::HostToNet64(value);
      break;
    default:
      break;
  }
  const char* value_bytes = reinterpret_cast<const char*>(&value_net);
  buffer->append(value_bytes, sizeof(int));
  return TPM_RC_SUCCESS;
}

TPM_RC Parse_int(
    std::string* buffer,
    int* value,
    std::string* value_bytes) {
  VLOG(2) << __func__;
  if (buffer->size() < sizeof(int))
    return TPM_RC_INSUFFICIENT;
  int value_net = 0;
  memcpy(&value_net, buffer->data(), sizeof(int));
  switch (sizeof(int)) {
    case 2:
      *value = base::NetToHost16(value_net);
      break;
    case 4:
      *value = base::NetToHost32(value_net);
      break;
    case 8:
      *value = base::NetToHost64(value_net);
      break;
    default:
      *value = value_net;
  }
  if (value_bytes) {
    value_bytes->append(buffer->substr(0, sizeof(int)));
  }
  buffer->erase(0, sizeof(int));
  return TPM_RC_SUCCESS;
}

TPM_RC Serialize_uint16_t(const uint16_t& value, std::string* buffer) {
  VLOG(2) << __func__;
  uint16_t value_net = value;
  switch (sizeof(uint16_t)) {
    case 2:
      value_net = base::HostToNet16(value);
      break;
    case 4:
      value_net = base::HostToNet32(value);
      break;
    case 8:
      value_net = base::HostToNet64(value);
      break;
    default:
      break;
  }
  const char* value_bytes = reinterpret_cast<const char*>(&value_net);
  buffer->append(value_bytes, sizeof(uint16_t));
  return TPM_RC_SUCCESS;
}

TPM_RC Parse_uint16_t(
    std::string* buffer,
    uint16_t* value,
    std::string* value_bytes) {
  VLOG(2) << __func__;
  if (buffer->size() < sizeof(uint16_t))
    return TPM_RC_INSUFFICIENT;
  uint16_t value_net = 0;
  memcpy(&value_net, buffer->data(), sizeof(uint16_t));
  switch (sizeof(uint16_t)) {
    case 2:
      *value = base::NetToHost16(value_net);
      break;
    case 4:
      *value = base::NetToHost32(value_net);
      break;
    case 8:
      *value = base::NetToHost64(value_net);
      break;
    default:
      *value = value_net;
  }
  if (value_bytes) {
    value_bytes->append(buffer->substr(0, sizeof(uint16_t)));
  }
  buffer->erase(0, sizeof(uint16_t));
  return TPM_RC_SUCCESS;
}

TPM_RC Serialize_int16_t(const int16_t& value, std::string* buffer) {
  VLOG(2) << __func__;
  int16_t value_net = value;
  switch (sizeof(int16_t)) {
    case 2:
      value_net = base::HostToNet16(value);
      break;
    case 4:
      value_net = base::HostToNet32(value);
      break;
    case 8:
      value_net = base::HostToNet64(value);
      break;
    default:
      break;
  }
  const char* value_bytes = reinterpret_cast<const char*>(&value_net);
  buffer->append(value_bytes, sizeof(int16_t));
  return TPM_RC_SUCCESS;
}

TPM_RC Parse_int16_t(
    std::string* buffer,
    int16_t* value,
    std::string* value_bytes) {
  VLOG(2) << __func__;
  if (buffer->size() < sizeof(int16_t))
    return TPM_RC_INSUFFICIENT;
  int16_t value_net = 0;
  memcpy(&value_net, buffer->data(), sizeof(int16_t));
  switch (sizeof(int16_t)) {
    case 2:
      *value = base::NetToHost16(value_net);
      break;
    case 4:
      *value = base::NetToHost32(value_net);
      break;
    case 8:
      *value = base::NetToHost64(value_net);
      break;
    default:
      *value = value_net;
  }
  if (value_bytes) {
    value_bytes->append(buffer->substr(0, sizeof(int16_t)));
  }
  buffer->erase(0, sizeof(int16_t));
  return TPM_RC_SUCCESS;
}

TPM_RC Serialize_uint32_t(const uint32_t& value, std::string* buffer) {
  VLOG(2) << __func__;
  uint32_t value_net = value;
  switch (sizeof(uint32_t)) {
    case 2:
      value_net = base::HostToNet16(value);
      break;
    case 4:
      value_net = base::HostToNet32(value);
      break;
    case 8:
      value_net = base::HostToNet64(value);
      break;
    default:
      break;
  }
  const char* value_bytes = reinterpret_cast<const char*>(&value_net);
  buffer->append(value_bytes, sizeof(uint32_t));
  return TPM_RC_SUCCESS;
}

TPM_RC Parse_uint32_t(
    std::string* buffer,
    uint32_t* value,
    std::string* value_bytes) {
  VLOG(2) << __func__;
  if (buffer->size() < sizeof(uint32_t))
    return TPM_RC_INSUFFICIENT;
  uint32_t value_net = 0;
  memcpy(&value_net, buffer->data(), sizeof(uint32_t));
  switch (sizeof(uint32_t)) {
    case 2:
      *value = base::NetToHost16(value_net);
      break;
    case 4:
      *value = base::NetToHost32(value_net);
      break;
    case 8:
      *value = base::NetToHost64(value_net);
      break;
    default:
      *value = value_net;
  }
  if (value_bytes) {
    value_bytes->append(buffer->substr(0, sizeof(uint32_t)));
  }
  buffer->erase(0, sizeof(uint32_t));
  return TPM_RC_SUCCESS;
}

TPM_RC Serialize_int32_t(const int32_t& value, std::string* buffer) {
  VLOG(2) << __func__;
  int32_t value_net = value;
  switch (sizeof(int32_t)) {
    case 2:
      value_net = base::HostToNet16(value);
      break;
    case 4:
      value_net = base::HostToNet32(value);
      break;
    case 8:
      value_net = base::HostToNet64(value);
      break;
    default:
      break;
  }
  const char* value_bytes = reinterpret_cast<const char*>(&value_net);
  buffer->append(value_bytes, sizeof(int32_t));
  return TPM_RC_SUCCESS;
}

TPM_RC Parse_int32_t(
    std::string* buffer,
    int32_t* value,
    std::string* value_bytes) {
  VLOG(2) << __func__;
  if (buffer->size() < sizeof(int32_t))
    return TPM_RC_INSUFFICIENT;
  int32_t value_net = 0;
  memcpy(&value_net, buffer->data(), sizeof(int32_t));
  switch (sizeof(int32_t)) {
    case 2:
      *value = base::NetToHost16(value_net);
      break;
    case 4:
      *value = base::NetToHost32(value_net);
      break;
    case 8:
      *value = base::NetToHost64(value_net);
      break;
    default:
      *value = value_net;
  }
  if (value_bytes) {
    value_bytes->append(buffer->substr(0, sizeof(int32_t)));
  }
  buffer->erase(0, sizeof(int32_t));
  return TPM_RC_SUCCESS;
}

TPM_RC Serialize_uint64_t(const uint64_t& value, std::string* buffer) {
  VLOG(2) << __func__;
  uint64_t value_net = value;
  switch (sizeof(uint64_t)) {
    case 2:
      value_net = base::HostToNet16(value);
      break;
    case 4:
      value_net = base::HostToNet32(value);
      break;
    case 8:
      value_net = base::HostToNet64(value);
      break;
    default:
      break;
  }
  const char* value_bytes = reinterpret_cast<const char*>(&value_net);
  buffer->append(value_bytes, sizeof(uint64_t));
  return TPM_RC_SUCCESS;
}

TPM_RC Parse_uint64_t(
    std::string* buffer,
    uint64_t* value,
    std::string* value_bytes) {
  VLOG(2) << __func__;
  if (buffer->size() < sizeof(uint64_t))
    return TPM_RC_INSUFFICIENT;
  uint64_t value_net = 0;
  memcpy(&value_net, buffer->data(), sizeof(uint64_t));
  switch (sizeof(uint64_t)) {
    case 2:
      *value = base::NetToHost16(value_net);
      break;
    case 4:
      *value = base::NetToHost32(value_net);
      break;
    case 8:
      *value = base::NetToHost64(value_net);
      break;
    default:
      *value = value_net;
  }
  if (value_bytes) {
    value_bytes->append(buffer->substr(0, sizeof(uint64_t)));
  }
  buffer->erase(0, sizeof(uint64_t));
  return TPM_RC_SUCCESS;
}

TPM_RC Serialize_int64_t(const int64_t& value, std::string* buffer) {
  VLOG(2) << __func__;
  int64_t value_net = value;
  switch (sizeof(int64_t)) {
    case 2:
      value_net = base::HostToNet16(value);
      break;
    case 4:
      value_net = base::HostToNet32(value);
      break;
    case 8:
      value_net = base::HostToNet64(value);
      break;
    default:
      break;
  }
  const char* value_bytes = reinterpret_cast<const char*>(&value_net);
  buffer->append(value_bytes, sizeof(int64_t));
  return TPM_RC_SUCCESS;
}

TPM_RC Parse_int64_t(
    std::string* buffer,
    int64_t* value,
    std::string* value_bytes) {
  VLOG(2) << __func__;
  if (buffer->size() < sizeof(int64_t))
    return TPM_RC_INSUFFICIENT;
  int64_t value_net = 0;
  memcpy(&value_net, buffer->data(), sizeof(int64_t));
  switch (sizeof(int64_t)) {
    case 2:
      *value = base::NetToHost16(value_net);
      break;
    case 4:
      *value = base::NetToHost32(value_net);
      break;
    case 8:
      *value = base::NetToHost64(value_net);
      break;
    default:
      *value = value_net;
  }
  if (value_bytes) {
    value_bytes->append(buffer->substr(0, sizeof(int64_t)));
  }
  buffer->erase(0, sizeof(int64_t));
  return TPM_RC_SUCCESS;
}

TPM_RC Serialize_UINT8(
    const UINT8& value,
    std::string* buffer) {
  VLOG(2) << __func__;
  return Serialize_uint8_t(value, buffer);
}

TPM_RC Parse_UINT8(
    std::string* buffer,
    UINT8* value,
    std::string* value_bytes) {
  VLOG(2) << __func__;
  return Parse_uint8_t(buffer, value, value_bytes);
}

TPM_RC Serialize_BYTE(
    const BYTE& value,
    std::string* buffer) {
  VLOG(2) << __func__;
  return Serialize_uint8_t(value, buffer);
}

TPM_RC Parse_BYTE(
    std::string* buffer,
    BYTE* value,
    std::string* value_bytes) {
  VLOG(2) << __func__;
  return Parse_uint8_t(buffer, value, value_bytes);
}

TPM_RC Serialize_INT8(
    const INT8& value,
    std::string* buffer) {
  VLOG(2) << __func__;
  return Serialize_int8_t(value, buffer);
}

TPM_RC Parse_INT8(
    std::string* buffer,
    INT8* value,
    std::string* value_bytes) {
  VLOG(2) << __func__;
  return Parse_int8_t(buffer, value, value_bytes);
}

TPM_RC Serialize_BOOL(
    const BOOL& value,
    std::string* buffer) {
  VLOG(2) << __func__;
  return Serialize_int(value, buffer);
}

TPM_RC Parse_BOOL(
    std::string* buffer,
    BOOL* value,
    std::string* value_bytes) {
  VLOG(2) << __func__;
  return Parse_int(buffer, value, value_bytes);
}

TPM_RC Serialize_UINT16(
    const UINT16& value,
    std::string* buffer) {
  VLOG(2) << __func__;
  return Serialize_uint16_t(value, buffer);
}

TPM_RC Parse_UINT16(
    std::string* buffer,
    UINT16* value,
    std::string* value_bytes) {
  VLOG(2) << __func__;
  return Parse_uint16_t(buffer, value, value_bytes);
}

TPM_RC Serialize_INT16(
    const INT16& value,
    std::string* buffer) {
  VLOG(2) << __func__;
  return Serialize_int16_t(value, buffer);
}

TPM_RC Parse_INT16(
    std::string* buffer,
    INT16* value,
    std::string* value_bytes) {
  VLOG(2) << __func__;
  return Parse_int16_t(buffer, value, value_bytes);
}

TPM_RC Serialize_UINT32(
    const UINT32& value,
    std::string* buffer) {
  VLOG(2) << __func__;
  return Serialize_uint32_t(value, buffer);
}

TPM_RC Parse_UINT32(
    std::string* buffer,
    UINT32* value,
    std::string* value_bytes) {
  VLOG(2) << __func__;
  return Parse_uint32_t(buffer, value, value_bytes);
}

TPM_RC Serialize_INT32(
    const INT32& value,
    std::string* buffer) {
  VLOG(2) << __func__;
  return Serialize_int32_t(value, buffer);
}

TPM_RC Parse_INT32(
    std::string* buffer,
    INT32* value,
    std::string* value_bytes) {
  VLOG(2) << __func__;
  return Parse_int32_t(buffer, value, value_bytes);
}

TPM_RC Serialize_UINT64(
    const UINT64& value,
    std::string* buffer) {
  VLOG(2) << __func__;
  return Serialize_uint64_t(value, buffer);
}

TPM_RC Parse_UINT64(
    std::string* buffer,
    UINT64* value,
    std::string* value_bytes) {
  VLOG(2) << __func__;
  return Parse_uint64_t(buffer, value, value_bytes);
}

TPM_RC Serialize_INT64(
    const INT64& value,
    std::string* buffer) {
  VLOG(2) << __func__;
  return Serialize_int64_t(value, buffer);
}

TPM_RC Parse_INT64(
    std::string* buffer,
    INT64* value,
    std::string* value_bytes) {
  VLOG(2) << __func__;
  return Parse_int64_t(buffer, value, value_bytes);
}

TPM_RC Serialize_TPM_ALGORITHM_ID(
    const TPM_ALGORITHM_ID& value,
    std::string* buffer) {
  VLOG(2) << __func__;
  return Serialize_UINT32(value, buffer);
}

TPM_RC Parse_TPM_ALGORITHM_ID(
    std::string* buffer,
    TPM_ALGORITHM_ID* value,
    std::string* value_bytes) {
  VLOG(2) << __func__;
  return Parse_UINT32(buffer, value, value_bytes);
}

TPM_RC Serialize_TPM_MODIFIER_INDICATOR(
    const TPM_MODIFIER_INDICATOR& value,
    std::string* buffer) {
  VLOG(2) << __func__;
  return Serialize_UINT32(value, buffer);
}

TPM_RC Parse_TPM_MODIFIER_INDICATOR(
    std::string* buffer,
    TPM_MODIFIER_INDICATOR* value,
    std::string* value_bytes) {
  VLOG(2) << __func__;
  return Parse_UINT32(buffer, value, value_bytes);
}

TPM_RC Serialize_TPM_AUTHORIZATION_SIZE(
    const TPM_AUTHORIZATION_SIZE& value,
    std::string* buffer) {
  VLOG(2) << __func__;
  return Serialize_UINT32(value, buffer);
}

TPM_RC Parse_TPM_AUTHORIZATION_SIZE(
    std::string* buffer,
    TPM_AUTHORIZATION_SIZE* value,
    std::string* value_bytes) {
  VLOG(2) << __func__;
  return Parse_UINT32(buffer, value, value_bytes);
}

TPM_RC Serialize_TPM_PARAMETER_SIZE(
    const TPM_PARAMETER_SIZE& value,
    std::string* buffer) {
  VLOG(2) << __func__;
  return Serialize_UINT32(value, buffer);
}

TPM_RC Parse_TPM_PARAMETER_SIZE(
    std::string* buffer,
    TPM_PARAMETER_SIZE* value,
    std::string* value_bytes) {
  VLOG(2) << __func__;
  return Parse_UINT32(buffer, value, value_bytes);
}

TPM_RC Serialize_TPM_KEY_SIZE(
    const TPM_KEY_SIZE& value,
    std::string* buffer) {
  VLOG(2) << __func__;
  return Serialize_UINT16(value, buffer);
}

TPM_RC Parse_TPM_KEY_SIZE(
    std::string* buffer,
    TPM_KEY_SIZE* value,
    std::string* value_bytes) {
  VLOG(2) << __func__;
  return Parse_UINT16(buffer, value, value_bytes);
}

TPM_RC Serialize_TPM_KEY_BITS(
    const TPM_KEY_BITS& value,
    std::string* buffer) {
  VLOG(2) << __func__;
  return Serialize_UINT16(value, buffer);
}

TPM_RC Parse_TPM_KEY_BITS(
    std::string* buffer,
    TPM_KEY_BITS* value,
    std::string* value_bytes) {
  VLOG(2) << __func__;
  return Parse_UINT16(buffer, value, value_bytes);
}

TPM_RC Serialize_TPM_HANDLE(
    const TPM_HANDLE& value,
    std::string* buffer) {
  VLOG(2) << __func__;
  return Serialize_UINT32(value, buffer);
}

TPM_RC Parse_TPM_HANDLE(
    std::string* buffer,
    TPM_HANDLE* value,
    std::string* value_bytes) {
  VLOG(2) << __func__;
  return Parse_UINT32(buffer, value, value_bytes);
}

TPM_RC Serialize_TPM2B_DIGEST(
    const TPM2B_DIGEST& value,
    std::string* buffer) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Serialize_UINT16(value.size, buffer);
  if (result) {
    return result;
  }

  if (arraysize(value.buffer) < value.size) {
    return TPM_RC_INSUFFICIENT;
  }
  for (uint32_t i = 0; i < value.size; ++i) {
    result = Serialize_BYTE(value.buffer[i], buffer);
    if (result) {
      return result;
    }
  }
  return result;
}

TPM_RC Parse_TPM2B_DIGEST(
    std::string* buffer,
    TPM2B_DIGEST* value,
    std::string* value_bytes) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Parse_UINT16(
      buffer,
      &value->size,
      value_bytes);
  if (result) {
    return result;
  }

  if (arraysize(value->buffer) < value->size) {
    return TPM_RC_INSUFFICIENT;
  }
  for (uint32_t i = 0; i < value->size; ++i) {
    result = Parse_BYTE(
        buffer,
        &value->buffer[i],
        value_bytes);
    if (result) {
      return result;
    }
  }
  return result;
}

TPM2B_DIGEST Make_TPM2B_DIGEST(
    const std::string& bytes) {
  TPM2B_DIGEST tpm2b;
  CHECK(bytes.size() <= sizeof(tpm2b.buffer));
  memset(&tpm2b, 0, sizeof(TPM2B_DIGEST));
  tpm2b.size = bytes.size();
  memcpy(tpm2b.buffer, bytes.data(), bytes.size());
  return tpm2b;
}

std::string StringFrom_TPM2B_DIGEST(
    const TPM2B_DIGEST& tpm2b) {
  const char* char_buffer = reinterpret_cast<const char*>(
      tpm2b.buffer);
  return std::string(char_buffer, tpm2b.size);
}

TPM_RC Serialize_TPM2B_NONCE(
    const TPM2B_NONCE& value,
    std::string* buffer) {
  VLOG(2) << __func__;
  return Serialize_TPM2B_DIGEST(value, buffer);
}

TPM_RC Parse_TPM2B_NONCE(
    std::string* buffer,
    TPM2B_NONCE* value,
    std::string* value_bytes) {
  VLOG(2) << __func__;
  return Parse_TPM2B_DIGEST(buffer, value, value_bytes);
}

TPM_RC Serialize_TPM2B_AUTH(
    const TPM2B_AUTH& value,
    std::string* buffer) {
  VLOG(2) << __func__;
  return Serialize_TPM2B_DIGEST(value, buffer);
}

TPM_RC Parse_TPM2B_AUTH(
    std::string* buffer,
    TPM2B_AUTH* value,
    std::string* value_bytes) {
  VLOG(2) << __func__;
  return Parse_TPM2B_DIGEST(buffer, value, value_bytes);
}

TPM_RC Serialize_TPM2B_OPERAND(
    const TPM2B_OPERAND& value,
    std::string* buffer) {
  VLOG(2) << __func__;
  return Serialize_TPM2B_DIGEST(value, buffer);
}

TPM_RC Parse_TPM2B_OPERAND(
    std::string* buffer,
    TPM2B_OPERAND* value,
    std::string* value_bytes) {
  VLOG(2) << __func__;
  return Parse_TPM2B_DIGEST(buffer, value, value_bytes);
}

TPM_RC Serialize_TPM_ALG_ID(
    const TPM_ALG_ID& value,
    std::string* buffer) {
  VLOG(2) << __func__;
  return Serialize_UINT16(value, buffer);
}

TPM_RC Parse_TPM_ALG_ID(
    std::string* buffer,
    TPM_ALG_ID* value,
    std::string* value_bytes) {
  VLOG(2) << __func__;
  return Parse_UINT16(buffer, value, value_bytes);
}

TPM_RC Serialize_TPMI_ALG_HASH(
    const TPMI_ALG_HASH& value,
    std::string* buffer) {
  VLOG(2) << __func__;
  return Serialize_TPM_ALG_ID(value, buffer);
}

TPM_RC Parse_TPMI_ALG_HASH(
    std::string* buffer,
    TPMI_ALG_HASH* value,
    std::string* value_bytes) {
  VLOG(2) << __func__;
  return Parse_TPM_ALG_ID(buffer, value, value_bytes);
}

TPM_RC Serialize_TPMS_SCHEME_SIGHASH(
    const TPMS_SCHEME_SIGHASH& value,
    std::string* buffer) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Serialize_TPMI_ALG_HASH(value.hash_alg, buffer);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Parse_TPMS_SCHEME_SIGHASH(
    std::string* buffer,
    TPMS_SCHEME_SIGHASH* value,
    std::string* value_bytes) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Parse_TPMI_ALG_HASH(
      buffer,
      &value->hash_alg,
      value_bytes);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Serialize_TPMS_SCHEME_HMAC(
    const TPMS_SCHEME_HMAC& value,
    std::string* buffer) {
  VLOG(2) << __func__;
  return Serialize_TPMS_SCHEME_SIGHASH(value, buffer);
}

TPM_RC Parse_TPMS_SCHEME_HMAC(
    std::string* buffer,
    TPMS_SCHEME_HMAC* value,
    std::string* value_bytes) {
  VLOG(2) << __func__;
  return Parse_TPMS_SCHEME_SIGHASH(buffer, value, value_bytes);
}

TPM_RC Serialize_TPMS_SCHEME_RSASSA(
    const TPMS_SCHEME_RSASSA& value,
    std::string* buffer) {
  VLOG(2) << __func__;
  return Serialize_TPMS_SCHEME_SIGHASH(value, buffer);
}

TPM_RC Parse_TPMS_SCHEME_RSASSA(
    std::string* buffer,
    TPMS_SCHEME_RSASSA* value,
    std::string* value_bytes) {
  VLOG(2) << __func__;
  return Parse_TPMS_SCHEME_SIGHASH(buffer, value, value_bytes);
}

TPM_RC Serialize_TPMS_SCHEME_RSAPSS(
    const TPMS_SCHEME_RSAPSS& value,
    std::string* buffer) {
  VLOG(2) << __func__;
  return Serialize_TPMS_SCHEME_SIGHASH(value, buffer);
}

TPM_RC Parse_TPMS_SCHEME_RSAPSS(
    std::string* buffer,
    TPMS_SCHEME_RSAPSS* value,
    std::string* value_bytes) {
  VLOG(2) << __func__;
  return Parse_TPMS_SCHEME_SIGHASH(buffer, value, value_bytes);
}

TPM_RC Serialize_TPMS_SCHEME_ECDSA(
    const TPMS_SCHEME_ECDSA& value,
    std::string* buffer) {
  VLOG(2) << __func__;
  return Serialize_TPMS_SCHEME_SIGHASH(value, buffer);
}

TPM_RC Parse_TPMS_SCHEME_ECDSA(
    std::string* buffer,
    TPMS_SCHEME_ECDSA* value,
    std::string* value_bytes) {
  VLOG(2) << __func__;
  return Parse_TPMS_SCHEME_SIGHASH(buffer, value, value_bytes);
}

TPM_RC Serialize_TPMS_SCHEME_SM2(
    const TPMS_SCHEME_SM2& value,
    std::string* buffer) {
  VLOG(2) << __func__;
  return Serialize_TPMS_SCHEME_SIGHASH(value, buffer);
}

TPM_RC Parse_TPMS_SCHEME_SM2(
    std::string* buffer,
    TPMS_SCHEME_SM2* value,
    std::string* value_bytes) {
  VLOG(2) << __func__;
  return Parse_TPMS_SCHEME_SIGHASH(buffer, value, value_bytes);
}

TPM_RC Serialize_TPMS_SCHEME_ECSCHNORR(
    const TPMS_SCHEME_ECSCHNORR& value,
    std::string* buffer) {
  VLOG(2) << __func__;
  return Serialize_TPMS_SCHEME_SIGHASH(value, buffer);
}

TPM_RC Parse_TPMS_SCHEME_ECSCHNORR(
    std::string* buffer,
    TPMS_SCHEME_ECSCHNORR* value,
    std::string* value_bytes) {
  VLOG(2) << __func__;
  return Parse_TPMS_SCHEME_SIGHASH(buffer, value, value_bytes);
}

TPM_RC Serialize_TPMI_YES_NO(
    const TPMI_YES_NO& value,
    std::string* buffer) {
  VLOG(2) << __func__;
  return Serialize_BYTE(value, buffer);
}

TPM_RC Parse_TPMI_YES_NO(
    std::string* buffer,
    TPMI_YES_NO* value,
    std::string* value_bytes) {
  VLOG(2) << __func__;
  return Parse_BYTE(buffer, value, value_bytes);
}

TPM_RC Serialize_TPMI_DH_OBJECT(
    const TPMI_DH_OBJECT& value,
    std::string* buffer) {
  VLOG(2) << __func__;
  return Serialize_TPM_HANDLE(value, buffer);
}

TPM_RC Parse_TPMI_DH_OBJECT(
    std::string* buffer,
    TPMI_DH_OBJECT* value,
    std::string* value_bytes) {
  VLOG(2) << __func__;
  return Parse_TPM_HANDLE(buffer, value, value_bytes);
}

TPM_RC Serialize_TPMI_DH_PERSISTENT(
    const TPMI_DH_PERSISTENT& value,
    std::string* buffer) {
  VLOG(2) << __func__;
  return Serialize_TPM_HANDLE(value, buffer);
}

TPM_RC Parse_TPMI_DH_PERSISTENT(
    std::string* buffer,
    TPMI_DH_PERSISTENT* value,
    std::string* value_bytes) {
  VLOG(2) << __func__;
  return Parse_TPM_HANDLE(buffer, value, value_bytes);
}

TPM_RC Serialize_TPMI_DH_ENTITY(
    const TPMI_DH_ENTITY& value,
    std::string* buffer) {
  VLOG(2) << __func__;
  return Serialize_TPM_HANDLE(value, buffer);
}

TPM_RC Parse_TPMI_DH_ENTITY(
    std::string* buffer,
    TPMI_DH_ENTITY* value,
    std::string* value_bytes) {
  VLOG(2) << __func__;
  return Parse_TPM_HANDLE(buffer, value, value_bytes);
}

TPM_RC Serialize_TPMI_DH_PCR(
    const TPMI_DH_PCR& value,
    std::string* buffer) {
  VLOG(2) << __func__;
  return Serialize_TPM_HANDLE(value, buffer);
}

TPM_RC Parse_TPMI_DH_PCR(
    std::string* buffer,
    TPMI_DH_PCR* value,
    std::string* value_bytes) {
  VLOG(2) << __func__;
  return Parse_TPM_HANDLE(buffer, value, value_bytes);
}

TPM_RC Serialize_TPMI_SH_AUTH_SESSION(
    const TPMI_SH_AUTH_SESSION& value,
    std::string* buffer) {
  VLOG(2) << __func__;
  return Serialize_TPM_HANDLE(value, buffer);
}

TPM_RC Parse_TPMI_SH_AUTH_SESSION(
    std::string* buffer,
    TPMI_SH_AUTH_SESSION* value,
    std::string* value_bytes) {
  VLOG(2) << __func__;
  return Parse_TPM_HANDLE(buffer, value, value_bytes);
}

TPM_RC Serialize_TPMI_SH_HMAC(
    const TPMI_SH_HMAC& value,
    std::string* buffer) {
  VLOG(2) << __func__;
  return Serialize_TPM_HANDLE(value, buffer);
}

TPM_RC Parse_TPMI_SH_HMAC(
    std::string* buffer,
    TPMI_SH_HMAC* value,
    std::string* value_bytes) {
  VLOG(2) << __func__;
  return Parse_TPM_HANDLE(buffer, value, value_bytes);
}

TPM_RC Serialize_TPMI_SH_POLICY(
    const TPMI_SH_POLICY& value,
    std::string* buffer) {
  VLOG(2) << __func__;
  return Serialize_TPM_HANDLE(value, buffer);
}

TPM_RC Parse_TPMI_SH_POLICY(
    std::string* buffer,
    TPMI_SH_POLICY* value,
    std::string* value_bytes) {
  VLOG(2) << __func__;
  return Parse_TPM_HANDLE(buffer, value, value_bytes);
}

TPM_RC Serialize_TPMI_DH_CONTEXT(
    const TPMI_DH_CONTEXT& value,
    std::string* buffer) {
  VLOG(2) << __func__;
  return Serialize_TPM_HANDLE(value, buffer);
}

TPM_RC Parse_TPMI_DH_CONTEXT(
    std::string* buffer,
    TPMI_DH_CONTEXT* value,
    std::string* value_bytes) {
  VLOG(2) << __func__;
  return Parse_TPM_HANDLE(buffer, value, value_bytes);
}

TPM_RC Serialize_TPMI_RH_HIERARCHY(
    const TPMI_RH_HIERARCHY& value,
    std::string* buffer) {
  VLOG(2) << __func__;
  return Serialize_TPM_HANDLE(value, buffer);
}

TPM_RC Parse_TPMI_RH_HIERARCHY(
    std::string* buffer,
    TPMI_RH_HIERARCHY* value,
    std::string* value_bytes) {
  VLOG(2) << __func__;
  return Parse_TPM_HANDLE(buffer, value, value_bytes);
}

TPM_RC Serialize_TPMI_RH_ENABLES(
    const TPMI_RH_ENABLES& value,
    std::string* buffer) {
  VLOG(2) << __func__;
  return Serialize_TPM_HANDLE(value, buffer);
}

TPM_RC Parse_TPMI_RH_ENABLES(
    std::string* buffer,
    TPMI_RH_ENABLES* value,
    std::string* value_bytes) {
  VLOG(2) << __func__;
  return Parse_TPM_HANDLE(buffer, value, value_bytes);
}

TPM_RC Serialize_TPMI_RH_HIERARCHY_AUTH(
    const TPMI_RH_HIERARCHY_AUTH& value,
    std::string* buffer) {
  VLOG(2) << __func__;
  return Serialize_TPM_HANDLE(value, buffer);
}

TPM_RC Parse_TPMI_RH_HIERARCHY_AUTH(
    std::string* buffer,
    TPMI_RH_HIERARCHY_AUTH* value,
    std::string* value_bytes) {
  VLOG(2) << __func__;
  return Parse_TPM_HANDLE(buffer, value, value_bytes);
}

TPM_RC Serialize_TPMI_RH_PLATFORM(
    const TPMI_RH_PLATFORM& value,
    std::string* buffer) {
  VLOG(2) << __func__;
  return Serialize_TPM_HANDLE(value, buffer);
}

TPM_RC Parse_TPMI_RH_PLATFORM(
    std::string* buffer,
    TPMI_RH_PLATFORM* value,
    std::string* value_bytes) {
  VLOG(2) << __func__;
  return Parse_TPM_HANDLE(buffer, value, value_bytes);
}

TPM_RC Serialize_TPMI_RH_OWNER(
    const TPMI_RH_OWNER& value,
    std::string* buffer) {
  VLOG(2) << __func__;
  return Serialize_TPM_HANDLE(value, buffer);
}

TPM_RC Parse_TPMI_RH_OWNER(
    std::string* buffer,
    TPMI_RH_OWNER* value,
    std::string* value_bytes) {
  VLOG(2) << __func__;
  return Parse_TPM_HANDLE(buffer, value, value_bytes);
}

TPM_RC Serialize_TPMI_RH_ENDORSEMENT(
    const TPMI_RH_ENDORSEMENT& value,
    std::string* buffer) {
  VLOG(2) << __func__;
  return Serialize_TPM_HANDLE(value, buffer);
}

TPM_RC Parse_TPMI_RH_ENDORSEMENT(
    std::string* buffer,
    TPMI_RH_ENDORSEMENT* value,
    std::string* value_bytes) {
  VLOG(2) << __func__;
  return Parse_TPM_HANDLE(buffer, value, value_bytes);
}

TPM_RC Serialize_TPMI_RH_PROVISION(
    const TPMI_RH_PROVISION& value,
    std::string* buffer) {
  VLOG(2) << __func__;
  return Serialize_TPM_HANDLE(value, buffer);
}

TPM_RC Parse_TPMI_RH_PROVISION(
    std::string* buffer,
    TPMI_RH_PROVISION* value,
    std::string* value_bytes) {
  VLOG(2) << __func__;
  return Parse_TPM_HANDLE(buffer, value, value_bytes);
}

TPM_RC Serialize_TPMI_RH_CLEAR(
    const TPMI_RH_CLEAR& value,
    std::string* buffer) {
  VLOG(2) << __func__;
  return Serialize_TPM_HANDLE(value, buffer);
}

TPM_RC Parse_TPMI_RH_CLEAR(
    std::string* buffer,
    TPMI_RH_CLEAR* value,
    std::string* value_bytes) {
  VLOG(2) << __func__;
  return Parse_TPM_HANDLE(buffer, value, value_bytes);
}

TPM_RC Serialize_TPMI_RH_NV_AUTH(
    const TPMI_RH_NV_AUTH& value,
    std::string* buffer) {
  VLOG(2) << __func__;
  return Serialize_TPM_HANDLE(value, buffer);
}

TPM_RC Parse_TPMI_RH_NV_AUTH(
    std::string* buffer,
    TPMI_RH_NV_AUTH* value,
    std::string* value_bytes) {
  VLOG(2) << __func__;
  return Parse_TPM_HANDLE(buffer, value, value_bytes);
}

TPM_RC Serialize_TPMI_RH_LOCKOUT(
    const TPMI_RH_LOCKOUT& value,
    std::string* buffer) {
  VLOG(2) << __func__;
  return Serialize_TPM_HANDLE(value, buffer);
}

TPM_RC Parse_TPMI_RH_LOCKOUT(
    std::string* buffer,
    TPMI_RH_LOCKOUT* value,
    std::string* value_bytes) {
  VLOG(2) << __func__;
  return Parse_TPM_HANDLE(buffer, value, value_bytes);
}

TPM_RC Serialize_TPMI_RH_NV_INDEX(
    const TPMI_RH_NV_INDEX& value,
    std::string* buffer) {
  VLOG(2) << __func__;
  return Serialize_TPM_HANDLE(value, buffer);
}

TPM_RC Parse_TPMI_RH_NV_INDEX(
    std::string* buffer,
    TPMI_RH_NV_INDEX* value,
    std::string* value_bytes) {
  VLOG(2) << __func__;
  return Parse_TPM_HANDLE(buffer, value, value_bytes);
}

TPM_RC Serialize_TPMI_ALG_ASYM(
    const TPMI_ALG_ASYM& value,
    std::string* buffer) {
  VLOG(2) << __func__;
  return Serialize_TPM_ALG_ID(value, buffer);
}

TPM_RC Parse_TPMI_ALG_ASYM(
    std::string* buffer,
    TPMI_ALG_ASYM* value,
    std::string* value_bytes) {
  VLOG(2) << __func__;
  return Parse_TPM_ALG_ID(buffer, value, value_bytes);
}

TPM_RC Serialize_TPMI_ALG_SYM(
    const TPMI_ALG_SYM& value,
    std::string* buffer) {
  VLOG(2) << __func__;
  return Serialize_TPM_ALG_ID(value, buffer);
}

TPM_RC Parse_TPMI_ALG_SYM(
    std::string* buffer,
    TPMI_ALG_SYM* value,
    std::string* value_bytes) {
  VLOG(2) << __func__;
  return Parse_TPM_ALG_ID(buffer, value, value_bytes);
}

TPM_RC Serialize_TPMI_ALG_SYM_OBJECT(
    const TPMI_ALG_SYM_OBJECT& value,
    std::string* buffer) {
  VLOG(2) << __func__;
  return Serialize_TPM_ALG_ID(value, buffer);
}

TPM_RC Parse_TPMI_ALG_SYM_OBJECT(
    std::string* buffer,
    TPMI_ALG_SYM_OBJECT* value,
    std::string* value_bytes) {
  VLOG(2) << __func__;
  return Parse_TPM_ALG_ID(buffer, value, value_bytes);
}

TPM_RC Serialize_TPMI_ALG_SYM_MODE(
    const TPMI_ALG_SYM_MODE& value,
    std::string* buffer) {
  VLOG(2) << __func__;
  return Serialize_TPM_ALG_ID(value, buffer);
}

TPM_RC Parse_TPMI_ALG_SYM_MODE(
    std::string* buffer,
    TPMI_ALG_SYM_MODE* value,
    std::string* value_bytes) {
  VLOG(2) << __func__;
  return Parse_TPM_ALG_ID(buffer, value, value_bytes);
}

TPM_RC Serialize_TPMI_ALG_KDF(
    const TPMI_ALG_KDF& value,
    std::string* buffer) {
  VLOG(2) << __func__;
  return Serialize_TPM_ALG_ID(value, buffer);
}

TPM_RC Parse_TPMI_ALG_KDF(
    std::string* buffer,
    TPMI_ALG_KDF* value,
    std::string* value_bytes) {
  VLOG(2) << __func__;
  return Parse_TPM_ALG_ID(buffer, value, value_bytes);
}

TPM_RC Serialize_TPMI_ALG_SIG_SCHEME(
    const TPMI_ALG_SIG_SCHEME& value,
    std::string* buffer) {
  VLOG(2) << __func__;
  return Serialize_TPM_ALG_ID(value, buffer);
}

TPM_RC Parse_TPMI_ALG_SIG_SCHEME(
    std::string* buffer,
    TPMI_ALG_SIG_SCHEME* value,
    std::string* value_bytes) {
  VLOG(2) << __func__;
  return Parse_TPM_ALG_ID(buffer, value, value_bytes);
}

TPM_RC Serialize_TPMI_ECC_KEY_EXCHANGE(
    const TPMI_ECC_KEY_EXCHANGE& value,
    std::string* buffer) {
  VLOG(2) << __func__;
  return Serialize_TPM_ALG_ID(value, buffer);
}

TPM_RC Parse_TPMI_ECC_KEY_EXCHANGE(
    std::string* buffer,
    TPMI_ECC_KEY_EXCHANGE* value,
    std::string* value_bytes) {
  VLOG(2) << __func__;
  return Parse_TPM_ALG_ID(buffer, value, value_bytes);
}

TPM_RC Serialize_TPM_ST(
    const TPM_ST& value,
    std::string* buffer) {
  VLOG(2) << __func__;
  return Serialize_UINT16(value, buffer);
}

TPM_RC Parse_TPM_ST(
    std::string* buffer,
    TPM_ST* value,
    std::string* value_bytes) {
  VLOG(2) << __func__;
  return Parse_UINT16(buffer, value, value_bytes);
}

TPM_RC Serialize_TPMI_ST_COMMAND_TAG(
    const TPMI_ST_COMMAND_TAG& value,
    std::string* buffer) {
  VLOG(2) << __func__;
  return Serialize_TPM_ST(value, buffer);
}

TPM_RC Parse_TPMI_ST_COMMAND_TAG(
    std::string* buffer,
    TPMI_ST_COMMAND_TAG* value,
    std::string* value_bytes) {
  VLOG(2) << __func__;
  return Parse_TPM_ST(buffer, value, value_bytes);
}

TPM_RC Serialize_TPMI_ST_ATTEST(
    const TPMI_ST_ATTEST& value,
    std::string* buffer) {
  VLOG(2) << __func__;
  return Serialize_TPM_ST(value, buffer);
}

TPM_RC Parse_TPMI_ST_ATTEST(
    std::string* buffer,
    TPMI_ST_ATTEST* value,
    std::string* value_bytes) {
  VLOG(2) << __func__;
  return Parse_TPM_ST(buffer, value, value_bytes);
}

TPM_RC Serialize_TPMI_AES_KEY_BITS(
    const TPMI_AES_KEY_BITS& value,
    std::string* buffer) {
  VLOG(2) << __func__;
  return Serialize_TPM_KEY_BITS(value, buffer);
}

TPM_RC Parse_TPMI_AES_KEY_BITS(
    std::string* buffer,
    TPMI_AES_KEY_BITS* value,
    std::string* value_bytes) {
  VLOG(2) << __func__;
  return Parse_TPM_KEY_BITS(buffer, value, value_bytes);
}

TPM_RC Serialize_TPMI_SM4_KEY_BITS(
    const TPMI_SM4_KEY_BITS& value,
    std::string* buffer) {
  VLOG(2) << __func__;
  return Serialize_TPM_KEY_BITS(value, buffer);
}

TPM_RC Parse_TPMI_SM4_KEY_BITS(
    std::string* buffer,
    TPMI_SM4_KEY_BITS* value,
    std::string* value_bytes) {
  VLOG(2) << __func__;
  return Parse_TPM_KEY_BITS(buffer, value, value_bytes);
}

TPM_RC Serialize_TPMI_ALG_KEYEDHASH_SCHEME(
    const TPMI_ALG_KEYEDHASH_SCHEME& value,
    std::string* buffer) {
  VLOG(2) << __func__;
  return Serialize_TPM_ALG_ID(value, buffer);
}

TPM_RC Parse_TPMI_ALG_KEYEDHASH_SCHEME(
    std::string* buffer,
    TPMI_ALG_KEYEDHASH_SCHEME* value,
    std::string* value_bytes) {
  VLOG(2) << __func__;
  return Parse_TPM_ALG_ID(buffer, value, value_bytes);
}

TPM_RC Serialize_TPMI_ALG_ASYM_SCHEME(
    const TPMI_ALG_ASYM_SCHEME& value,
    std::string* buffer) {
  VLOG(2) << __func__;
  return Serialize_TPM_ALG_ID(value, buffer);
}

TPM_RC Parse_TPMI_ALG_ASYM_SCHEME(
    std::string* buffer,
    TPMI_ALG_ASYM_SCHEME* value,
    std::string* value_bytes) {
  VLOG(2) << __func__;
  return Parse_TPM_ALG_ID(buffer, value, value_bytes);
}

TPM_RC Serialize_TPMI_ALG_RSA_SCHEME(
    const TPMI_ALG_RSA_SCHEME& value,
    std::string* buffer) {
  VLOG(2) << __func__;
  return Serialize_TPM_ALG_ID(value, buffer);
}

TPM_RC Parse_TPMI_ALG_RSA_SCHEME(
    std::string* buffer,
    TPMI_ALG_RSA_SCHEME* value,
    std::string* value_bytes) {
  VLOG(2) << __func__;
  return Parse_TPM_ALG_ID(buffer, value, value_bytes);
}

TPM_RC Serialize_TPMI_ALG_RSA_DECRYPT(
    const TPMI_ALG_RSA_DECRYPT& value,
    std::string* buffer) {
  VLOG(2) << __func__;
  return Serialize_TPM_ALG_ID(value, buffer);
}

TPM_RC Parse_TPMI_ALG_RSA_DECRYPT(
    std::string* buffer,
    TPMI_ALG_RSA_DECRYPT* value,
    std::string* value_bytes) {
  VLOG(2) << __func__;
  return Parse_TPM_ALG_ID(buffer, value, value_bytes);
}

TPM_RC Serialize_TPMI_RSA_KEY_BITS(
    const TPMI_RSA_KEY_BITS& value,
    std::string* buffer) {
  VLOG(2) << __func__;
  return Serialize_TPM_KEY_BITS(value, buffer);
}

TPM_RC Parse_TPMI_RSA_KEY_BITS(
    std::string* buffer,
    TPMI_RSA_KEY_BITS* value,
    std::string* value_bytes) {
  VLOG(2) << __func__;
  return Parse_TPM_KEY_BITS(buffer, value, value_bytes);
}

TPM_RC Serialize_TPMI_ALG_ECC_SCHEME(
    const TPMI_ALG_ECC_SCHEME& value,
    std::string* buffer) {
  VLOG(2) << __func__;
  return Serialize_TPM_ALG_ID(value, buffer);
}

TPM_RC Parse_TPMI_ALG_ECC_SCHEME(
    std::string* buffer,
    TPMI_ALG_ECC_SCHEME* value,
    std::string* value_bytes) {
  VLOG(2) << __func__;
  return Parse_TPM_ALG_ID(buffer, value, value_bytes);
}

TPM_RC Serialize_TPM_ECC_CURVE(
    const TPM_ECC_CURVE& value,
    std::string* buffer) {
  VLOG(2) << __func__;
  return Serialize_UINT16(value, buffer);
}

TPM_RC Parse_TPM_ECC_CURVE(
    std::string* buffer,
    TPM_ECC_CURVE* value,
    std::string* value_bytes) {
  VLOG(2) << __func__;
  return Parse_UINT16(buffer, value, value_bytes);
}

TPM_RC Serialize_TPMI_ECC_CURVE(
    const TPMI_ECC_CURVE& value,
    std::string* buffer) {
  VLOG(2) << __func__;
  return Serialize_TPM_ECC_CURVE(value, buffer);
}

TPM_RC Parse_TPMI_ECC_CURVE(
    std::string* buffer,
    TPMI_ECC_CURVE* value,
    std::string* value_bytes) {
  VLOG(2) << __func__;
  return Parse_TPM_ECC_CURVE(buffer, value, value_bytes);
}

TPM_RC Serialize_TPMI_ALG_PUBLIC(
    const TPMI_ALG_PUBLIC& value,
    std::string* buffer) {
  VLOG(2) << __func__;
  return Serialize_TPM_ALG_ID(value, buffer);
}

TPM_RC Parse_TPMI_ALG_PUBLIC(
    std::string* buffer,
    TPMI_ALG_PUBLIC* value,
    std::string* value_bytes) {
  VLOG(2) << __func__;
  return Parse_TPM_ALG_ID(buffer, value, value_bytes);
}

TPM_RC Serialize_TPMA_ALGORITHM(
    const TPMA_ALGORITHM& value,
    std::string* buffer) {
  VLOG(2) << __func__;
  return Serialize_UINT32(value, buffer);
}

TPM_RC Parse_TPMA_ALGORITHM(
    std::string* buffer,
    TPMA_ALGORITHM* value,
    std::string* value_bytes) {
  VLOG(2) << __func__;
  return Parse_UINT32(buffer, value, value_bytes);
}

TPM_RC Serialize_TPMA_OBJECT(
    const TPMA_OBJECT& value,
    std::string* buffer) {
  VLOG(2) << __func__;
  return Serialize_UINT32(value, buffer);
}

TPM_RC Parse_TPMA_OBJECT(
    std::string* buffer,
    TPMA_OBJECT* value,
    std::string* value_bytes) {
  VLOG(2) << __func__;
  return Parse_UINT32(buffer, value, value_bytes);
}

TPM_RC Serialize_TPMA_SESSION(
    const TPMA_SESSION& value,
    std::string* buffer) {
  VLOG(2) << __func__;
  return Serialize_UINT8(value, buffer);
}

TPM_RC Parse_TPMA_SESSION(
    std::string* buffer,
    TPMA_SESSION* value,
    std::string* value_bytes) {
  VLOG(2) << __func__;
  return Parse_UINT8(buffer, value, value_bytes);
}

TPM_RC Serialize_TPMA_LOCALITY(
    const TPMA_LOCALITY& value,
    std::string* buffer) {
  VLOG(2) << __func__;
  return Serialize_UINT8(value, buffer);
}

TPM_RC Parse_TPMA_LOCALITY(
    std::string* buffer,
    TPMA_LOCALITY* value,
    std::string* value_bytes) {
  VLOG(2) << __func__;
  return Parse_UINT8(buffer, value, value_bytes);
}

TPM_RC Serialize_TPMA_PERMANENT(
    const TPMA_PERMANENT& value,
    std::string* buffer) {
  VLOG(2) << __func__;
  return Serialize_UINT32(value, buffer);
}

TPM_RC Parse_TPMA_PERMANENT(
    std::string* buffer,
    TPMA_PERMANENT* value,
    std::string* value_bytes) {
  VLOG(2) << __func__;
  return Parse_UINT32(buffer, value, value_bytes);
}

TPM_RC Serialize_TPMA_STARTUP_CLEAR(
    const TPMA_STARTUP_CLEAR& value,
    std::string* buffer) {
  VLOG(2) << __func__;
  return Serialize_UINT32(value, buffer);
}

TPM_RC Parse_TPMA_STARTUP_CLEAR(
    std::string* buffer,
    TPMA_STARTUP_CLEAR* value,
    std::string* value_bytes) {
  VLOG(2) << __func__;
  return Parse_UINT32(buffer, value, value_bytes);
}

TPM_RC Serialize_TPMA_MEMORY(
    const TPMA_MEMORY& value,
    std::string* buffer) {
  VLOG(2) << __func__;
  return Serialize_UINT32(value, buffer);
}

TPM_RC Parse_TPMA_MEMORY(
    std::string* buffer,
    TPMA_MEMORY* value,
    std::string* value_bytes) {
  VLOG(2) << __func__;
  return Parse_UINT32(buffer, value, value_bytes);
}

TPM_RC Serialize_TPM_CC(
    const TPM_CC& value,
    std::string* buffer) {
  VLOG(2) << __func__;
  return Serialize_UINT32(value, buffer);
}

TPM_RC Parse_TPM_CC(
    std::string* buffer,
    TPM_CC* value,
    std::string* value_bytes) {
  VLOG(2) << __func__;
  return Parse_UINT32(buffer, value, value_bytes);
}

TPM_RC Serialize_TPMA_CC(
    const TPMA_CC& value,
    std::string* buffer) {
  VLOG(2) << __func__;
  return Serialize_TPM_CC(value, buffer);
}

TPM_RC Parse_TPMA_CC(
    std::string* buffer,
    TPMA_CC* value,
    std::string* value_bytes) {
  VLOG(2) << __func__;
  return Parse_TPM_CC(buffer, value, value_bytes);
}

TPM_RC Serialize_TPM_NV_INDEX(
    const TPM_NV_INDEX& value,
    std::string* buffer) {
  VLOG(2) << __func__;
  return Serialize_UINT32(value, buffer);
}

TPM_RC Parse_TPM_NV_INDEX(
    std::string* buffer,
    TPM_NV_INDEX* value,
    std::string* value_bytes) {
  VLOG(2) << __func__;
  return Parse_UINT32(buffer, value, value_bytes);
}

TPM_RC Serialize_TPMA_NV(
    const TPMA_NV& value,
    std::string* buffer) {
  VLOG(2) << __func__;
  return Serialize_UINT32(value, buffer);
}

TPM_RC Parse_TPMA_NV(
    std::string* buffer,
    TPMA_NV* value,
    std::string* value_bytes) {
  VLOG(2) << __func__;
  return Parse_UINT32(buffer, value, value_bytes);
}

TPM_RC Serialize_TPM_SPEC(
    const TPM_SPEC& value,
    std::string* buffer) {
  VLOG(2) << __func__;
  return Serialize_UINT32(value, buffer);
}

TPM_RC Parse_TPM_SPEC(
    std::string* buffer,
    TPM_SPEC* value,
    std::string* value_bytes) {
  VLOG(2) << __func__;
  return Parse_UINT32(buffer, value, value_bytes);
}

TPM_RC Serialize_TPM_GENERATED(
    const TPM_GENERATED& value,
    std::string* buffer) {
  VLOG(2) << __func__;
  return Serialize_UINT32(value, buffer);
}

TPM_RC Parse_TPM_GENERATED(
    std::string* buffer,
    TPM_GENERATED* value,
    std::string* value_bytes) {
  VLOG(2) << __func__;
  return Parse_UINT32(buffer, value, value_bytes);
}

TPM_RC Serialize_TPM_RC(
    const TPM_RC& value,
    std::string* buffer) {
  VLOG(2) << __func__;
  return Serialize_UINT32(value, buffer);
}

TPM_RC Parse_TPM_RC(
    std::string* buffer,
    TPM_RC* value,
    std::string* value_bytes) {
  VLOG(2) << __func__;
  return Parse_UINT32(buffer, value, value_bytes);
}

TPM_RC Serialize_TPM_CLOCK_ADJUST(
    const TPM_CLOCK_ADJUST& value,
    std::string* buffer) {
  VLOG(2) << __func__;
  return Serialize_INT8(value, buffer);
}

TPM_RC Parse_TPM_CLOCK_ADJUST(
    std::string* buffer,
    TPM_CLOCK_ADJUST* value,
    std::string* value_bytes) {
  VLOG(2) << __func__;
  return Parse_INT8(buffer, value, value_bytes);
}

TPM_RC Serialize_TPM_EO(
    const TPM_EO& value,
    std::string* buffer) {
  VLOG(2) << __func__;
  return Serialize_UINT16(value, buffer);
}

TPM_RC Parse_TPM_EO(
    std::string* buffer,
    TPM_EO* value,
    std::string* value_bytes) {
  VLOG(2) << __func__;
  return Parse_UINT16(buffer, value, value_bytes);
}

TPM_RC Serialize_TPM_SU(
    const TPM_SU& value,
    std::string* buffer) {
  VLOG(2) << __func__;
  return Serialize_UINT16(value, buffer);
}

TPM_RC Parse_TPM_SU(
    std::string* buffer,
    TPM_SU* value,
    std::string* value_bytes) {
  VLOG(2) << __func__;
  return Parse_UINT16(buffer, value, value_bytes);
}

TPM_RC Serialize_TPM_SE(
    const TPM_SE& value,
    std::string* buffer) {
  VLOG(2) << __func__;
  return Serialize_UINT8(value, buffer);
}

TPM_RC Parse_TPM_SE(
    std::string* buffer,
    TPM_SE* value,
    std::string* value_bytes) {
  VLOG(2) << __func__;
  return Parse_UINT8(buffer, value, value_bytes);
}

TPM_RC Serialize_TPM_CAP(
    const TPM_CAP& value,
    std::string* buffer) {
  VLOG(2) << __func__;
  return Serialize_UINT32(value, buffer);
}

TPM_RC Parse_TPM_CAP(
    std::string* buffer,
    TPM_CAP* value,
    std::string* value_bytes) {
  VLOG(2) << __func__;
  return Parse_UINT32(buffer, value, value_bytes);
}

TPM_RC Serialize_TPM_PT(
    const TPM_PT& value,
    std::string* buffer) {
  VLOG(2) << __func__;
  return Serialize_UINT32(value, buffer);
}

TPM_RC Parse_TPM_PT(
    std::string* buffer,
    TPM_PT* value,
    std::string* value_bytes) {
  VLOG(2) << __func__;
  return Parse_UINT32(buffer, value, value_bytes);
}

TPM_RC Serialize_TPM_PT_PCR(
    const TPM_PT_PCR& value,
    std::string* buffer) {
  VLOG(2) << __func__;
  return Serialize_UINT32(value, buffer);
}

TPM_RC Parse_TPM_PT_PCR(
    std::string* buffer,
    TPM_PT_PCR* value,
    std::string* value_bytes) {
  VLOG(2) << __func__;
  return Parse_UINT32(buffer, value, value_bytes);
}

TPM_RC Serialize_TPM_PS(
    const TPM_PS& value,
    std::string* buffer) {
  VLOG(2) << __func__;
  return Serialize_UINT32(value, buffer);
}

TPM_RC Parse_TPM_PS(
    std::string* buffer,
    TPM_PS* value,
    std::string* value_bytes) {
  VLOG(2) << __func__;
  return Parse_UINT32(buffer, value, value_bytes);
}

TPM_RC Serialize_TPM_HT(
    const TPM_HT& value,
    std::string* buffer) {
  VLOG(2) << __func__;
  return Serialize_UINT8(value, buffer);
}

TPM_RC Parse_TPM_HT(
    std::string* buffer,
    TPM_HT* value,
    std::string* value_bytes) {
  VLOG(2) << __func__;
  return Parse_UINT8(buffer, value, value_bytes);
}

TPM_RC Serialize_TPM_RH(
    const TPM_RH& value,
    std::string* buffer) {
  VLOG(2) << __func__;
  return Serialize_UINT32(value, buffer);
}

TPM_RC Parse_TPM_RH(
    std::string* buffer,
    TPM_RH* value,
    std::string* value_bytes) {
  VLOG(2) << __func__;
  return Parse_UINT32(buffer, value, value_bytes);
}

TPM_RC Serialize_TPM_HC(
    const TPM_HC& value,
    std::string* buffer) {
  VLOG(2) << __func__;
  return Serialize_TPM_HANDLE(value, buffer);
}

TPM_RC Parse_TPM_HC(
    std::string* buffer,
    TPM_HC* value,
    std::string* value_bytes) {
  VLOG(2) << __func__;
  return Parse_TPM_HANDLE(buffer, value, value_bytes);
}

TPM_RC Serialize_TPMS_ALGORITHM_DESCRIPTION(
    const TPMS_ALGORITHM_DESCRIPTION& value,
    std::string* buffer) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Serialize_TPM_ALG_ID(value.alg, buffer);
  if (result) {
    return result;
  }

  result = Serialize_TPMA_ALGORITHM(value.attributes, buffer);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Parse_TPMS_ALGORITHM_DESCRIPTION(
    std::string* buffer,
    TPMS_ALGORITHM_DESCRIPTION* value,
    std::string* value_bytes) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Parse_TPM_ALG_ID(
      buffer,
      &value->alg,
      value_bytes);
  if (result) {
    return result;
  }

  result = Parse_TPMA_ALGORITHM(
      buffer,
      &value->attributes,
      value_bytes);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Serialize_TPMU_HA(
    const TPMU_HA& value,
    TPMI_ALG_HASH selector,
    std::string* buffer) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  if (selector == TPM_ALG_SHA384) {
    if (arraysize(value.sha384) < SHA384_DIGEST_SIZE) {
      return TPM_RC_INSUFFICIENT;
    }
    for (uint32_t i = 0; i < SHA384_DIGEST_SIZE; ++i) {
      result = Serialize_BYTE(value.sha384[i], buffer);
      if (result) {
        return result;
      }
    }
  }

  if (selector == TPM_ALG_SHA1) {
    if (arraysize(value.sha1) < SHA1_DIGEST_SIZE) {
      return TPM_RC_INSUFFICIENT;
    }
    for (uint32_t i = 0; i < SHA1_DIGEST_SIZE; ++i) {
      result = Serialize_BYTE(value.sha1[i], buffer);
      if (result) {
        return result;
      }
    }
  }

  if (selector == TPM_ALG_SM3_256) {
    if (arraysize(value.sm3_256) < SM3_256_DIGEST_SIZE) {
      return TPM_RC_INSUFFICIENT;
    }
    for (uint32_t i = 0; i < SM3_256_DIGEST_SIZE; ++i) {
      result = Serialize_BYTE(value.sm3_256[i], buffer);
      if (result) {
        return result;
      }
    }
  }

  if (selector == TPM_ALG_NULL) {
    // Do nothing.
  }

  if (selector == TPM_ALG_SHA256) {
    if (arraysize(value.sha256) < SHA256_DIGEST_SIZE) {
      return TPM_RC_INSUFFICIENT;
    }
    for (uint32_t i = 0; i < SHA256_DIGEST_SIZE; ++i) {
      result = Serialize_BYTE(value.sha256[i], buffer);
      if (result) {
        return result;
      }
    }
  }

  if (selector == TPM_ALG_SHA512) {
    if (arraysize(value.sha512) < SHA512_DIGEST_SIZE) {
      return TPM_RC_INSUFFICIENT;
    }
    for (uint32_t i = 0; i < SHA512_DIGEST_SIZE; ++i) {
      result = Serialize_BYTE(value.sha512[i], buffer);
      if (result) {
        return result;
      }
    }
  }
  return result;
}

TPM_RC Parse_TPMU_HA(
    std::string* buffer,
    TPMI_ALG_HASH selector,
    TPMU_HA* value,
    std::string* value_bytes) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  if (selector == TPM_ALG_SHA384) {
    if (arraysize(value->sha384) < SHA384_DIGEST_SIZE) {
      return TPM_RC_INSUFFICIENT;
    }
    for (uint32_t i = 0; i < SHA384_DIGEST_SIZE; ++i) {
      result = Parse_BYTE(
          buffer,
          &value->sha384[i],
          value_bytes);
      if (result) {
        return result;
      }
    }
  }

  if (selector == TPM_ALG_SHA1) {
    if (arraysize(value->sha1) < SHA1_DIGEST_SIZE) {
      return TPM_RC_INSUFFICIENT;
    }
    for (uint32_t i = 0; i < SHA1_DIGEST_SIZE; ++i) {
      result = Parse_BYTE(
          buffer,
          &value->sha1[i],
          value_bytes);
      if (result) {
        return result;
      }
    }
  }

  if (selector == TPM_ALG_SM3_256) {
    if (arraysize(value->sm3_256) < SM3_256_DIGEST_SIZE) {
      return TPM_RC_INSUFFICIENT;
    }
    for (uint32_t i = 0; i < SM3_256_DIGEST_SIZE; ++i) {
      result = Parse_BYTE(
          buffer,
          &value->sm3_256[i],
          value_bytes);
      if (result) {
        return result;
      }
    }
  }

  if (selector == TPM_ALG_NULL) {
    // Do nothing.
  }

  if (selector == TPM_ALG_SHA256) {
    if (arraysize(value->sha256) < SHA256_DIGEST_SIZE) {
      return TPM_RC_INSUFFICIENT;
    }
    for (uint32_t i = 0; i < SHA256_DIGEST_SIZE; ++i) {
      result = Parse_BYTE(
          buffer,
          &value->sha256[i],
          value_bytes);
      if (result) {
        return result;
      }
    }
  }

  if (selector == TPM_ALG_SHA512) {
    if (arraysize(value->sha512) < SHA512_DIGEST_SIZE) {
      return TPM_RC_INSUFFICIENT;
    }
    for (uint32_t i = 0; i < SHA512_DIGEST_SIZE; ++i) {
      result = Parse_BYTE(
          buffer,
          &value->sha512[i],
          value_bytes);
      if (result) {
        return result;
      }
    }
  }
  return result;
}

TPM_RC Serialize_TPMT_HA(
    const TPMT_HA& value,
    std::string* buffer) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Serialize_TPMI_ALG_HASH(value.hash_alg, buffer);
  if (result) {
    return result;
  }

  result = Serialize_TPMU_HA(
      value.digest,
      value.hash_alg,
      buffer);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Parse_TPMT_HA(
    std::string* buffer,
    TPMT_HA* value,
    std::string* value_bytes) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Parse_TPMI_ALG_HASH(
      buffer,
      &value->hash_alg,
      value_bytes);
  if (result) {
    return result;
  }

  result = Parse_TPMU_HA(
      buffer,
      value->hash_alg,
      &value->digest,
      value_bytes);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Serialize_TPM2B_DATA(
    const TPM2B_DATA& value,
    std::string* buffer) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Serialize_UINT16(value.size, buffer);
  if (result) {
    return result;
  }

  if (arraysize(value.buffer) < value.size) {
    return TPM_RC_INSUFFICIENT;
  }
  for (uint32_t i = 0; i < value.size; ++i) {
    result = Serialize_BYTE(value.buffer[i], buffer);
    if (result) {
      return result;
    }
  }
  return result;
}

TPM_RC Parse_TPM2B_DATA(
    std::string* buffer,
    TPM2B_DATA* value,
    std::string* value_bytes) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Parse_UINT16(
      buffer,
      &value->size,
      value_bytes);
  if (result) {
    return result;
  }

  if (arraysize(value->buffer) < value->size) {
    return TPM_RC_INSUFFICIENT;
  }
  for (uint32_t i = 0; i < value->size; ++i) {
    result = Parse_BYTE(
        buffer,
        &value->buffer[i],
        value_bytes);
    if (result) {
      return result;
    }
  }
  return result;
}

TPM2B_DATA Make_TPM2B_DATA(
    const std::string& bytes) {
  TPM2B_DATA tpm2b;
  CHECK(bytes.size() <= sizeof(tpm2b.buffer));
  memset(&tpm2b, 0, sizeof(TPM2B_DATA));
  tpm2b.size = bytes.size();
  memcpy(tpm2b.buffer, bytes.data(), bytes.size());
  return tpm2b;
}

std::string StringFrom_TPM2B_DATA(
    const TPM2B_DATA& tpm2b) {
  const char* char_buffer = reinterpret_cast<const char*>(
      tpm2b.buffer);
  return std::string(char_buffer, tpm2b.size);
}

TPM_RC Serialize_TPM2B_EVENT(
    const TPM2B_EVENT& value,
    std::string* buffer) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Serialize_UINT16(value.size, buffer);
  if (result) {
    return result;
  }

  if (arraysize(value.buffer) < value.size) {
    return TPM_RC_INSUFFICIENT;
  }
  for (uint32_t i = 0; i < value.size; ++i) {
    result = Serialize_BYTE(value.buffer[i], buffer);
    if (result) {
      return result;
    }
  }
  return result;
}

TPM_RC Parse_TPM2B_EVENT(
    std::string* buffer,
    TPM2B_EVENT* value,
    std::string* value_bytes) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Parse_UINT16(
      buffer,
      &value->size,
      value_bytes);
  if (result) {
    return result;
  }

  if (arraysize(value->buffer) < value->size) {
    return TPM_RC_INSUFFICIENT;
  }
  for (uint32_t i = 0; i < value->size; ++i) {
    result = Parse_BYTE(
        buffer,
        &value->buffer[i],
        value_bytes);
    if (result) {
      return result;
    }
  }
  return result;
}

TPM2B_EVENT Make_TPM2B_EVENT(
    const std::string& bytes) {
  TPM2B_EVENT tpm2b;
  CHECK(bytes.size() <= sizeof(tpm2b.buffer));
  memset(&tpm2b, 0, sizeof(TPM2B_EVENT));
  tpm2b.size = bytes.size();
  memcpy(tpm2b.buffer, bytes.data(), bytes.size());
  return tpm2b;
}

std::string StringFrom_TPM2B_EVENT(
    const TPM2B_EVENT& tpm2b) {
  const char* char_buffer = reinterpret_cast<const char*>(
      tpm2b.buffer);
  return std::string(char_buffer, tpm2b.size);
}

TPM_RC Serialize_TPM2B_MAX_BUFFER(
    const TPM2B_MAX_BUFFER& value,
    std::string* buffer) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Serialize_UINT16(value.size, buffer);
  if (result) {
    return result;
  }

  if (arraysize(value.buffer) < value.size) {
    return TPM_RC_INSUFFICIENT;
  }
  for (uint32_t i = 0; i < value.size; ++i) {
    result = Serialize_BYTE(value.buffer[i], buffer);
    if (result) {
      return result;
    }
  }
  return result;
}

TPM_RC Parse_TPM2B_MAX_BUFFER(
    std::string* buffer,
    TPM2B_MAX_BUFFER* value,
    std::string* value_bytes) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Parse_UINT16(
      buffer,
      &value->size,
      value_bytes);
  if (result) {
    return result;
  }

  if (arraysize(value->buffer) < value->size) {
    return TPM_RC_INSUFFICIENT;
  }
  for (uint32_t i = 0; i < value->size; ++i) {
    result = Parse_BYTE(
        buffer,
        &value->buffer[i],
        value_bytes);
    if (result) {
      return result;
    }
  }
  return result;
}

TPM2B_MAX_BUFFER Make_TPM2B_MAX_BUFFER(
    const std::string& bytes) {
  TPM2B_MAX_BUFFER tpm2b;
  CHECK(bytes.size() <= sizeof(tpm2b.buffer));
  memset(&tpm2b, 0, sizeof(TPM2B_MAX_BUFFER));
  tpm2b.size = bytes.size();
  memcpy(tpm2b.buffer, bytes.data(), bytes.size());
  return tpm2b;
}

std::string StringFrom_TPM2B_MAX_BUFFER(
    const TPM2B_MAX_BUFFER& tpm2b) {
  const char* char_buffer = reinterpret_cast<const char*>(
      tpm2b.buffer);
  return std::string(char_buffer, tpm2b.size);
}

TPM_RC Serialize_TPM2B_MAX_NV_BUFFER(
    const TPM2B_MAX_NV_BUFFER& value,
    std::string* buffer) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Serialize_UINT16(value.size, buffer);
  if (result) {
    return result;
  }

  if (arraysize(value.buffer) < value.size) {
    return TPM_RC_INSUFFICIENT;
  }
  for (uint32_t i = 0; i < value.size; ++i) {
    result = Serialize_BYTE(value.buffer[i], buffer);
    if (result) {
      return result;
    }
  }
  return result;
}

TPM_RC Parse_TPM2B_MAX_NV_BUFFER(
    std::string* buffer,
    TPM2B_MAX_NV_BUFFER* value,
    std::string* value_bytes) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Parse_UINT16(
      buffer,
      &value->size,
      value_bytes);
  if (result) {
    return result;
  }

  if (arraysize(value->buffer) < value->size) {
    return TPM_RC_INSUFFICIENT;
  }
  for (uint32_t i = 0; i < value->size; ++i) {
    result = Parse_BYTE(
        buffer,
        &value->buffer[i],
        value_bytes);
    if (result) {
      return result;
    }
  }
  return result;
}

TPM2B_MAX_NV_BUFFER Make_TPM2B_MAX_NV_BUFFER(
    const std::string& bytes) {
  TPM2B_MAX_NV_BUFFER tpm2b;
  CHECK(bytes.size() <= sizeof(tpm2b.buffer));
  memset(&tpm2b, 0, sizeof(TPM2B_MAX_NV_BUFFER));
  tpm2b.size = bytes.size();
  memcpy(tpm2b.buffer, bytes.data(), bytes.size());
  return tpm2b;
}

std::string StringFrom_TPM2B_MAX_NV_BUFFER(
    const TPM2B_MAX_NV_BUFFER& tpm2b) {
  const char* char_buffer = reinterpret_cast<const char*>(
      tpm2b.buffer);
  return std::string(char_buffer, tpm2b.size);
}

TPM_RC Serialize_TPM2B_TIMEOUT(
    const TPM2B_TIMEOUT& value,
    std::string* buffer) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Serialize_UINT16(value.size, buffer);
  if (result) {
    return result;
  }

  if (arraysize(value.buffer) < value.size) {
    return TPM_RC_INSUFFICIENT;
  }
  for (uint32_t i = 0; i < value.size; ++i) {
    result = Serialize_BYTE(value.buffer[i], buffer);
    if (result) {
      return result;
    }
  }
  return result;
}

TPM_RC Parse_TPM2B_TIMEOUT(
    std::string* buffer,
    TPM2B_TIMEOUT* value,
    std::string* value_bytes) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Parse_UINT16(
      buffer,
      &value->size,
      value_bytes);
  if (result) {
    return result;
  }

  if (arraysize(value->buffer) < value->size) {
    return TPM_RC_INSUFFICIENT;
  }
  for (uint32_t i = 0; i < value->size; ++i) {
    result = Parse_BYTE(
        buffer,
        &value->buffer[i],
        value_bytes);
    if (result) {
      return result;
    }
  }
  return result;
}

TPM2B_TIMEOUT Make_TPM2B_TIMEOUT(
    const std::string& bytes) {
  TPM2B_TIMEOUT tpm2b;
  CHECK(bytes.size() <= sizeof(tpm2b.buffer));
  memset(&tpm2b, 0, sizeof(TPM2B_TIMEOUT));
  tpm2b.size = bytes.size();
  memcpy(tpm2b.buffer, bytes.data(), bytes.size());
  return tpm2b;
}

std::string StringFrom_TPM2B_TIMEOUT(
    const TPM2B_TIMEOUT& tpm2b) {
  const char* char_buffer = reinterpret_cast<const char*>(
      tpm2b.buffer);
  return std::string(char_buffer, tpm2b.size);
}

TPM_RC Serialize_TPM2B_IV(
    const TPM2B_IV& value,
    std::string* buffer) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Serialize_UINT16(value.size, buffer);
  if (result) {
    return result;
  }

  if (arraysize(value.buffer) < value.size) {
    return TPM_RC_INSUFFICIENT;
  }
  for (uint32_t i = 0; i < value.size; ++i) {
    result = Serialize_BYTE(value.buffer[i], buffer);
    if (result) {
      return result;
    }
  }
  return result;
}

TPM_RC Parse_TPM2B_IV(
    std::string* buffer,
    TPM2B_IV* value,
    std::string* value_bytes) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Parse_UINT16(
      buffer,
      &value->size,
      value_bytes);
  if (result) {
    return result;
  }

  if (arraysize(value->buffer) < value->size) {
    return TPM_RC_INSUFFICIENT;
  }
  for (uint32_t i = 0; i < value->size; ++i) {
    result = Parse_BYTE(
        buffer,
        &value->buffer[i],
        value_bytes);
    if (result) {
      return result;
    }
  }
  return result;
}

TPM2B_IV Make_TPM2B_IV(
    const std::string& bytes) {
  TPM2B_IV tpm2b;
  CHECK(bytes.size() <= sizeof(tpm2b.buffer));
  memset(&tpm2b, 0, sizeof(TPM2B_IV));
  tpm2b.size = bytes.size();
  memcpy(tpm2b.buffer, bytes.data(), bytes.size());
  return tpm2b;
}

std::string StringFrom_TPM2B_IV(
    const TPM2B_IV& tpm2b) {
  const char* char_buffer = reinterpret_cast<const char*>(
      tpm2b.buffer);
  return std::string(char_buffer, tpm2b.size);
}

TPM_RC Serialize_TPM2B_NAME(
    const TPM2B_NAME& value,
    std::string* buffer) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Serialize_UINT16(value.size, buffer);
  if (result) {
    return result;
  }

  if (arraysize(value.name) < value.size) {
    return TPM_RC_INSUFFICIENT;
  }
  for (uint32_t i = 0; i < value.size; ++i) {
    result = Serialize_BYTE(value.name[i], buffer);
    if (result) {
      return result;
    }
  }
  return result;
}

TPM_RC Parse_TPM2B_NAME(
    std::string* buffer,
    TPM2B_NAME* value,
    std::string* value_bytes) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Parse_UINT16(
      buffer,
      &value->size,
      value_bytes);
  if (result) {
    return result;
  }

  if (arraysize(value->name) < value->size) {
    return TPM_RC_INSUFFICIENT;
  }
  for (uint32_t i = 0; i < value->size; ++i) {
    result = Parse_BYTE(
        buffer,
        &value->name[i],
        value_bytes);
    if (result) {
      return result;
    }
  }
  return result;
}

TPM2B_NAME Make_TPM2B_NAME(
    const std::string& bytes) {
  TPM2B_NAME tpm2b;
  CHECK(bytes.size() <= sizeof(tpm2b.name));
  memset(&tpm2b, 0, sizeof(TPM2B_NAME));
  tpm2b.size = bytes.size();
  memcpy(tpm2b.name, bytes.data(), bytes.size());
  return tpm2b;
}

std::string StringFrom_TPM2B_NAME(
    const TPM2B_NAME& tpm2b) {
  const char* char_buffer = reinterpret_cast<const char*>(
      tpm2b.name);
  return std::string(char_buffer, tpm2b.size);
}

TPM_RC Serialize_TPMS_PCR_SELECT(
    const TPMS_PCR_SELECT& value,
    std::string* buffer) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Serialize_UINT8(value.sizeof_select, buffer);
  if (result) {
    return result;
  }

  if (arraysize(value.pcr_select) < value.sizeof_select) {
    return TPM_RC_INSUFFICIENT;
  }
  for (uint32_t i = 0; i < value.sizeof_select; ++i) {
    result = Serialize_BYTE(value.pcr_select[i], buffer);
    if (result) {
      return result;
    }
  }
  return result;
}

TPM_RC Parse_TPMS_PCR_SELECT(
    std::string* buffer,
    TPMS_PCR_SELECT* value,
    std::string* value_bytes) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Parse_UINT8(
      buffer,
      &value->sizeof_select,
      value_bytes);
  if (result) {
    return result;
  }

  if (arraysize(value->pcr_select) < value->sizeof_select) {
    return TPM_RC_INSUFFICIENT;
  }
  for (uint32_t i = 0; i < value->sizeof_select; ++i) {
    result = Parse_BYTE(
        buffer,
        &value->pcr_select[i],
        value_bytes);
    if (result) {
      return result;
    }
  }
  return result;
}

TPM_RC Serialize_TPMS_PCR_SELECTION(
    const TPMS_PCR_SELECTION& value,
    std::string* buffer) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Serialize_TPMI_ALG_HASH(value.hash, buffer);
  if (result) {
    return result;
  }

  result = Serialize_UINT8(value.sizeof_select, buffer);
  if (result) {
    return result;
  }

  if (arraysize(value.pcr_select) < value.sizeof_select) {
    return TPM_RC_INSUFFICIENT;
  }
  for (uint32_t i = 0; i < value.sizeof_select; ++i) {
    result = Serialize_BYTE(value.pcr_select[i], buffer);
    if (result) {
      return result;
    }
  }
  return result;
}

TPM_RC Parse_TPMS_PCR_SELECTION(
    std::string* buffer,
    TPMS_PCR_SELECTION* value,
    std::string* value_bytes) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Parse_TPMI_ALG_HASH(
      buffer,
      &value->hash,
      value_bytes);
  if (result) {
    return result;
  }

  result = Parse_UINT8(
      buffer,
      &value->sizeof_select,
      value_bytes);
  if (result) {
    return result;
  }

  if (arraysize(value->pcr_select) < value->sizeof_select) {
    return TPM_RC_INSUFFICIENT;
  }
  for (uint32_t i = 0; i < value->sizeof_select; ++i) {
    result = Parse_BYTE(
        buffer,
        &value->pcr_select[i],
        value_bytes);
    if (result) {
      return result;
    }
  }
  return result;
}

TPM_RC Serialize_TPMT_TK_CREATION(
    const TPMT_TK_CREATION& value,
    std::string* buffer) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Serialize_TPM_ST(value.tag, buffer);
  if (result) {
    return result;
  }

  result = Serialize_TPMI_RH_HIERARCHY(value.hierarchy, buffer);
  if (result) {
    return result;
  }

  result = Serialize_TPM2B_DIGEST(value.digest, buffer);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Parse_TPMT_TK_CREATION(
    std::string* buffer,
    TPMT_TK_CREATION* value,
    std::string* value_bytes) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Parse_TPM_ST(
      buffer,
      &value->tag,
      value_bytes);
  if (result) {
    return result;
  }

  result = Parse_TPMI_RH_HIERARCHY(
      buffer,
      &value->hierarchy,
      value_bytes);
  if (result) {
    return result;
  }

  result = Parse_TPM2B_DIGEST(
      buffer,
      &value->digest,
      value_bytes);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Serialize_TPMT_TK_VERIFIED(
    const TPMT_TK_VERIFIED& value,
    std::string* buffer) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Serialize_TPM_ST(value.tag, buffer);
  if (result) {
    return result;
  }

  result = Serialize_TPMI_RH_HIERARCHY(value.hierarchy, buffer);
  if (result) {
    return result;
  }

  result = Serialize_TPM2B_DIGEST(value.digest, buffer);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Parse_TPMT_TK_VERIFIED(
    std::string* buffer,
    TPMT_TK_VERIFIED* value,
    std::string* value_bytes) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Parse_TPM_ST(
      buffer,
      &value->tag,
      value_bytes);
  if (result) {
    return result;
  }

  result = Parse_TPMI_RH_HIERARCHY(
      buffer,
      &value->hierarchy,
      value_bytes);
  if (result) {
    return result;
  }

  result = Parse_TPM2B_DIGEST(
      buffer,
      &value->digest,
      value_bytes);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Serialize_TPMT_TK_AUTH(
    const TPMT_TK_AUTH& value,
    std::string* buffer) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Serialize_TPMI_RH_HIERARCHY(value.hierarchy, buffer);
  if (result) {
    return result;
  }

  result = Serialize_TPM2B_DIGEST(value.digest, buffer);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Parse_TPMT_TK_AUTH(
    std::string* buffer,
    TPMT_TK_AUTH* value,
    std::string* value_bytes) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Parse_TPMI_RH_HIERARCHY(
      buffer,
      &value->hierarchy,
      value_bytes);
  if (result) {
    return result;
  }

  result = Parse_TPM2B_DIGEST(
      buffer,
      &value->digest,
      value_bytes);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Serialize_TPMT_TK_HASHCHECK(
    const TPMT_TK_HASHCHECK& value,
    std::string* buffer) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Serialize_TPM_ST(value.tag, buffer);
  if (result) {
    return result;
  }

  result = Serialize_TPMI_RH_HIERARCHY(value.hierarchy, buffer);
  if (result) {
    return result;
  }

  result = Serialize_TPM2B_DIGEST(value.digest, buffer);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Parse_TPMT_TK_HASHCHECK(
    std::string* buffer,
    TPMT_TK_HASHCHECK* value,
    std::string* value_bytes) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Parse_TPM_ST(
      buffer,
      &value->tag,
      value_bytes);
  if (result) {
    return result;
  }

  result = Parse_TPMI_RH_HIERARCHY(
      buffer,
      &value->hierarchy,
      value_bytes);
  if (result) {
    return result;
  }

  result = Parse_TPM2B_DIGEST(
      buffer,
      &value->digest,
      value_bytes);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Serialize_TPMS_ALG_PROPERTY(
    const TPMS_ALG_PROPERTY& value,
    std::string* buffer) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Serialize_TPM_ALG_ID(value.alg, buffer);
  if (result) {
    return result;
  }

  result = Serialize_TPMA_ALGORITHM(value.alg_properties, buffer);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Parse_TPMS_ALG_PROPERTY(
    std::string* buffer,
    TPMS_ALG_PROPERTY* value,
    std::string* value_bytes) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Parse_TPM_ALG_ID(
      buffer,
      &value->alg,
      value_bytes);
  if (result) {
    return result;
  }

  result = Parse_TPMA_ALGORITHM(
      buffer,
      &value->alg_properties,
      value_bytes);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Serialize_TPMS_TAGGED_PROPERTY(
    const TPMS_TAGGED_PROPERTY& value,
    std::string* buffer) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Serialize_TPM_PT(value.property, buffer);
  if (result) {
    return result;
  }

  result = Serialize_UINT32(value.value, buffer);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Parse_TPMS_TAGGED_PROPERTY(
    std::string* buffer,
    TPMS_TAGGED_PROPERTY* value,
    std::string* value_bytes) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Parse_TPM_PT(
      buffer,
      &value->property,
      value_bytes);
  if (result) {
    return result;
  }

  result = Parse_UINT32(
      buffer,
      &value->value,
      value_bytes);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Serialize_TPMS_TAGGED_PCR_SELECT(
    const TPMS_TAGGED_PCR_SELECT& value,
    std::string* buffer) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Serialize_TPM_PT(value.tag, buffer);
  if (result) {
    return result;
  }

  result = Serialize_UINT8(value.sizeof_select, buffer);
  if (result) {
    return result;
  }

  if (arraysize(value.pcr_select) < value.sizeof_select) {
    return TPM_RC_INSUFFICIENT;
  }
  for (uint32_t i = 0; i < value.sizeof_select; ++i) {
    result = Serialize_BYTE(value.pcr_select[i], buffer);
    if (result) {
      return result;
    }
  }
  return result;
}

TPM_RC Parse_TPMS_TAGGED_PCR_SELECT(
    std::string* buffer,
    TPMS_TAGGED_PCR_SELECT* value,
    std::string* value_bytes) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Parse_TPM_PT(
      buffer,
      &value->tag,
      value_bytes);
  if (result) {
    return result;
  }

  result = Parse_UINT8(
      buffer,
      &value->sizeof_select,
      value_bytes);
  if (result) {
    return result;
  }

  if (arraysize(value->pcr_select) < value->sizeof_select) {
    return TPM_RC_INSUFFICIENT;
  }
  for (uint32_t i = 0; i < value->sizeof_select; ++i) {
    result = Parse_BYTE(
        buffer,
        &value->pcr_select[i],
        value_bytes);
    if (result) {
      return result;
    }
  }
  return result;
}

TPM_RC Serialize_TPML_CC(
    const TPML_CC& value,
    std::string* buffer) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Serialize_UINT32(value.count, buffer);
  if (result) {
    return result;
  }

  if (arraysize(value.command_codes) < value.count) {
    return TPM_RC_INSUFFICIENT;
  }
  for (uint32_t i = 0; i < value.count; ++i) {
    result = Serialize_TPM_CC(value.command_codes[i], buffer);
    if (result) {
      return result;
    }
  }
  return result;
}

TPM_RC Parse_TPML_CC(
    std::string* buffer,
    TPML_CC* value,
    std::string* value_bytes) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Parse_UINT32(
      buffer,
      &value->count,
      value_bytes);
  if (result) {
    return result;
  }

  if (arraysize(value->command_codes) < value->count) {
    return TPM_RC_INSUFFICIENT;
  }
  for (uint32_t i = 0; i < value->count; ++i) {
    result = Parse_TPM_CC(
        buffer,
        &value->command_codes[i],
        value_bytes);
    if (result) {
      return result;
    }
  }
  return result;
}

TPM_RC Serialize_TPML_CCA(
    const TPML_CCA& value,
    std::string* buffer) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Serialize_UINT32(value.count, buffer);
  if (result) {
    return result;
  }

  if (arraysize(value.command_attributes) < value.count) {
    return TPM_RC_INSUFFICIENT;
  }
  for (uint32_t i = 0; i < value.count; ++i) {
    result = Serialize_TPMA_CC(value.command_attributes[i], buffer);
    if (result) {
      return result;
    }
  }
  return result;
}

TPM_RC Parse_TPML_CCA(
    std::string* buffer,
    TPML_CCA* value,
    std::string* value_bytes) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Parse_UINT32(
      buffer,
      &value->count,
      value_bytes);
  if (result) {
    return result;
  }

  if (arraysize(value->command_attributes) < value->count) {
    return TPM_RC_INSUFFICIENT;
  }
  for (uint32_t i = 0; i < value->count; ++i) {
    result = Parse_TPMA_CC(
        buffer,
        &value->command_attributes[i],
        value_bytes);
    if (result) {
      return result;
    }
  }
  return result;
}

TPM_RC Serialize_TPML_ALG(
    const TPML_ALG& value,
    std::string* buffer) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Serialize_UINT32(value.count, buffer);
  if (result) {
    return result;
  }

  if (arraysize(value.algorithms) < value.count) {
    return TPM_RC_INSUFFICIENT;
  }
  for (uint32_t i = 0; i < value.count; ++i) {
    result = Serialize_TPM_ALG_ID(value.algorithms[i], buffer);
    if (result) {
      return result;
    }
  }
  return result;
}

TPM_RC Parse_TPML_ALG(
    std::string* buffer,
    TPML_ALG* value,
    std::string* value_bytes) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Parse_UINT32(
      buffer,
      &value->count,
      value_bytes);
  if (result) {
    return result;
  }

  if (arraysize(value->algorithms) < value->count) {
    return TPM_RC_INSUFFICIENT;
  }
  for (uint32_t i = 0; i < value->count; ++i) {
    result = Parse_TPM_ALG_ID(
        buffer,
        &value->algorithms[i],
        value_bytes);
    if (result) {
      return result;
    }
  }
  return result;
}

TPM_RC Serialize_TPML_HANDLE(
    const TPML_HANDLE& value,
    std::string* buffer) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Serialize_UINT32(value.count, buffer);
  if (result) {
    return result;
  }

  if (arraysize(value.handle) < value.count) {
    return TPM_RC_INSUFFICIENT;
  }
  for (uint32_t i = 0; i < value.count; ++i) {
    result = Serialize_TPM_HANDLE(value.handle[i], buffer);
    if (result) {
      return result;
    }
  }
  return result;
}

TPM_RC Parse_TPML_HANDLE(
    std::string* buffer,
    TPML_HANDLE* value,
    std::string* value_bytes) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Parse_UINT32(
      buffer,
      &value->count,
      value_bytes);
  if (result) {
    return result;
  }

  if (arraysize(value->handle) < value->count) {
    return TPM_RC_INSUFFICIENT;
  }
  for (uint32_t i = 0; i < value->count; ++i) {
    result = Parse_TPM_HANDLE(
        buffer,
        &value->handle[i],
        value_bytes);
    if (result) {
      return result;
    }
  }
  return result;
}

TPM_RC Serialize_TPML_DIGEST(
    const TPML_DIGEST& value,
    std::string* buffer) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Serialize_UINT32(value.count, buffer);
  if (result) {
    return result;
  }

  if (arraysize(value.digests) < value.count) {
    return TPM_RC_INSUFFICIENT;
  }
  for (uint32_t i = 0; i < value.count; ++i) {
    result = Serialize_TPM2B_DIGEST(value.digests[i], buffer);
    if (result) {
      return result;
    }
  }
  return result;
}

TPM_RC Parse_TPML_DIGEST(
    std::string* buffer,
    TPML_DIGEST* value,
    std::string* value_bytes) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Parse_UINT32(
      buffer,
      &value->count,
      value_bytes);
  if (result) {
    return result;
  }

  if (arraysize(value->digests) < value->count) {
    return TPM_RC_INSUFFICIENT;
  }
  for (uint32_t i = 0; i < value->count; ++i) {
    result = Parse_TPM2B_DIGEST(
        buffer,
        &value->digests[i],
        value_bytes);
    if (result) {
      return result;
    }
  }
  return result;
}

TPM_RC Serialize_TPML_DIGEST_VALUES(
    const TPML_DIGEST_VALUES& value,
    std::string* buffer) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Serialize_UINT32(value.count, buffer);
  if (result) {
    return result;
  }

  if (arraysize(value.digests) < value.count) {
    return TPM_RC_INSUFFICIENT;
  }
  for (uint32_t i = 0; i < value.count; ++i) {
    result = Serialize_TPMT_HA(value.digests[i], buffer);
    if (result) {
      return result;
    }
  }
  return result;
}

TPM_RC Parse_TPML_DIGEST_VALUES(
    std::string* buffer,
    TPML_DIGEST_VALUES* value,
    std::string* value_bytes) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Parse_UINT32(
      buffer,
      &value->count,
      value_bytes);
  if (result) {
    return result;
  }

  if (arraysize(value->digests) < value->count) {
    return TPM_RC_INSUFFICIENT;
  }
  for (uint32_t i = 0; i < value->count; ++i) {
    result = Parse_TPMT_HA(
        buffer,
        &value->digests[i],
        value_bytes);
    if (result) {
      return result;
    }
  }
  return result;
}

TPM_RC Serialize_TPM2B_DIGEST_VALUES(
    const TPM2B_DIGEST_VALUES& value,
    std::string* buffer) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Serialize_UINT16(value.size, buffer);
  if (result) {
    return result;
  }

  if (arraysize(value.buffer) < value.size) {
    return TPM_RC_INSUFFICIENT;
  }
  for (uint32_t i = 0; i < value.size; ++i) {
    result = Serialize_BYTE(value.buffer[i], buffer);
    if (result) {
      return result;
    }
  }
  return result;
}

TPM_RC Parse_TPM2B_DIGEST_VALUES(
    std::string* buffer,
    TPM2B_DIGEST_VALUES* value,
    std::string* value_bytes) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Parse_UINT16(
      buffer,
      &value->size,
      value_bytes);
  if (result) {
    return result;
  }

  if (arraysize(value->buffer) < value->size) {
    return TPM_RC_INSUFFICIENT;
  }
  for (uint32_t i = 0; i < value->size; ++i) {
    result = Parse_BYTE(
        buffer,
        &value->buffer[i],
        value_bytes);
    if (result) {
      return result;
    }
  }
  return result;
}

TPM2B_DIGEST_VALUES Make_TPM2B_DIGEST_VALUES(
    const std::string& bytes) {
  TPM2B_DIGEST_VALUES tpm2b;
  CHECK(bytes.size() <= sizeof(tpm2b.buffer));
  memset(&tpm2b, 0, sizeof(TPM2B_DIGEST_VALUES));
  tpm2b.size = bytes.size();
  memcpy(tpm2b.buffer, bytes.data(), bytes.size());
  return tpm2b;
}

std::string StringFrom_TPM2B_DIGEST_VALUES(
    const TPM2B_DIGEST_VALUES& tpm2b) {
  const char* char_buffer = reinterpret_cast<const char*>(
      tpm2b.buffer);
  return std::string(char_buffer, tpm2b.size);
}

TPM_RC Serialize_TPML_PCR_SELECTION(
    const TPML_PCR_SELECTION& value,
    std::string* buffer) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Serialize_UINT32(value.count, buffer);
  if (result) {
    return result;
  }

  if (arraysize(value.pcr_selections) < value.count) {
    return TPM_RC_INSUFFICIENT;
  }
  for (uint32_t i = 0; i < value.count; ++i) {
    result = Serialize_TPMS_PCR_SELECTION(value.pcr_selections[i], buffer);
    if (result) {
      return result;
    }
  }
  return result;
}

TPM_RC Parse_TPML_PCR_SELECTION(
    std::string* buffer,
    TPML_PCR_SELECTION* value,
    std::string* value_bytes) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Parse_UINT32(
      buffer,
      &value->count,
      value_bytes);
  if (result) {
    return result;
  }

  if (arraysize(value->pcr_selections) < value->count) {
    return TPM_RC_INSUFFICIENT;
  }
  for (uint32_t i = 0; i < value->count; ++i) {
    result = Parse_TPMS_PCR_SELECTION(
        buffer,
        &value->pcr_selections[i],
        value_bytes);
    if (result) {
      return result;
    }
  }
  return result;
}

TPM_RC Serialize_TPML_ALG_PROPERTY(
    const TPML_ALG_PROPERTY& value,
    std::string* buffer) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Serialize_UINT32(value.count, buffer);
  if (result) {
    return result;
  }

  if (arraysize(value.alg_properties) < value.count) {
    return TPM_RC_INSUFFICIENT;
  }
  for (uint32_t i = 0; i < value.count; ++i) {
    result = Serialize_TPMS_ALG_PROPERTY(value.alg_properties[i], buffer);
    if (result) {
      return result;
    }
  }
  return result;
}

TPM_RC Parse_TPML_ALG_PROPERTY(
    std::string* buffer,
    TPML_ALG_PROPERTY* value,
    std::string* value_bytes) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Parse_UINT32(
      buffer,
      &value->count,
      value_bytes);
  if (result) {
    return result;
  }

  if (arraysize(value->alg_properties) < value->count) {
    return TPM_RC_INSUFFICIENT;
  }
  for (uint32_t i = 0; i < value->count; ++i) {
    result = Parse_TPMS_ALG_PROPERTY(
        buffer,
        &value->alg_properties[i],
        value_bytes);
    if (result) {
      return result;
    }
  }
  return result;
}

TPM_RC Serialize_TPML_TAGGED_TPM_PROPERTY(
    const TPML_TAGGED_TPM_PROPERTY& value,
    std::string* buffer) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Serialize_UINT32(value.count, buffer);
  if (result) {
    return result;
  }

  if (arraysize(value.tpm_property) < value.count) {
    return TPM_RC_INSUFFICIENT;
  }
  for (uint32_t i = 0; i < value.count; ++i) {
    result = Serialize_TPMS_TAGGED_PROPERTY(value.tpm_property[i], buffer);
    if (result) {
      return result;
    }
  }
  return result;
}

TPM_RC Parse_TPML_TAGGED_TPM_PROPERTY(
    std::string* buffer,
    TPML_TAGGED_TPM_PROPERTY* value,
    std::string* value_bytes) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Parse_UINT32(
      buffer,
      &value->count,
      value_bytes);
  if (result) {
    return result;
  }

  if (arraysize(value->tpm_property) < value->count) {
    return TPM_RC_INSUFFICIENT;
  }
  for (uint32_t i = 0; i < value->count; ++i) {
    result = Parse_TPMS_TAGGED_PROPERTY(
        buffer,
        &value->tpm_property[i],
        value_bytes);
    if (result) {
      return result;
    }
  }
  return result;
}

TPM_RC Serialize_TPML_TAGGED_PCR_PROPERTY(
    const TPML_TAGGED_PCR_PROPERTY& value,
    std::string* buffer) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Serialize_UINT32(value.count, buffer);
  if (result) {
    return result;
  }

  if (arraysize(value.pcr_property) < value.count) {
    return TPM_RC_INSUFFICIENT;
  }
  for (uint32_t i = 0; i < value.count; ++i) {
    result = Serialize_TPMS_TAGGED_PCR_SELECT(value.pcr_property[i], buffer);
    if (result) {
      return result;
    }
  }
  return result;
}

TPM_RC Parse_TPML_TAGGED_PCR_PROPERTY(
    std::string* buffer,
    TPML_TAGGED_PCR_PROPERTY* value,
    std::string* value_bytes) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Parse_UINT32(
      buffer,
      &value->count,
      value_bytes);
  if (result) {
    return result;
  }

  if (arraysize(value->pcr_property) < value->count) {
    return TPM_RC_INSUFFICIENT;
  }
  for (uint32_t i = 0; i < value->count; ++i) {
    result = Parse_TPMS_TAGGED_PCR_SELECT(
        buffer,
        &value->pcr_property[i],
        value_bytes);
    if (result) {
      return result;
    }
  }
  return result;
}

TPM_RC Serialize_TPML_ECC_CURVE(
    const TPML_ECC_CURVE& value,
    std::string* buffer) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Serialize_UINT32(value.count, buffer);
  if (result) {
    return result;
  }

  if (arraysize(value.ecc_curves) < value.count) {
    return TPM_RC_INSUFFICIENT;
  }
  for (uint32_t i = 0; i < value.count; ++i) {
    result = Serialize_TPM_ECC_CURVE(value.ecc_curves[i], buffer);
    if (result) {
      return result;
    }
  }
  return result;
}

TPM_RC Parse_TPML_ECC_CURVE(
    std::string* buffer,
    TPML_ECC_CURVE* value,
    std::string* value_bytes) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Parse_UINT32(
      buffer,
      &value->count,
      value_bytes);
  if (result) {
    return result;
  }

  if (arraysize(value->ecc_curves) < value->count) {
    return TPM_RC_INSUFFICIENT;
  }
  for (uint32_t i = 0; i < value->count; ++i) {
    result = Parse_TPM_ECC_CURVE(
        buffer,
        &value->ecc_curves[i],
        value_bytes);
    if (result) {
      return result;
    }
  }
  return result;
}

TPM_RC Serialize_TPMU_CAPABILITIES(
    const TPMU_CAPABILITIES& value,
    TPM_CAP selector,
    std::string* buffer) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  if (selector == TPM_CAP_PCRS) {
    result = Serialize_TPML_PCR_SELECTION(value.assigned_pcr, buffer);
    if (result) {
      return result;
    }
  }

  if (selector == TPM_CAP_TPM_PROPERTIES) {
    result = Serialize_TPML_TAGGED_TPM_PROPERTY(value.tpm_properties, buffer);
    if (result) {
      return result;
    }
  }

  if (selector == TPM_CAP_PP_COMMANDS) {
    result = Serialize_TPML_CC(value.pp_commands, buffer);
    if (result) {
      return result;
    }
  }

  if (selector == TPM_CAP_AUDIT_COMMANDS) {
    result = Serialize_TPML_CC(value.audit_commands, buffer);
    if (result) {
      return result;
    }
  }

  if (selector == TPM_CAP_COMMANDS) {
    result = Serialize_TPML_CCA(value.command, buffer);
    if (result) {
      return result;
    }
  }

  if (selector == TPM_CAP_ECC_CURVES) {
    result = Serialize_TPML_ECC_CURVE(value.ecc_curves, buffer);
    if (result) {
      return result;
    }
  }

  if (selector == TPM_CAP_PCR_PROPERTIES) {
    result = Serialize_TPML_TAGGED_PCR_PROPERTY(value.pcr_properties, buffer);
    if (result) {
      return result;
    }
  }

  if (selector == TPM_CAP_HANDLES) {
    result = Serialize_TPML_HANDLE(value.handles, buffer);
    if (result) {
      return result;
    }
  }

  if (selector == TPM_CAP_ALGS) {
    result = Serialize_TPML_ALG_PROPERTY(value.algorithms, buffer);
    if (result) {
      return result;
    }
  }
  return result;
}

TPM_RC Parse_TPMU_CAPABILITIES(
    std::string* buffer,
    TPM_CAP selector,
    TPMU_CAPABILITIES* value,
    std::string* value_bytes) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  if (selector == TPM_CAP_PCRS) {
    result = Parse_TPML_PCR_SELECTION(
        buffer,
        &value->assigned_pcr,
        value_bytes);
    if (result) {
      return result;
    }
  }

  if (selector == TPM_CAP_TPM_PROPERTIES) {
    result = Parse_TPML_TAGGED_TPM_PROPERTY(
        buffer,
        &value->tpm_properties,
        value_bytes);
    if (result) {
      return result;
    }
  }

  if (selector == TPM_CAP_PP_COMMANDS) {
    result = Parse_TPML_CC(
        buffer,
        &value->pp_commands,
        value_bytes);
    if (result) {
      return result;
    }
  }

  if (selector == TPM_CAP_AUDIT_COMMANDS) {
    result = Parse_TPML_CC(
        buffer,
        &value->audit_commands,
        value_bytes);
    if (result) {
      return result;
    }
  }

  if (selector == TPM_CAP_COMMANDS) {
    result = Parse_TPML_CCA(
        buffer,
        &value->command,
        value_bytes);
    if (result) {
      return result;
    }
  }

  if (selector == TPM_CAP_ECC_CURVES) {
    result = Parse_TPML_ECC_CURVE(
        buffer,
        &value->ecc_curves,
        value_bytes);
    if (result) {
      return result;
    }
  }

  if (selector == TPM_CAP_PCR_PROPERTIES) {
    result = Parse_TPML_TAGGED_PCR_PROPERTY(
        buffer,
        &value->pcr_properties,
        value_bytes);
    if (result) {
      return result;
    }
  }

  if (selector == TPM_CAP_HANDLES) {
    result = Parse_TPML_HANDLE(
        buffer,
        &value->handles,
        value_bytes);
    if (result) {
      return result;
    }
  }

  if (selector == TPM_CAP_ALGS) {
    result = Parse_TPML_ALG_PROPERTY(
        buffer,
        &value->algorithms,
        value_bytes);
    if (result) {
      return result;
    }
  }
  return result;
}

TPM_RC Serialize_TPMS_CAPABILITY_DATA(
    const TPMS_CAPABILITY_DATA& value,
    std::string* buffer) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Serialize_TPM_CAP(value.capability, buffer);
  if (result) {
    return result;
  }

  result = Serialize_TPMU_CAPABILITIES(
      value.data,
      value.capability,
      buffer);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Parse_TPMS_CAPABILITY_DATA(
    std::string* buffer,
    TPMS_CAPABILITY_DATA* value,
    std::string* value_bytes) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Parse_TPM_CAP(
      buffer,
      &value->capability,
      value_bytes);
  if (result) {
    return result;
  }

  result = Parse_TPMU_CAPABILITIES(
      buffer,
      value->capability,
      &value->data,
      value_bytes);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Serialize_TPMS_CLOCK_INFO(
    const TPMS_CLOCK_INFO& value,
    std::string* buffer) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Serialize_UINT64(value.clock, buffer);
  if (result) {
    return result;
  }

  result = Serialize_UINT32(value.reset_count, buffer);
  if (result) {
    return result;
  }

  result = Serialize_UINT32(value.restart_count, buffer);
  if (result) {
    return result;
  }

  result = Serialize_TPMI_YES_NO(value.safe, buffer);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Parse_TPMS_CLOCK_INFO(
    std::string* buffer,
    TPMS_CLOCK_INFO* value,
    std::string* value_bytes) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Parse_UINT64(
      buffer,
      &value->clock,
      value_bytes);
  if (result) {
    return result;
  }

  result = Parse_UINT32(
      buffer,
      &value->reset_count,
      value_bytes);
  if (result) {
    return result;
  }

  result = Parse_UINT32(
      buffer,
      &value->restart_count,
      value_bytes);
  if (result) {
    return result;
  }

  result = Parse_TPMI_YES_NO(
      buffer,
      &value->safe,
      value_bytes);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Serialize_TPMS_TIME_INFO(
    const TPMS_TIME_INFO& value,
    std::string* buffer) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Serialize_UINT64(value.time, buffer);
  if (result) {
    return result;
  }

  result = Serialize_TPMS_CLOCK_INFO(value.clock_info, buffer);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Parse_TPMS_TIME_INFO(
    std::string* buffer,
    TPMS_TIME_INFO* value,
    std::string* value_bytes) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Parse_UINT64(
      buffer,
      &value->time,
      value_bytes);
  if (result) {
    return result;
  }

  result = Parse_TPMS_CLOCK_INFO(
      buffer,
      &value->clock_info,
      value_bytes);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Serialize_TPMS_TIME_ATTEST_INFO(
    const TPMS_TIME_ATTEST_INFO& value,
    std::string* buffer) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Serialize_TPMS_TIME_INFO(value.time, buffer);
  if (result) {
    return result;
  }

  result = Serialize_UINT64(value.firmware_version, buffer);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Parse_TPMS_TIME_ATTEST_INFO(
    std::string* buffer,
    TPMS_TIME_ATTEST_INFO* value,
    std::string* value_bytes) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Parse_TPMS_TIME_INFO(
      buffer,
      &value->time,
      value_bytes);
  if (result) {
    return result;
  }

  result = Parse_UINT64(
      buffer,
      &value->firmware_version,
      value_bytes);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Serialize_TPMS_CERTIFY_INFO(
    const TPMS_CERTIFY_INFO& value,
    std::string* buffer) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Serialize_TPM2B_NAME(value.name, buffer);
  if (result) {
    return result;
  }

  result = Serialize_TPM2B_NAME(value.qualified_name, buffer);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Parse_TPMS_CERTIFY_INFO(
    std::string* buffer,
    TPMS_CERTIFY_INFO* value,
    std::string* value_bytes) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Parse_TPM2B_NAME(
      buffer,
      &value->name,
      value_bytes);
  if (result) {
    return result;
  }

  result = Parse_TPM2B_NAME(
      buffer,
      &value->qualified_name,
      value_bytes);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Serialize_TPMS_QUOTE_INFO(
    const TPMS_QUOTE_INFO& value,
    std::string* buffer) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Serialize_TPML_PCR_SELECTION(value.pcr_select, buffer);
  if (result) {
    return result;
  }

  result = Serialize_TPM2B_DIGEST(value.pcr_digest, buffer);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Parse_TPMS_QUOTE_INFO(
    std::string* buffer,
    TPMS_QUOTE_INFO* value,
    std::string* value_bytes) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Parse_TPML_PCR_SELECTION(
      buffer,
      &value->pcr_select,
      value_bytes);
  if (result) {
    return result;
  }

  result = Parse_TPM2B_DIGEST(
      buffer,
      &value->pcr_digest,
      value_bytes);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Serialize_TPMS_COMMAND_AUDIT_INFO(
    const TPMS_COMMAND_AUDIT_INFO& value,
    std::string* buffer) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Serialize_UINT64(value.audit_counter, buffer);
  if (result) {
    return result;
  }

  result = Serialize_TPM_ALG_ID(value.digest_alg, buffer);
  if (result) {
    return result;
  }

  result = Serialize_TPM2B_DIGEST(value.audit_digest, buffer);
  if (result) {
    return result;
  }

  result = Serialize_TPM2B_DIGEST(value.command_digest, buffer);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Parse_TPMS_COMMAND_AUDIT_INFO(
    std::string* buffer,
    TPMS_COMMAND_AUDIT_INFO* value,
    std::string* value_bytes) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Parse_UINT64(
      buffer,
      &value->audit_counter,
      value_bytes);
  if (result) {
    return result;
  }

  result = Parse_TPM_ALG_ID(
      buffer,
      &value->digest_alg,
      value_bytes);
  if (result) {
    return result;
  }

  result = Parse_TPM2B_DIGEST(
      buffer,
      &value->audit_digest,
      value_bytes);
  if (result) {
    return result;
  }

  result = Parse_TPM2B_DIGEST(
      buffer,
      &value->command_digest,
      value_bytes);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Serialize_TPMS_SESSION_AUDIT_INFO(
    const TPMS_SESSION_AUDIT_INFO& value,
    std::string* buffer) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Serialize_TPMI_YES_NO(value.exclusive_session, buffer);
  if (result) {
    return result;
  }

  result = Serialize_TPM2B_DIGEST(value.session_digest, buffer);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Parse_TPMS_SESSION_AUDIT_INFO(
    std::string* buffer,
    TPMS_SESSION_AUDIT_INFO* value,
    std::string* value_bytes) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Parse_TPMI_YES_NO(
      buffer,
      &value->exclusive_session,
      value_bytes);
  if (result) {
    return result;
  }

  result = Parse_TPM2B_DIGEST(
      buffer,
      &value->session_digest,
      value_bytes);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Serialize_TPMS_CREATION_INFO(
    const TPMS_CREATION_INFO& value,
    std::string* buffer) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Serialize_TPM2B_NAME(value.object_name, buffer);
  if (result) {
    return result;
  }

  result = Serialize_TPM2B_DIGEST(value.creation_hash, buffer);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Parse_TPMS_CREATION_INFO(
    std::string* buffer,
    TPMS_CREATION_INFO* value,
    std::string* value_bytes) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Parse_TPM2B_NAME(
      buffer,
      &value->object_name,
      value_bytes);
  if (result) {
    return result;
  }

  result = Parse_TPM2B_DIGEST(
      buffer,
      &value->creation_hash,
      value_bytes);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Serialize_TPMS_NV_CERTIFY_INFO(
    const TPMS_NV_CERTIFY_INFO& value,
    std::string* buffer) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Serialize_TPM2B_NAME(value.index_name, buffer);
  if (result) {
    return result;
  }

  result = Serialize_UINT16(value.offset, buffer);
  if (result) {
    return result;
  }

  result = Serialize_TPM2B_MAX_NV_BUFFER(value.nv_contents, buffer);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Parse_TPMS_NV_CERTIFY_INFO(
    std::string* buffer,
    TPMS_NV_CERTIFY_INFO* value,
    std::string* value_bytes) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Parse_TPM2B_NAME(
      buffer,
      &value->index_name,
      value_bytes);
  if (result) {
    return result;
  }

  result = Parse_UINT16(
      buffer,
      &value->offset,
      value_bytes);
  if (result) {
    return result;
  }

  result = Parse_TPM2B_MAX_NV_BUFFER(
      buffer,
      &value->nv_contents,
      value_bytes);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Serialize_TPMU_ATTEST(
    const TPMU_ATTEST& value,
    TPMI_ST_ATTEST selector,
    std::string* buffer) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  if (selector == TPM_ST_ATTEST_SESSION_AUDIT) {
    result = Serialize_TPMS_SESSION_AUDIT_INFO(value.session_audit, buffer);
    if (result) {
      return result;
    }
  }

  if (selector == TPM_ST_ATTEST_QUOTE) {
    result = Serialize_TPMS_QUOTE_INFO(value.quote, buffer);
    if (result) {
      return result;
    }
  }

  if (selector == TPM_ST_ATTEST_COMMAND_AUDIT) {
    result = Serialize_TPMS_COMMAND_AUDIT_INFO(value.command_audit, buffer);
    if (result) {
      return result;
    }
  }

  if (selector == TPM_ST_ATTEST_CERTIFY) {
    result = Serialize_TPMS_CERTIFY_INFO(value.certify, buffer);
    if (result) {
      return result;
    }
  }

  if (selector == TPM_ST_ATTEST_NV) {
    result = Serialize_TPMS_NV_CERTIFY_INFO(value.nv, buffer);
    if (result) {
      return result;
    }
  }

  if (selector == TPM_ST_ATTEST_TIME) {
    result = Serialize_TPMS_TIME_ATTEST_INFO(value.time, buffer);
    if (result) {
      return result;
    }
  }

  if (selector == TPM_ST_ATTEST_CREATION) {
    result = Serialize_TPMS_CREATION_INFO(value.creation, buffer);
    if (result) {
      return result;
    }
  }
  return result;
}

TPM_RC Parse_TPMU_ATTEST(
    std::string* buffer,
    TPMI_ST_ATTEST selector,
    TPMU_ATTEST* value,
    std::string* value_bytes) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  if (selector == TPM_ST_ATTEST_SESSION_AUDIT) {
    result = Parse_TPMS_SESSION_AUDIT_INFO(
        buffer,
        &value->session_audit,
        value_bytes);
    if (result) {
      return result;
    }
  }

  if (selector == TPM_ST_ATTEST_QUOTE) {
    result = Parse_TPMS_QUOTE_INFO(
        buffer,
        &value->quote,
        value_bytes);
    if (result) {
      return result;
    }
  }

  if (selector == TPM_ST_ATTEST_COMMAND_AUDIT) {
    result = Parse_TPMS_COMMAND_AUDIT_INFO(
        buffer,
        &value->command_audit,
        value_bytes);
    if (result) {
      return result;
    }
  }

  if (selector == TPM_ST_ATTEST_CERTIFY) {
    result = Parse_TPMS_CERTIFY_INFO(
        buffer,
        &value->certify,
        value_bytes);
    if (result) {
      return result;
    }
  }

  if (selector == TPM_ST_ATTEST_NV) {
    result = Parse_TPMS_NV_CERTIFY_INFO(
        buffer,
        &value->nv,
        value_bytes);
    if (result) {
      return result;
    }
  }

  if (selector == TPM_ST_ATTEST_TIME) {
    result = Parse_TPMS_TIME_ATTEST_INFO(
        buffer,
        &value->time,
        value_bytes);
    if (result) {
      return result;
    }
  }

  if (selector == TPM_ST_ATTEST_CREATION) {
    result = Parse_TPMS_CREATION_INFO(
        buffer,
        &value->creation,
        value_bytes);
    if (result) {
      return result;
    }
  }
  return result;
}

TPM_RC Serialize_TPMS_ATTEST(
    const TPMS_ATTEST& value,
    std::string* buffer) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Serialize_TPM_GENERATED(value.magic, buffer);
  if (result) {
    return result;
  }

  result = Serialize_TPMI_ST_ATTEST(value.type, buffer);
  if (result) {
    return result;
  }

  result = Serialize_TPM2B_NAME(value.qualified_signer, buffer);
  if (result) {
    return result;
  }

  result = Serialize_TPM2B_DATA(value.extra_data, buffer);
  if (result) {
    return result;
  }

  result = Serialize_TPMS_CLOCK_INFO(value.clock_info, buffer);
  if (result) {
    return result;
  }

  result = Serialize_UINT64(value.firmware_version, buffer);
  if (result) {
    return result;
  }

  result = Serialize_TPMU_ATTEST(
      value.attested,
      value.type,
      buffer);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Parse_TPMS_ATTEST(
    std::string* buffer,
    TPMS_ATTEST* value,
    std::string* value_bytes) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Parse_TPM_GENERATED(
      buffer,
      &value->magic,
      value_bytes);
  if (result) {
    return result;
  }

  result = Parse_TPMI_ST_ATTEST(
      buffer,
      &value->type,
      value_bytes);
  if (result) {
    return result;
  }

  result = Parse_TPM2B_NAME(
      buffer,
      &value->qualified_signer,
      value_bytes);
  if (result) {
    return result;
  }

  result = Parse_TPM2B_DATA(
      buffer,
      &value->extra_data,
      value_bytes);
  if (result) {
    return result;
  }

  result = Parse_TPMS_CLOCK_INFO(
      buffer,
      &value->clock_info,
      value_bytes);
  if (result) {
    return result;
  }

  result = Parse_UINT64(
      buffer,
      &value->firmware_version,
      value_bytes);
  if (result) {
    return result;
  }

  result = Parse_TPMU_ATTEST(
      buffer,
      value->type,
      &value->attested,
      value_bytes);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Serialize_TPM2B_ATTEST(
    const TPM2B_ATTEST& value,
    std::string* buffer) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Serialize_UINT16(value.size, buffer);
  if (result) {
    return result;
  }

  if (arraysize(value.attestation_data) < value.size) {
    return TPM_RC_INSUFFICIENT;
  }
  for (uint32_t i = 0; i < value.size; ++i) {
    result = Serialize_BYTE(value.attestation_data[i], buffer);
    if (result) {
      return result;
    }
  }
  return result;
}

TPM_RC Parse_TPM2B_ATTEST(
    std::string* buffer,
    TPM2B_ATTEST* value,
    std::string* value_bytes) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Parse_UINT16(
      buffer,
      &value->size,
      value_bytes);
  if (result) {
    return result;
  }

  if (arraysize(value->attestation_data) < value->size) {
    return TPM_RC_INSUFFICIENT;
  }
  for (uint32_t i = 0; i < value->size; ++i) {
    result = Parse_BYTE(
        buffer,
        &value->attestation_data[i],
        value_bytes);
    if (result) {
      return result;
    }
  }
  return result;
}

TPM2B_ATTEST Make_TPM2B_ATTEST(
    const std::string& bytes) {
  TPM2B_ATTEST tpm2b;
  CHECK(bytes.size() <= sizeof(tpm2b.attestation_data));
  memset(&tpm2b, 0, sizeof(TPM2B_ATTEST));
  tpm2b.size = bytes.size();
  memcpy(tpm2b.attestation_data, bytes.data(), bytes.size());
  return tpm2b;
}

std::string StringFrom_TPM2B_ATTEST(
    const TPM2B_ATTEST& tpm2b) {
  const char* char_buffer = reinterpret_cast<const char*>(
      tpm2b.attestation_data);
  return std::string(char_buffer, tpm2b.size);
}

TPM_RC Serialize_TPMS_AUTH_COMMAND(
    const TPMS_AUTH_COMMAND& value,
    std::string* buffer) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Serialize_TPMI_SH_AUTH_SESSION(value.session_handle, buffer);
  if (result) {
    return result;
  }

  result = Serialize_TPM2B_NONCE(value.nonce, buffer);
  if (result) {
    return result;
  }

  result = Serialize_TPMA_SESSION(value.session_attributes, buffer);
  if (result) {
    return result;
  }

  result = Serialize_TPM2B_AUTH(value.hmac, buffer);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Parse_TPMS_AUTH_COMMAND(
    std::string* buffer,
    TPMS_AUTH_COMMAND* value,
    std::string* value_bytes) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Parse_TPMI_SH_AUTH_SESSION(
      buffer,
      &value->session_handle,
      value_bytes);
  if (result) {
    return result;
  }

  result = Parse_TPM2B_NONCE(
      buffer,
      &value->nonce,
      value_bytes);
  if (result) {
    return result;
  }

  result = Parse_TPMA_SESSION(
      buffer,
      &value->session_attributes,
      value_bytes);
  if (result) {
    return result;
  }

  result = Parse_TPM2B_AUTH(
      buffer,
      &value->hmac,
      value_bytes);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Serialize_TPMS_AUTH_RESPONSE(
    const TPMS_AUTH_RESPONSE& value,
    std::string* buffer) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Serialize_TPM2B_NONCE(value.nonce, buffer);
  if (result) {
    return result;
  }

  result = Serialize_TPMA_SESSION(value.session_attributes, buffer);
  if (result) {
    return result;
  }

  result = Serialize_TPM2B_AUTH(value.hmac, buffer);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Parse_TPMS_AUTH_RESPONSE(
    std::string* buffer,
    TPMS_AUTH_RESPONSE* value,
    std::string* value_bytes) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Parse_TPM2B_NONCE(
      buffer,
      &value->nonce,
      value_bytes);
  if (result) {
    return result;
  }

  result = Parse_TPMA_SESSION(
      buffer,
      &value->session_attributes,
      value_bytes);
  if (result) {
    return result;
  }

  result = Parse_TPM2B_AUTH(
      buffer,
      &value->hmac,
      value_bytes);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Serialize_TPMU_SYM_KEY_BITS(
    const TPMU_SYM_KEY_BITS& value,
    TPMI_ALG_SYM selector,
    std::string* buffer) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  if (selector == TPM_ALG_NULL) {
    // Do nothing.
  }

  if (selector == TPM_ALG_SM4) {
    result = Serialize_TPMI_SM4_KEY_BITS(value.sm4, buffer);
    if (result) {
      return result;
    }
  }

  if (selector == TPM_ALG_AES) {
    result = Serialize_TPMI_AES_KEY_BITS(value.aes, buffer);
    if (result) {
      return result;
    }
  }

  if (selector == TPM_ALG_XOR) {
    result = Serialize_TPMI_ALG_HASH(value.xor_, buffer);
    if (result) {
      return result;
    }
  }
  return result;
}

TPM_RC Parse_TPMU_SYM_KEY_BITS(
    std::string* buffer,
    TPMI_ALG_SYM selector,
    TPMU_SYM_KEY_BITS* value,
    std::string* value_bytes) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  if (selector == TPM_ALG_NULL) {
    // Do nothing.
  }

  if (selector == TPM_ALG_SM4) {
    result = Parse_TPMI_SM4_KEY_BITS(
        buffer,
        &value->sm4,
        value_bytes);
    if (result) {
      return result;
    }
  }

  if (selector == TPM_ALG_AES) {
    result = Parse_TPMI_AES_KEY_BITS(
        buffer,
        &value->aes,
        value_bytes);
    if (result) {
      return result;
    }
  }

  if (selector == TPM_ALG_XOR) {
    result = Parse_TPMI_ALG_HASH(
        buffer,
        &value->xor_,
        value_bytes);
    if (result) {
      return result;
    }
  }
  return result;
}

TPM_RC Serialize_TPMU_SYM_MODE(
    const TPMU_SYM_MODE& value,
    TPMI_ALG_SYM selector,
    std::string* buffer) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  if (selector == TPM_ALG_NULL) {
    // Do nothing.
  }

  if (selector == TPM_ALG_SM4) {
    result = Serialize_TPMI_ALG_SYM_MODE(value.sm4, buffer);
    if (result) {
      return result;
    }
  }

  if (selector == TPM_ALG_AES) {
    result = Serialize_TPMI_ALG_SYM_MODE(value.aes, buffer);
    if (result) {
      return result;
    }
  }

  if (selector == TPM_ALG_XOR) {
    // Do nothing.
  }
  return result;
}

TPM_RC Parse_TPMU_SYM_MODE(
    std::string* buffer,
    TPMI_ALG_SYM selector,
    TPMU_SYM_MODE* value,
    std::string* value_bytes) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  if (selector == TPM_ALG_NULL) {
    // Do nothing.
  }

  if (selector == TPM_ALG_SM4) {
    result = Parse_TPMI_ALG_SYM_MODE(
        buffer,
        &value->sm4,
        value_bytes);
    if (result) {
      return result;
    }
  }

  if (selector == TPM_ALG_AES) {
    result = Parse_TPMI_ALG_SYM_MODE(
        buffer,
        &value->aes,
        value_bytes);
    if (result) {
      return result;
    }
  }

  if (selector == TPM_ALG_XOR) {
    // Do nothing.
  }
  return result;
}

TPM_RC Serialize_TPMU_SYM_DETAILS(
    const TPMU_SYM_DETAILS& value,
    TPMI_ALG_SYM selector,
    std::string* buffer) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;
  return result;
}

TPM_RC Parse_TPMU_SYM_DETAILS(
    std::string* buffer,
    TPMI_ALG_SYM selector,
    TPMU_SYM_DETAILS* value,
    std::string* value_bytes) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;
  return result;
}

TPM_RC Serialize_TPMT_SYM_DEF(
    const TPMT_SYM_DEF& value,
    std::string* buffer) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Serialize_TPMI_ALG_SYM(value.algorithm, buffer);
  if (result) {
    return result;
  }

  result = Serialize_TPMU_SYM_KEY_BITS(
      value.key_bits,
      value.algorithm,
      buffer);
  if (result) {
    return result;
  }

  result = Serialize_TPMU_SYM_MODE(
      value.mode,
      value.algorithm,
      buffer);
  if (result) {
    return result;
  }

  result = Serialize_TPMU_SYM_DETAILS(
      value.details,
      value.algorithm,
      buffer);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Parse_TPMT_SYM_DEF(
    std::string* buffer,
    TPMT_SYM_DEF* value,
    std::string* value_bytes) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Parse_TPMI_ALG_SYM(
      buffer,
      &value->algorithm,
      value_bytes);
  if (result) {
    return result;
  }

  result = Parse_TPMU_SYM_KEY_BITS(
      buffer,
      value->algorithm,
      &value->key_bits,
      value_bytes);
  if (result) {
    return result;
  }

  result = Parse_TPMU_SYM_MODE(
      buffer,
      value->algorithm,
      &value->mode,
      value_bytes);
  if (result) {
    return result;
  }

  result = Parse_TPMU_SYM_DETAILS(
      buffer,
      value->algorithm,
      &value->details,
      value_bytes);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Serialize_TPMT_SYM_DEF_OBJECT(
    const TPMT_SYM_DEF_OBJECT& value,
    std::string* buffer) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Serialize_TPMI_ALG_SYM_OBJECT(value.algorithm, buffer);
  if (result) {
    return result;
  }

  result = Serialize_TPMU_SYM_KEY_BITS(
      value.key_bits,
      value.algorithm,
      buffer);
  if (result) {
    return result;
  }

  result = Serialize_TPMU_SYM_MODE(
      value.mode,
      value.algorithm,
      buffer);
  if (result) {
    return result;
  }

  result = Serialize_TPMU_SYM_DETAILS(
      value.details,
      value.algorithm,
      buffer);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Parse_TPMT_SYM_DEF_OBJECT(
    std::string* buffer,
    TPMT_SYM_DEF_OBJECT* value,
    std::string* value_bytes) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Parse_TPMI_ALG_SYM_OBJECT(
      buffer,
      &value->algorithm,
      value_bytes);
  if (result) {
    return result;
  }

  result = Parse_TPMU_SYM_KEY_BITS(
      buffer,
      value->algorithm,
      &value->key_bits,
      value_bytes);
  if (result) {
    return result;
  }

  result = Parse_TPMU_SYM_MODE(
      buffer,
      value->algorithm,
      &value->mode,
      value_bytes);
  if (result) {
    return result;
  }

  result = Parse_TPMU_SYM_DETAILS(
      buffer,
      value->algorithm,
      &value->details,
      value_bytes);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Serialize_TPM2B_SYM_KEY(
    const TPM2B_SYM_KEY& value,
    std::string* buffer) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Serialize_UINT16(value.size, buffer);
  if (result) {
    return result;
  }

  if (arraysize(value.buffer) < value.size) {
    return TPM_RC_INSUFFICIENT;
  }
  for (uint32_t i = 0; i < value.size; ++i) {
    result = Serialize_BYTE(value.buffer[i], buffer);
    if (result) {
      return result;
    }
  }
  return result;
}

TPM_RC Parse_TPM2B_SYM_KEY(
    std::string* buffer,
    TPM2B_SYM_KEY* value,
    std::string* value_bytes) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Parse_UINT16(
      buffer,
      &value->size,
      value_bytes);
  if (result) {
    return result;
  }

  if (arraysize(value->buffer) < value->size) {
    return TPM_RC_INSUFFICIENT;
  }
  for (uint32_t i = 0; i < value->size; ++i) {
    result = Parse_BYTE(
        buffer,
        &value->buffer[i],
        value_bytes);
    if (result) {
      return result;
    }
  }
  return result;
}

TPM2B_SYM_KEY Make_TPM2B_SYM_KEY(
    const std::string& bytes) {
  TPM2B_SYM_KEY tpm2b;
  CHECK(bytes.size() <= sizeof(tpm2b.buffer));
  memset(&tpm2b, 0, sizeof(TPM2B_SYM_KEY));
  tpm2b.size = bytes.size();
  memcpy(tpm2b.buffer, bytes.data(), bytes.size());
  return tpm2b;
}

std::string StringFrom_TPM2B_SYM_KEY(
    const TPM2B_SYM_KEY& tpm2b) {
  const char* char_buffer = reinterpret_cast<const char*>(
      tpm2b.buffer);
  return std::string(char_buffer, tpm2b.size);
}

TPM_RC Serialize_TPMS_SYMCIPHER_PARMS(
    const TPMS_SYMCIPHER_PARMS& value,
    std::string* buffer) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Serialize_TPMT_SYM_DEF_OBJECT(value.sym, buffer);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Parse_TPMS_SYMCIPHER_PARMS(
    std::string* buffer,
    TPMS_SYMCIPHER_PARMS* value,
    std::string* value_bytes) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Parse_TPMT_SYM_DEF_OBJECT(
      buffer,
      &value->sym,
      value_bytes);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Serialize_TPM2B_SENSITIVE_DATA(
    const TPM2B_SENSITIVE_DATA& value,
    std::string* buffer) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Serialize_UINT16(value.size, buffer);
  if (result) {
    return result;
  }

  if (arraysize(value.buffer) < value.size) {
    return TPM_RC_INSUFFICIENT;
  }
  for (uint32_t i = 0; i < value.size; ++i) {
    result = Serialize_BYTE(value.buffer[i], buffer);
    if (result) {
      return result;
    }
  }
  return result;
}

TPM_RC Parse_TPM2B_SENSITIVE_DATA(
    std::string* buffer,
    TPM2B_SENSITIVE_DATA* value,
    std::string* value_bytes) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Parse_UINT16(
      buffer,
      &value->size,
      value_bytes);
  if (result) {
    return result;
  }

  if (arraysize(value->buffer) < value->size) {
    return TPM_RC_INSUFFICIENT;
  }
  for (uint32_t i = 0; i < value->size; ++i) {
    result = Parse_BYTE(
        buffer,
        &value->buffer[i],
        value_bytes);
    if (result) {
      return result;
    }
  }
  return result;
}

TPM2B_SENSITIVE_DATA Make_TPM2B_SENSITIVE_DATA(
    const std::string& bytes) {
  TPM2B_SENSITIVE_DATA tpm2b;
  CHECK(bytes.size() <= sizeof(tpm2b.buffer));
  memset(&tpm2b, 0, sizeof(TPM2B_SENSITIVE_DATA));
  tpm2b.size = bytes.size();
  memcpy(tpm2b.buffer, bytes.data(), bytes.size());
  return tpm2b;
}

std::string StringFrom_TPM2B_SENSITIVE_DATA(
    const TPM2B_SENSITIVE_DATA& tpm2b) {
  const char* char_buffer = reinterpret_cast<const char*>(
      tpm2b.buffer);
  return std::string(char_buffer, tpm2b.size);
}

TPM_RC Serialize_TPMS_SENSITIVE_CREATE(
    const TPMS_SENSITIVE_CREATE& value,
    std::string* buffer) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Serialize_TPM2B_AUTH(value.user_auth, buffer);
  if (result) {
    return result;
  }

  result = Serialize_TPM2B_SENSITIVE_DATA(value.data, buffer);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Parse_TPMS_SENSITIVE_CREATE(
    std::string* buffer,
    TPMS_SENSITIVE_CREATE* value,
    std::string* value_bytes) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Parse_TPM2B_AUTH(
      buffer,
      &value->user_auth,
      value_bytes);
  if (result) {
    return result;
  }

  result = Parse_TPM2B_SENSITIVE_DATA(
      buffer,
      &value->data,
      value_bytes);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Serialize_TPM2B_SENSITIVE_CREATE(
    const TPM2B_SENSITIVE_CREATE& value,
    std::string* buffer) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Serialize_UINT16(value.size, buffer);
  if (result) {
    return result;
  }

  result = Serialize_TPMS_SENSITIVE_CREATE(value.sensitive, buffer);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Parse_TPM2B_SENSITIVE_CREATE(
    std::string* buffer,
    TPM2B_SENSITIVE_CREATE* value,
    std::string* value_bytes) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Parse_UINT16(
      buffer,
      &value->size,
      value_bytes);
  if (result) {
    return result;
  }

  result = Parse_TPMS_SENSITIVE_CREATE(
      buffer,
      &value->sensitive,
      value_bytes);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Serialize_TPMS_SCHEME_XOR(
    const TPMS_SCHEME_XOR& value,
    std::string* buffer) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Serialize_TPMI_ALG_HASH(value.hash_alg, buffer);
  if (result) {
    return result;
  }

  result = Serialize_TPMI_ALG_KDF(value.kdf, buffer);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Parse_TPMS_SCHEME_XOR(
    std::string* buffer,
    TPMS_SCHEME_XOR* value,
    std::string* value_bytes) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Parse_TPMI_ALG_HASH(
      buffer,
      &value->hash_alg,
      value_bytes);
  if (result) {
    return result;
  }

  result = Parse_TPMI_ALG_KDF(
      buffer,
      &value->kdf,
      value_bytes);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Serialize_TPMU_SCHEME_KEYEDHASH(
    const TPMU_SCHEME_KEYEDHASH& value,
    TPMI_ALG_KEYEDHASH_SCHEME selector,
    std::string* buffer) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  if (selector == TPM_ALG_NULL) {
    // Do nothing.
  }

  if (selector == TPM_ALG_HMAC) {
    result = Serialize_TPMS_SCHEME_HMAC(value.hmac, buffer);
    if (result) {
      return result;
    }
  }

  if (selector == TPM_ALG_XOR) {
    result = Serialize_TPMS_SCHEME_XOR(value.xor_, buffer);
    if (result) {
      return result;
    }
  }
  return result;
}

TPM_RC Parse_TPMU_SCHEME_KEYEDHASH(
    std::string* buffer,
    TPMI_ALG_KEYEDHASH_SCHEME selector,
    TPMU_SCHEME_KEYEDHASH* value,
    std::string* value_bytes) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  if (selector == TPM_ALG_NULL) {
    // Do nothing.
  }

  if (selector == TPM_ALG_HMAC) {
    result = Parse_TPMS_SCHEME_HMAC(
        buffer,
        &value->hmac,
        value_bytes);
    if (result) {
      return result;
    }
  }

  if (selector == TPM_ALG_XOR) {
    result = Parse_TPMS_SCHEME_XOR(
        buffer,
        &value->xor_,
        value_bytes);
    if (result) {
      return result;
    }
  }
  return result;
}

TPM_RC Serialize_TPMT_KEYEDHASH_SCHEME(
    const TPMT_KEYEDHASH_SCHEME& value,
    std::string* buffer) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Serialize_TPMI_ALG_KEYEDHASH_SCHEME(value.scheme, buffer);
  if (result) {
    return result;
  }

  result = Serialize_TPMU_SCHEME_KEYEDHASH(
      value.details,
      value.scheme,
      buffer);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Parse_TPMT_KEYEDHASH_SCHEME(
    std::string* buffer,
    TPMT_KEYEDHASH_SCHEME* value,
    std::string* value_bytes) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Parse_TPMI_ALG_KEYEDHASH_SCHEME(
      buffer,
      &value->scheme,
      value_bytes);
  if (result) {
    return result;
  }

  result = Parse_TPMU_SCHEME_KEYEDHASH(
      buffer,
      value->scheme,
      &value->details,
      value_bytes);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Serialize_TPMS_SCHEME_ECDAA(
    const TPMS_SCHEME_ECDAA& value,
    std::string* buffer) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Serialize_TPMI_ALG_HASH(value.hash_alg, buffer);
  if (result) {
    return result;
  }

  result = Serialize_UINT16(value.count, buffer);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Parse_TPMS_SCHEME_ECDAA(
    std::string* buffer,
    TPMS_SCHEME_ECDAA* value,
    std::string* value_bytes) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Parse_TPMI_ALG_HASH(
      buffer,
      &value->hash_alg,
      value_bytes);
  if (result) {
    return result;
  }

  result = Parse_UINT16(
      buffer,
      &value->count,
      value_bytes);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Serialize_TPMU_SIG_SCHEME(
    const TPMU_SIG_SCHEME& value,
    TPMI_ALG_SIG_SCHEME selector,
    std::string* buffer) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  if (selector == TPM_ALG_HMAC) {
    result = Serialize_TPMS_SCHEME_HMAC(value.hmac, buffer);
    if (result) {
      return result;
    }
  }

  if (selector == TPM_ALG_ECSCHNORR) {
    result = Serialize_TPMS_SCHEME_ECSCHNORR(value.ec_schnorr, buffer);
    if (result) {
      return result;
    }
  }

  if (selector == TPM_ALG_RSAPSS) {
    result = Serialize_TPMS_SCHEME_RSAPSS(value.rsapss, buffer);
    if (result) {
      return result;
    }
  }

  if (selector == TPM_ALG_ECDAA) {
    result = Serialize_TPMS_SCHEME_ECDAA(value.ecdaa, buffer);
    if (result) {
      return result;
    }
  }

  if (selector == TPM_ALG_RSASSA) {
    result = Serialize_TPMS_SCHEME_RSASSA(value.rsassa, buffer);
    if (result) {
      return result;
    }
  }

  if (selector == TPM_ALG_SM2) {
    result = Serialize_TPMS_SCHEME_SM2(value.sm2, buffer);
    if (result) {
      return result;
    }
  }

  if (selector == TPM_ALG_ECDSA) {
    result = Serialize_TPMS_SCHEME_ECDSA(value.ecdsa, buffer);
    if (result) {
      return result;
    }
  }

  if (selector == TPM_ALG_NULL) {
    // Do nothing.
  }
  return result;
}

TPM_RC Parse_TPMU_SIG_SCHEME(
    std::string* buffer,
    TPMI_ALG_SIG_SCHEME selector,
    TPMU_SIG_SCHEME* value,
    std::string* value_bytes) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  if (selector == TPM_ALG_HMAC) {
    result = Parse_TPMS_SCHEME_HMAC(
        buffer,
        &value->hmac,
        value_bytes);
    if (result) {
      return result;
    }
  }

  if (selector == TPM_ALG_ECSCHNORR) {
    result = Parse_TPMS_SCHEME_ECSCHNORR(
        buffer,
        &value->ec_schnorr,
        value_bytes);
    if (result) {
      return result;
    }
  }

  if (selector == TPM_ALG_RSAPSS) {
    result = Parse_TPMS_SCHEME_RSAPSS(
        buffer,
        &value->rsapss,
        value_bytes);
    if (result) {
      return result;
    }
  }

  if (selector == TPM_ALG_ECDAA) {
    result = Parse_TPMS_SCHEME_ECDAA(
        buffer,
        &value->ecdaa,
        value_bytes);
    if (result) {
      return result;
    }
  }

  if (selector == TPM_ALG_RSASSA) {
    result = Parse_TPMS_SCHEME_RSASSA(
        buffer,
        &value->rsassa,
        value_bytes);
    if (result) {
      return result;
    }
  }

  if (selector == TPM_ALG_SM2) {
    result = Parse_TPMS_SCHEME_SM2(
        buffer,
        &value->sm2,
        value_bytes);
    if (result) {
      return result;
    }
  }

  if (selector == TPM_ALG_ECDSA) {
    result = Parse_TPMS_SCHEME_ECDSA(
        buffer,
        &value->ecdsa,
        value_bytes);
    if (result) {
      return result;
    }
  }

  if (selector == TPM_ALG_NULL) {
    // Do nothing.
  }
  return result;
}

TPM_RC Serialize_TPMT_SIG_SCHEME(
    const TPMT_SIG_SCHEME& value,
    std::string* buffer) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Serialize_TPMI_ALG_SIG_SCHEME(value.scheme, buffer);
  if (result) {
    return result;
  }

  result = Serialize_TPMU_SIG_SCHEME(
      value.details,
      value.scheme,
      buffer);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Parse_TPMT_SIG_SCHEME(
    std::string* buffer,
    TPMT_SIG_SCHEME* value,
    std::string* value_bytes) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Parse_TPMI_ALG_SIG_SCHEME(
      buffer,
      &value->scheme,
      value_bytes);
  if (result) {
    return result;
  }

  result = Parse_TPMU_SIG_SCHEME(
      buffer,
      value->scheme,
      &value->details,
      value_bytes);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Serialize_TPMS_SCHEME_OAEP(
    const TPMS_SCHEME_OAEP& value,
    std::string* buffer) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Serialize_TPMI_ALG_HASH(value.hash_alg, buffer);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Parse_TPMS_SCHEME_OAEP(
    std::string* buffer,
    TPMS_SCHEME_OAEP* value,
    std::string* value_bytes) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Parse_TPMI_ALG_HASH(
      buffer,
      &value->hash_alg,
      value_bytes);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Serialize_TPMS_SCHEME_ECDH(
    const TPMS_SCHEME_ECDH& value,
    std::string* buffer) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Serialize_TPMI_ALG_HASH(value.hash_alg, buffer);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Parse_TPMS_SCHEME_ECDH(
    std::string* buffer,
    TPMS_SCHEME_ECDH* value,
    std::string* value_bytes) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Parse_TPMI_ALG_HASH(
      buffer,
      &value->hash_alg,
      value_bytes);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Serialize_TPMS_SCHEME_MGF1(
    const TPMS_SCHEME_MGF1& value,
    std::string* buffer) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Serialize_TPMI_ALG_HASH(value.hash_alg, buffer);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Parse_TPMS_SCHEME_MGF1(
    std::string* buffer,
    TPMS_SCHEME_MGF1* value,
    std::string* value_bytes) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Parse_TPMI_ALG_HASH(
      buffer,
      &value->hash_alg,
      value_bytes);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Serialize_TPMS_SCHEME_KDF1_SP800_56a(
    const TPMS_SCHEME_KDF1_SP800_56a& value,
    std::string* buffer) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Serialize_TPMI_ALG_HASH(value.hash_alg, buffer);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Parse_TPMS_SCHEME_KDF1_SP800_56a(
    std::string* buffer,
    TPMS_SCHEME_KDF1_SP800_56a* value,
    std::string* value_bytes) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Parse_TPMI_ALG_HASH(
      buffer,
      &value->hash_alg,
      value_bytes);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Serialize_TPMS_SCHEME_KDF2(
    const TPMS_SCHEME_KDF2& value,
    std::string* buffer) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Serialize_TPMI_ALG_HASH(value.hash_alg, buffer);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Parse_TPMS_SCHEME_KDF2(
    std::string* buffer,
    TPMS_SCHEME_KDF2* value,
    std::string* value_bytes) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Parse_TPMI_ALG_HASH(
      buffer,
      &value->hash_alg,
      value_bytes);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Serialize_TPMS_SCHEME_KDF1_SP800_108(
    const TPMS_SCHEME_KDF1_SP800_108& value,
    std::string* buffer) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Serialize_TPMI_ALG_HASH(value.hash_alg, buffer);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Parse_TPMS_SCHEME_KDF1_SP800_108(
    std::string* buffer,
    TPMS_SCHEME_KDF1_SP800_108* value,
    std::string* value_bytes) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Parse_TPMI_ALG_HASH(
      buffer,
      &value->hash_alg,
      value_bytes);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Serialize_TPMU_KDF_SCHEME(
    const TPMU_KDF_SCHEME& value,
    TPMI_ALG_KDF selector,
    std::string* buffer) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  if (selector == TPM_ALG_KDF1_SP800_56a) {
    result = Serialize_TPMS_SCHEME_KDF1_SP800_56a(value.kdf1_sp800_56a, buffer);
    if (result) {
      return result;
    }
  }

  if (selector == TPM_ALG_MGF1) {
    result = Serialize_TPMS_SCHEME_MGF1(value.mgf1, buffer);
    if (result) {
      return result;
    }
  }

  if (selector == TPM_ALG_KDF1_SP800_108) {
    result = Serialize_TPMS_SCHEME_KDF1_SP800_108(value.kdf1_sp800_108, buffer);
    if (result) {
      return result;
    }
  }

  if (selector == TPM_ALG_KDF2) {
    result = Serialize_TPMS_SCHEME_KDF2(value.kdf2, buffer);
    if (result) {
      return result;
    }
  }

  if (selector == TPM_ALG_NULL) {
    // Do nothing.
  }
  return result;
}

TPM_RC Parse_TPMU_KDF_SCHEME(
    std::string* buffer,
    TPMI_ALG_KDF selector,
    TPMU_KDF_SCHEME* value,
    std::string* value_bytes) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  if (selector == TPM_ALG_KDF1_SP800_56a) {
    result = Parse_TPMS_SCHEME_KDF1_SP800_56a(
        buffer,
        &value->kdf1_sp800_56a,
        value_bytes);
    if (result) {
      return result;
    }
  }

  if (selector == TPM_ALG_MGF1) {
    result = Parse_TPMS_SCHEME_MGF1(
        buffer,
        &value->mgf1,
        value_bytes);
    if (result) {
      return result;
    }
  }

  if (selector == TPM_ALG_KDF1_SP800_108) {
    result = Parse_TPMS_SCHEME_KDF1_SP800_108(
        buffer,
        &value->kdf1_sp800_108,
        value_bytes);
    if (result) {
      return result;
    }
  }

  if (selector == TPM_ALG_KDF2) {
    result = Parse_TPMS_SCHEME_KDF2(
        buffer,
        &value->kdf2,
        value_bytes);
    if (result) {
      return result;
    }
  }

  if (selector == TPM_ALG_NULL) {
    // Do nothing.
  }
  return result;
}

TPM_RC Serialize_TPMT_KDF_SCHEME(
    const TPMT_KDF_SCHEME& value,
    std::string* buffer) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Serialize_TPMI_ALG_KDF(value.scheme, buffer);
  if (result) {
    return result;
  }

  result = Serialize_TPMU_KDF_SCHEME(
      value.details,
      value.scheme,
      buffer);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Parse_TPMT_KDF_SCHEME(
    std::string* buffer,
    TPMT_KDF_SCHEME* value,
    std::string* value_bytes) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Parse_TPMI_ALG_KDF(
      buffer,
      &value->scheme,
      value_bytes);
  if (result) {
    return result;
  }

  result = Parse_TPMU_KDF_SCHEME(
      buffer,
      value->scheme,
      &value->details,
      value_bytes);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Serialize_TPMU_ASYM_SCHEME(
    const TPMU_ASYM_SCHEME& value,
    TPMI_ALG_ASYM_SCHEME selector,
    std::string* buffer) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  if (selector == TPM_ALG_RSAES) {
    // Do nothing.
  }

  if (selector == TPM_ALG_ECSCHNORR) {
    result = Serialize_TPMS_SCHEME_ECSCHNORR(value.ec_schnorr, buffer);
    if (result) {
      return result;
    }
  }

  if (selector == TPM_ALG_NULL) {
    // Do nothing.
  }

  if (selector == TPM_ALG_ECDH) {
    result = Serialize_TPMS_SCHEME_ECDH(value.ecdh, buffer);
    if (result) {
      return result;
    }
  }

  if (selector == TPM_ALG_OAEP) {
    result = Serialize_TPMS_SCHEME_OAEP(value.oaep, buffer);
    if (result) {
      return result;
    }
  }

  if (selector == TPM_ALG_RSAPSS) {
    result = Serialize_TPMS_SCHEME_RSAPSS(value.rsapss, buffer);
    if (result) {
      return result;
    }
  }

  if (selector == TPM_ALG_ECDAA) {
    result = Serialize_TPMS_SCHEME_ECDAA(value.ecdaa, buffer);
    if (result) {
      return result;
    }
  }

  if (selector == TPM_ALG_RSASSA) {
    result = Serialize_TPMS_SCHEME_RSASSA(value.rsassa, buffer);
    if (result) {
      return result;
    }
  }

  if (selector == TPM_ALG_SM2) {
    result = Serialize_TPMS_SCHEME_SM2(value.sm2, buffer);
    if (result) {
      return result;
    }
  }

  if (selector == TPM_ALG_ECDSA) {
    result = Serialize_TPMS_SCHEME_ECDSA(value.ecdsa, buffer);
    if (result) {
      return result;
    }
  }
  return result;
}

TPM_RC Parse_TPMU_ASYM_SCHEME(
    std::string* buffer,
    TPMI_ALG_ASYM_SCHEME selector,
    TPMU_ASYM_SCHEME* value,
    std::string* value_bytes) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  if (selector == TPM_ALG_RSAES) {
    // Do nothing.
  }

  if (selector == TPM_ALG_ECSCHNORR) {
    result = Parse_TPMS_SCHEME_ECSCHNORR(
        buffer,
        &value->ec_schnorr,
        value_bytes);
    if (result) {
      return result;
    }
  }

  if (selector == TPM_ALG_NULL) {
    // Do nothing.
  }

  if (selector == TPM_ALG_ECDH) {
    result = Parse_TPMS_SCHEME_ECDH(
        buffer,
        &value->ecdh,
        value_bytes);
    if (result) {
      return result;
    }
  }

  if (selector == TPM_ALG_OAEP) {
    result = Parse_TPMS_SCHEME_OAEP(
        buffer,
        &value->oaep,
        value_bytes);
    if (result) {
      return result;
    }
  }

  if (selector == TPM_ALG_RSAPSS) {
    result = Parse_TPMS_SCHEME_RSAPSS(
        buffer,
        &value->rsapss,
        value_bytes);
    if (result) {
      return result;
    }
  }

  if (selector == TPM_ALG_ECDAA) {
    result = Parse_TPMS_SCHEME_ECDAA(
        buffer,
        &value->ecdaa,
        value_bytes);
    if (result) {
      return result;
    }
  }

  if (selector == TPM_ALG_RSASSA) {
    result = Parse_TPMS_SCHEME_RSASSA(
        buffer,
        &value->rsassa,
        value_bytes);
    if (result) {
      return result;
    }
  }

  if (selector == TPM_ALG_SM2) {
    result = Parse_TPMS_SCHEME_SM2(
        buffer,
        &value->sm2,
        value_bytes);
    if (result) {
      return result;
    }
  }

  if (selector == TPM_ALG_ECDSA) {
    result = Parse_TPMS_SCHEME_ECDSA(
        buffer,
        &value->ecdsa,
        value_bytes);
    if (result) {
      return result;
    }
  }
  return result;
}

TPM_RC Serialize_TPMT_ASYM_SCHEME(
    const TPMT_ASYM_SCHEME& value,
    std::string* buffer) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Serialize_TPMI_ALG_ASYM_SCHEME(value.scheme, buffer);
  if (result) {
    return result;
  }

  result = Serialize_TPMU_ASYM_SCHEME(
      value.details,
      value.scheme,
      buffer);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Parse_TPMT_ASYM_SCHEME(
    std::string* buffer,
    TPMT_ASYM_SCHEME* value,
    std::string* value_bytes) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Parse_TPMI_ALG_ASYM_SCHEME(
      buffer,
      &value->scheme,
      value_bytes);
  if (result) {
    return result;
  }

  result = Parse_TPMU_ASYM_SCHEME(
      buffer,
      value->scheme,
      &value->details,
      value_bytes);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Serialize_TPMT_RSA_SCHEME(
    const TPMT_RSA_SCHEME& value,
    std::string* buffer) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Serialize_TPMI_ALG_RSA_SCHEME(value.scheme, buffer);
  if (result) {
    return result;
  }

  result = Serialize_TPMU_ASYM_SCHEME(
      value.details,
      value.scheme,
      buffer);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Parse_TPMT_RSA_SCHEME(
    std::string* buffer,
    TPMT_RSA_SCHEME* value,
    std::string* value_bytes) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Parse_TPMI_ALG_RSA_SCHEME(
      buffer,
      &value->scheme,
      value_bytes);
  if (result) {
    return result;
  }

  result = Parse_TPMU_ASYM_SCHEME(
      buffer,
      value->scheme,
      &value->details,
      value_bytes);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Serialize_TPMT_RSA_DECRYPT(
    const TPMT_RSA_DECRYPT& value,
    std::string* buffer) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Serialize_TPMI_ALG_RSA_DECRYPT(value.scheme, buffer);
  if (result) {
    return result;
  }

  result = Serialize_TPMU_ASYM_SCHEME(
      value.details,
      value.scheme,
      buffer);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Parse_TPMT_RSA_DECRYPT(
    std::string* buffer,
    TPMT_RSA_DECRYPT* value,
    std::string* value_bytes) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Parse_TPMI_ALG_RSA_DECRYPT(
      buffer,
      &value->scheme,
      value_bytes);
  if (result) {
    return result;
  }

  result = Parse_TPMU_ASYM_SCHEME(
      buffer,
      value->scheme,
      &value->details,
      value_bytes);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Serialize_TPM2B_PUBLIC_KEY_RSA(
    const TPM2B_PUBLIC_KEY_RSA& value,
    std::string* buffer) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Serialize_UINT16(value.size, buffer);
  if (result) {
    return result;
  }

  if (arraysize(value.buffer) < value.size) {
    return TPM_RC_INSUFFICIENT;
  }
  for (uint32_t i = 0; i < value.size; ++i) {
    result = Serialize_BYTE(value.buffer[i], buffer);
    if (result) {
      return result;
    }
  }
  return result;
}

TPM_RC Parse_TPM2B_PUBLIC_KEY_RSA(
    std::string* buffer,
    TPM2B_PUBLIC_KEY_RSA* value,
    std::string* value_bytes) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Parse_UINT16(
      buffer,
      &value->size,
      value_bytes);
  if (result) {
    return result;
  }

  if (arraysize(value->buffer) < value->size) {
    return TPM_RC_INSUFFICIENT;
  }
  for (uint32_t i = 0; i < value->size; ++i) {
    result = Parse_BYTE(
        buffer,
        &value->buffer[i],
        value_bytes);
    if (result) {
      return result;
    }
  }
  return result;
}

TPM2B_PUBLIC_KEY_RSA Make_TPM2B_PUBLIC_KEY_RSA(
    const std::string& bytes) {
  TPM2B_PUBLIC_KEY_RSA tpm2b;
  CHECK(bytes.size() <= sizeof(tpm2b.buffer));
  memset(&tpm2b, 0, sizeof(TPM2B_PUBLIC_KEY_RSA));
  tpm2b.size = bytes.size();
  memcpy(tpm2b.buffer, bytes.data(), bytes.size());
  return tpm2b;
}

std::string StringFrom_TPM2B_PUBLIC_KEY_RSA(
    const TPM2B_PUBLIC_KEY_RSA& tpm2b) {
  const char* char_buffer = reinterpret_cast<const char*>(
      tpm2b.buffer);
  return std::string(char_buffer, tpm2b.size);
}

TPM_RC Serialize_TPM2B_PRIVATE_KEY_RSA(
    const TPM2B_PRIVATE_KEY_RSA& value,
    std::string* buffer) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Serialize_UINT16(value.size, buffer);
  if (result) {
    return result;
  }

  if (arraysize(value.buffer) < value.size) {
    return TPM_RC_INSUFFICIENT;
  }
  for (uint32_t i = 0; i < value.size; ++i) {
    result = Serialize_BYTE(value.buffer[i], buffer);
    if (result) {
      return result;
    }
  }
  return result;
}

TPM_RC Parse_TPM2B_PRIVATE_KEY_RSA(
    std::string* buffer,
    TPM2B_PRIVATE_KEY_RSA* value,
    std::string* value_bytes) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Parse_UINT16(
      buffer,
      &value->size,
      value_bytes);
  if (result) {
    return result;
  }

  if (arraysize(value->buffer) < value->size) {
    return TPM_RC_INSUFFICIENT;
  }
  for (uint32_t i = 0; i < value->size; ++i) {
    result = Parse_BYTE(
        buffer,
        &value->buffer[i],
        value_bytes);
    if (result) {
      return result;
    }
  }
  return result;
}

TPM2B_PRIVATE_KEY_RSA Make_TPM2B_PRIVATE_KEY_RSA(
    const std::string& bytes) {
  TPM2B_PRIVATE_KEY_RSA tpm2b;
  CHECK(bytes.size() <= sizeof(tpm2b.buffer));
  memset(&tpm2b, 0, sizeof(TPM2B_PRIVATE_KEY_RSA));
  tpm2b.size = bytes.size();
  memcpy(tpm2b.buffer, bytes.data(), bytes.size());
  return tpm2b;
}

std::string StringFrom_TPM2B_PRIVATE_KEY_RSA(
    const TPM2B_PRIVATE_KEY_RSA& tpm2b) {
  const char* char_buffer = reinterpret_cast<const char*>(
      tpm2b.buffer);
  return std::string(char_buffer, tpm2b.size);
}

TPM_RC Serialize_TPM2B_ECC_PARAMETER(
    const TPM2B_ECC_PARAMETER& value,
    std::string* buffer) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Serialize_UINT16(value.size, buffer);
  if (result) {
    return result;
  }

  if (arraysize(value.buffer) < value.size) {
    return TPM_RC_INSUFFICIENT;
  }
  for (uint32_t i = 0; i < value.size; ++i) {
    result = Serialize_BYTE(value.buffer[i], buffer);
    if (result) {
      return result;
    }
  }
  return result;
}

TPM_RC Parse_TPM2B_ECC_PARAMETER(
    std::string* buffer,
    TPM2B_ECC_PARAMETER* value,
    std::string* value_bytes) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Parse_UINT16(
      buffer,
      &value->size,
      value_bytes);
  if (result) {
    return result;
  }

  if (arraysize(value->buffer) < value->size) {
    return TPM_RC_INSUFFICIENT;
  }
  for (uint32_t i = 0; i < value->size; ++i) {
    result = Parse_BYTE(
        buffer,
        &value->buffer[i],
        value_bytes);
    if (result) {
      return result;
    }
  }
  return result;
}

TPM2B_ECC_PARAMETER Make_TPM2B_ECC_PARAMETER(
    const std::string& bytes) {
  TPM2B_ECC_PARAMETER tpm2b;
  CHECK(bytes.size() <= sizeof(tpm2b.buffer));
  memset(&tpm2b, 0, sizeof(TPM2B_ECC_PARAMETER));
  tpm2b.size = bytes.size();
  memcpy(tpm2b.buffer, bytes.data(), bytes.size());
  return tpm2b;
}

std::string StringFrom_TPM2B_ECC_PARAMETER(
    const TPM2B_ECC_PARAMETER& tpm2b) {
  const char* char_buffer = reinterpret_cast<const char*>(
      tpm2b.buffer);
  return std::string(char_buffer, tpm2b.size);
}

TPM_RC Serialize_TPMS_ECC_POINT(
    const TPMS_ECC_POINT& value,
    std::string* buffer) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Serialize_TPM2B_ECC_PARAMETER(value.x, buffer);
  if (result) {
    return result;
  }

  result = Serialize_TPM2B_ECC_PARAMETER(value.y, buffer);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Parse_TPMS_ECC_POINT(
    std::string* buffer,
    TPMS_ECC_POINT* value,
    std::string* value_bytes) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Parse_TPM2B_ECC_PARAMETER(
      buffer,
      &value->x,
      value_bytes);
  if (result) {
    return result;
  }

  result = Parse_TPM2B_ECC_PARAMETER(
      buffer,
      &value->y,
      value_bytes);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Serialize_TPM2B_ECC_POINT(
    const TPM2B_ECC_POINT& value,
    std::string* buffer) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Serialize_UINT16(value.size, buffer);
  if (result) {
    return result;
  }

  result = Serialize_TPMS_ECC_POINT(value.point, buffer);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Parse_TPM2B_ECC_POINT(
    std::string* buffer,
    TPM2B_ECC_POINT* value,
    std::string* value_bytes) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Parse_UINT16(
      buffer,
      &value->size,
      value_bytes);
  if (result) {
    return result;
  }

  result = Parse_TPMS_ECC_POINT(
      buffer,
      &value->point,
      value_bytes);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Serialize_TPMT_ECC_SCHEME(
    const TPMT_ECC_SCHEME& value,
    std::string* buffer) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Serialize_TPMI_ALG_ECC_SCHEME(value.scheme, buffer);
  if (result) {
    return result;
  }

  result = Serialize_TPMU_SIG_SCHEME(
      value.details,
      value.scheme,
      buffer);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Parse_TPMT_ECC_SCHEME(
    std::string* buffer,
    TPMT_ECC_SCHEME* value,
    std::string* value_bytes) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Parse_TPMI_ALG_ECC_SCHEME(
      buffer,
      &value->scheme,
      value_bytes);
  if (result) {
    return result;
  }

  result = Parse_TPMU_SIG_SCHEME(
      buffer,
      value->scheme,
      &value->details,
      value_bytes);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Serialize_TPMS_ALGORITHM_DETAIL_ECC(
    const TPMS_ALGORITHM_DETAIL_ECC& value,
    std::string* buffer) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Serialize_TPM_ECC_CURVE(value.curve_id, buffer);
  if (result) {
    return result;
  }

  result = Serialize_UINT16(value.key_size, buffer);
  if (result) {
    return result;
  }

  result = Serialize_TPMT_KDF_SCHEME(value.kdf, buffer);
  if (result) {
    return result;
  }

  result = Serialize_TPMT_ECC_SCHEME(value.sign, buffer);
  if (result) {
    return result;
  }

  result = Serialize_TPM2B_ECC_PARAMETER(value.p, buffer);
  if (result) {
    return result;
  }

  result = Serialize_TPM2B_ECC_PARAMETER(value.a, buffer);
  if (result) {
    return result;
  }

  result = Serialize_TPM2B_ECC_PARAMETER(value.b, buffer);
  if (result) {
    return result;
  }

  result = Serialize_TPM2B_ECC_PARAMETER(value.g_x, buffer);
  if (result) {
    return result;
  }

  result = Serialize_TPM2B_ECC_PARAMETER(value.g_y, buffer);
  if (result) {
    return result;
  }

  result = Serialize_TPM2B_ECC_PARAMETER(value.n, buffer);
  if (result) {
    return result;
  }

  result = Serialize_TPM2B_ECC_PARAMETER(value.h, buffer);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Parse_TPMS_ALGORITHM_DETAIL_ECC(
    std::string* buffer,
    TPMS_ALGORITHM_DETAIL_ECC* value,
    std::string* value_bytes) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Parse_TPM_ECC_CURVE(
      buffer,
      &value->curve_id,
      value_bytes);
  if (result) {
    return result;
  }

  result = Parse_UINT16(
      buffer,
      &value->key_size,
      value_bytes);
  if (result) {
    return result;
  }

  result = Parse_TPMT_KDF_SCHEME(
      buffer,
      &value->kdf,
      value_bytes);
  if (result) {
    return result;
  }

  result = Parse_TPMT_ECC_SCHEME(
      buffer,
      &value->sign,
      value_bytes);
  if (result) {
    return result;
  }

  result = Parse_TPM2B_ECC_PARAMETER(
      buffer,
      &value->p,
      value_bytes);
  if (result) {
    return result;
  }

  result = Parse_TPM2B_ECC_PARAMETER(
      buffer,
      &value->a,
      value_bytes);
  if (result) {
    return result;
  }

  result = Parse_TPM2B_ECC_PARAMETER(
      buffer,
      &value->b,
      value_bytes);
  if (result) {
    return result;
  }

  result = Parse_TPM2B_ECC_PARAMETER(
      buffer,
      &value->g_x,
      value_bytes);
  if (result) {
    return result;
  }

  result = Parse_TPM2B_ECC_PARAMETER(
      buffer,
      &value->g_y,
      value_bytes);
  if (result) {
    return result;
  }

  result = Parse_TPM2B_ECC_PARAMETER(
      buffer,
      &value->n,
      value_bytes);
  if (result) {
    return result;
  }

  result = Parse_TPM2B_ECC_PARAMETER(
      buffer,
      &value->h,
      value_bytes);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Serialize_TPMS_SIGNATURE_RSASSA(
    const TPMS_SIGNATURE_RSASSA& value,
    std::string* buffer) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Serialize_TPMI_ALG_HASH(value.hash, buffer);
  if (result) {
    return result;
  }

  result = Serialize_TPM2B_PUBLIC_KEY_RSA(value.sig, buffer);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Parse_TPMS_SIGNATURE_RSASSA(
    std::string* buffer,
    TPMS_SIGNATURE_RSASSA* value,
    std::string* value_bytes) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Parse_TPMI_ALG_HASH(
      buffer,
      &value->hash,
      value_bytes);
  if (result) {
    return result;
  }

  result = Parse_TPM2B_PUBLIC_KEY_RSA(
      buffer,
      &value->sig,
      value_bytes);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Serialize_TPMS_SIGNATURE_RSAPSS(
    const TPMS_SIGNATURE_RSAPSS& value,
    std::string* buffer) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Serialize_TPMI_ALG_HASH(value.hash, buffer);
  if (result) {
    return result;
  }

  result = Serialize_TPM2B_PUBLIC_KEY_RSA(value.sig, buffer);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Parse_TPMS_SIGNATURE_RSAPSS(
    std::string* buffer,
    TPMS_SIGNATURE_RSAPSS* value,
    std::string* value_bytes) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Parse_TPMI_ALG_HASH(
      buffer,
      &value->hash,
      value_bytes);
  if (result) {
    return result;
  }

  result = Parse_TPM2B_PUBLIC_KEY_RSA(
      buffer,
      &value->sig,
      value_bytes);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Serialize_TPMS_SIGNATURE_ECDSA(
    const TPMS_SIGNATURE_ECDSA& value,
    std::string* buffer) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Serialize_TPMI_ALG_HASH(value.hash, buffer);
  if (result) {
    return result;
  }

  result = Serialize_TPM2B_ECC_PARAMETER(value.signature_r, buffer);
  if (result) {
    return result;
  }

  result = Serialize_TPM2B_ECC_PARAMETER(value.signature_s, buffer);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Parse_TPMS_SIGNATURE_ECDSA(
    std::string* buffer,
    TPMS_SIGNATURE_ECDSA* value,
    std::string* value_bytes) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Parse_TPMI_ALG_HASH(
      buffer,
      &value->hash,
      value_bytes);
  if (result) {
    return result;
  }

  result = Parse_TPM2B_ECC_PARAMETER(
      buffer,
      &value->signature_r,
      value_bytes);
  if (result) {
    return result;
  }

  result = Parse_TPM2B_ECC_PARAMETER(
      buffer,
      &value->signature_s,
      value_bytes);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Serialize_TPMU_SIGNATURE(
    const TPMU_SIGNATURE& value,
    TPMI_ALG_SIG_SCHEME selector,
    std::string* buffer) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  if (selector == TPM_ALG_HMAC) {
    result = Serialize_TPMT_HA(value.hmac, buffer);
    if (result) {
      return result;
    }
  }

  if (selector == TPM_ALG_ECSCHNORR) {
    result = Serialize_TPMS_SIGNATURE_ECDSA(value.ecschnorr, buffer);
    if (result) {
      return result;
    }
  }

  if (selector == TPM_ALG_RSAPSS) {
    result = Serialize_TPMS_SIGNATURE_RSAPSS(value.rsapss, buffer);
    if (result) {
      return result;
    }
  }

  if (selector == TPM_ALG_ECDAA) {
    result = Serialize_TPMS_SIGNATURE_ECDSA(value.ecdaa, buffer);
    if (result) {
      return result;
    }
  }

  if (selector == TPM_ALG_RSASSA) {
    result = Serialize_TPMS_SIGNATURE_RSASSA(value.rsassa, buffer);
    if (result) {
      return result;
    }
  }

  if (selector == TPM_ALG_SM2) {
    result = Serialize_TPMS_SIGNATURE_ECDSA(value.sm2, buffer);
    if (result) {
      return result;
    }
  }

  if (selector == TPM_ALG_ECDSA) {
    result = Serialize_TPMS_SIGNATURE_ECDSA(value.ecdsa, buffer);
    if (result) {
      return result;
    }
  }

  if (selector == TPM_ALG_NULL) {
    // Do nothing.
  }
  return result;
}

TPM_RC Parse_TPMU_SIGNATURE(
    std::string* buffer,
    TPMI_ALG_SIG_SCHEME selector,
    TPMU_SIGNATURE* value,
    std::string* value_bytes) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  if (selector == TPM_ALG_HMAC) {
    result = Parse_TPMT_HA(
        buffer,
        &value->hmac,
        value_bytes);
    if (result) {
      return result;
    }
  }

  if (selector == TPM_ALG_ECSCHNORR) {
    result = Parse_TPMS_SIGNATURE_ECDSA(
        buffer,
        &value->ecschnorr,
        value_bytes);
    if (result) {
      return result;
    }
  }

  if (selector == TPM_ALG_RSAPSS) {
    result = Parse_TPMS_SIGNATURE_RSAPSS(
        buffer,
        &value->rsapss,
        value_bytes);
    if (result) {
      return result;
    }
  }

  if (selector == TPM_ALG_ECDAA) {
    result = Parse_TPMS_SIGNATURE_ECDSA(
        buffer,
        &value->ecdaa,
        value_bytes);
    if (result) {
      return result;
    }
  }

  if (selector == TPM_ALG_RSASSA) {
    result = Parse_TPMS_SIGNATURE_RSASSA(
        buffer,
        &value->rsassa,
        value_bytes);
    if (result) {
      return result;
    }
  }

  if (selector == TPM_ALG_SM2) {
    result = Parse_TPMS_SIGNATURE_ECDSA(
        buffer,
        &value->sm2,
        value_bytes);
    if (result) {
      return result;
    }
  }

  if (selector == TPM_ALG_ECDSA) {
    result = Parse_TPMS_SIGNATURE_ECDSA(
        buffer,
        &value->ecdsa,
        value_bytes);
    if (result) {
      return result;
    }
  }

  if (selector == TPM_ALG_NULL) {
    // Do nothing.
  }
  return result;
}

TPM_RC Serialize_TPMT_SIGNATURE(
    const TPMT_SIGNATURE& value,
    std::string* buffer) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Serialize_TPMI_ALG_SIG_SCHEME(value.sig_alg, buffer);
  if (result) {
    return result;
  }

  result = Serialize_TPMU_SIGNATURE(
      value.signature,
      value.sig_alg,
      buffer);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Parse_TPMT_SIGNATURE(
    std::string* buffer,
    TPMT_SIGNATURE* value,
    std::string* value_bytes) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Parse_TPMI_ALG_SIG_SCHEME(
      buffer,
      &value->sig_alg,
      value_bytes);
  if (result) {
    return result;
  }

  result = Parse_TPMU_SIGNATURE(
      buffer,
      value->sig_alg,
      &value->signature,
      value_bytes);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Serialize_TPM2B_ENCRYPTED_SECRET(
    const TPM2B_ENCRYPTED_SECRET& value,
    std::string* buffer) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Serialize_UINT16(value.size, buffer);
  if (result) {
    return result;
  }

  if (arraysize(value.secret) < value.size) {
    return TPM_RC_INSUFFICIENT;
  }
  for (uint32_t i = 0; i < value.size; ++i) {
    result = Serialize_BYTE(value.secret[i], buffer);
    if (result) {
      return result;
    }
  }
  return result;
}

TPM_RC Parse_TPM2B_ENCRYPTED_SECRET(
    std::string* buffer,
    TPM2B_ENCRYPTED_SECRET* value,
    std::string* value_bytes) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Parse_UINT16(
      buffer,
      &value->size,
      value_bytes);
  if (result) {
    return result;
  }

  if (arraysize(value->secret) < value->size) {
    return TPM_RC_INSUFFICIENT;
  }
  for (uint32_t i = 0; i < value->size; ++i) {
    result = Parse_BYTE(
        buffer,
        &value->secret[i],
        value_bytes);
    if (result) {
      return result;
    }
  }
  return result;
}

TPM2B_ENCRYPTED_SECRET Make_TPM2B_ENCRYPTED_SECRET(
    const std::string& bytes) {
  TPM2B_ENCRYPTED_SECRET tpm2b;
  CHECK(bytes.size() <= sizeof(tpm2b.secret));
  memset(&tpm2b, 0, sizeof(TPM2B_ENCRYPTED_SECRET));
  tpm2b.size = bytes.size();
  memcpy(tpm2b.secret, bytes.data(), bytes.size());
  return tpm2b;
}

std::string StringFrom_TPM2B_ENCRYPTED_SECRET(
    const TPM2B_ENCRYPTED_SECRET& tpm2b) {
  const char* char_buffer = reinterpret_cast<const char*>(
      tpm2b.secret);
  return std::string(char_buffer, tpm2b.size);
}

TPM_RC Serialize_TPMS_KEYEDHASH_PARMS(
    const TPMS_KEYEDHASH_PARMS& value,
    std::string* buffer) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Serialize_TPMT_KEYEDHASH_SCHEME(value.scheme, buffer);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Parse_TPMS_KEYEDHASH_PARMS(
    std::string* buffer,
    TPMS_KEYEDHASH_PARMS* value,
    std::string* value_bytes) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Parse_TPMT_KEYEDHASH_SCHEME(
      buffer,
      &value->scheme,
      value_bytes);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Serialize_TPMS_ASYM_PARMS(
    const TPMS_ASYM_PARMS& value,
    std::string* buffer) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Serialize_TPMT_SYM_DEF_OBJECT(value.symmetric, buffer);
  if (result) {
    return result;
  }

  result = Serialize_TPMT_ASYM_SCHEME(value.scheme, buffer);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Parse_TPMS_ASYM_PARMS(
    std::string* buffer,
    TPMS_ASYM_PARMS* value,
    std::string* value_bytes) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Parse_TPMT_SYM_DEF_OBJECT(
      buffer,
      &value->symmetric,
      value_bytes);
  if (result) {
    return result;
  }

  result = Parse_TPMT_ASYM_SCHEME(
      buffer,
      &value->scheme,
      value_bytes);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Serialize_TPMS_RSA_PARMS(
    const TPMS_RSA_PARMS& value,
    std::string* buffer) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Serialize_TPMT_SYM_DEF_OBJECT(value.symmetric, buffer);
  if (result) {
    return result;
  }

  result = Serialize_TPMT_RSA_SCHEME(value.scheme, buffer);
  if (result) {
    return result;
  }

  result = Serialize_TPMI_RSA_KEY_BITS(value.key_bits, buffer);
  if (result) {
    return result;
  }

  result = Serialize_UINT32(value.exponent, buffer);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Parse_TPMS_RSA_PARMS(
    std::string* buffer,
    TPMS_RSA_PARMS* value,
    std::string* value_bytes) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Parse_TPMT_SYM_DEF_OBJECT(
      buffer,
      &value->symmetric,
      value_bytes);
  if (result) {
    return result;
  }

  result = Parse_TPMT_RSA_SCHEME(
      buffer,
      &value->scheme,
      value_bytes);
  if (result) {
    return result;
  }

  result = Parse_TPMI_RSA_KEY_BITS(
      buffer,
      &value->key_bits,
      value_bytes);
  if (result) {
    return result;
  }

  result = Parse_UINT32(
      buffer,
      &value->exponent,
      value_bytes);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Serialize_TPMS_ECC_PARMS(
    const TPMS_ECC_PARMS& value,
    std::string* buffer) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Serialize_TPMT_SYM_DEF_OBJECT(value.symmetric, buffer);
  if (result) {
    return result;
  }

  result = Serialize_TPMT_ECC_SCHEME(value.scheme, buffer);
  if (result) {
    return result;
  }

  result = Serialize_TPMI_ECC_CURVE(value.curve_id, buffer);
  if (result) {
    return result;
  }

  result = Serialize_TPMT_KDF_SCHEME(value.kdf, buffer);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Parse_TPMS_ECC_PARMS(
    std::string* buffer,
    TPMS_ECC_PARMS* value,
    std::string* value_bytes) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Parse_TPMT_SYM_DEF_OBJECT(
      buffer,
      &value->symmetric,
      value_bytes);
  if (result) {
    return result;
  }

  result = Parse_TPMT_ECC_SCHEME(
      buffer,
      &value->scheme,
      value_bytes);
  if (result) {
    return result;
  }

  result = Parse_TPMI_ECC_CURVE(
      buffer,
      &value->curve_id,
      value_bytes);
  if (result) {
    return result;
  }

  result = Parse_TPMT_KDF_SCHEME(
      buffer,
      &value->kdf,
      value_bytes);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Serialize_TPMU_PUBLIC_PARMS(
    const TPMU_PUBLIC_PARMS& value,
    TPMI_ALG_PUBLIC selector,
    std::string* buffer) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  if (selector == TPM_ALG_KEYEDHASH) {
    result = Serialize_TPMS_KEYEDHASH_PARMS(value.keyed_hash_detail, buffer);
    if (result) {
      return result;
    }
  }

  if (selector == TPM_ALG_RSA) {
    result = Serialize_TPMS_RSA_PARMS(value.rsa_detail, buffer);
    if (result) {
      return result;
    }
  }

  if (selector == TPM_ALG_SYMCIPHER) {
    result = Serialize_TPMS_SYMCIPHER_PARMS(value.sym_detail, buffer);
    if (result) {
      return result;
    }
  }

  if (selector == TPM_ALG_ECC) {
    result = Serialize_TPMS_ECC_PARMS(value.ecc_detail, buffer);
    if (result) {
      return result;
    }
  }
  return result;
}

TPM_RC Parse_TPMU_PUBLIC_PARMS(
    std::string* buffer,
    TPMI_ALG_PUBLIC selector,
    TPMU_PUBLIC_PARMS* value,
    std::string* value_bytes) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  if (selector == TPM_ALG_KEYEDHASH) {
    result = Parse_TPMS_KEYEDHASH_PARMS(
        buffer,
        &value->keyed_hash_detail,
        value_bytes);
    if (result) {
      return result;
    }
  }

  if (selector == TPM_ALG_RSA) {
    result = Parse_TPMS_RSA_PARMS(
        buffer,
        &value->rsa_detail,
        value_bytes);
    if (result) {
      return result;
    }
  }

  if (selector == TPM_ALG_SYMCIPHER) {
    result = Parse_TPMS_SYMCIPHER_PARMS(
        buffer,
        &value->sym_detail,
        value_bytes);
    if (result) {
      return result;
    }
  }

  if (selector == TPM_ALG_ECC) {
    result = Parse_TPMS_ECC_PARMS(
        buffer,
        &value->ecc_detail,
        value_bytes);
    if (result) {
      return result;
    }
  }
  return result;
}

TPM_RC Serialize_TPMT_PUBLIC_PARMS(
    const TPMT_PUBLIC_PARMS& value,
    std::string* buffer) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Serialize_TPMI_ALG_PUBLIC(value.type, buffer);
  if (result) {
    return result;
  }

  result = Serialize_TPMU_PUBLIC_PARMS(
      value.parameters,
      value.type,
      buffer);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Parse_TPMT_PUBLIC_PARMS(
    std::string* buffer,
    TPMT_PUBLIC_PARMS* value,
    std::string* value_bytes) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Parse_TPMI_ALG_PUBLIC(
      buffer,
      &value->type,
      value_bytes);
  if (result) {
    return result;
  }

  result = Parse_TPMU_PUBLIC_PARMS(
      buffer,
      value->type,
      &value->parameters,
      value_bytes);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Serialize_TPMU_PUBLIC_ID(
    const TPMU_PUBLIC_ID& value,
    TPMI_ALG_PUBLIC selector,
    std::string* buffer) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  if (selector == TPM_ALG_KEYEDHASH) {
    result = Serialize_TPM2B_DIGEST(value.keyed_hash, buffer);
    if (result) {
      return result;
    }
  }

  if (selector == TPM_ALG_RSA) {
    result = Serialize_TPM2B_PUBLIC_KEY_RSA(value.rsa, buffer);
    if (result) {
      return result;
    }
  }

  if (selector == TPM_ALG_SYMCIPHER) {
    result = Serialize_TPM2B_DIGEST(value.sym, buffer);
    if (result) {
      return result;
    }
  }

  if (selector == TPM_ALG_ECC) {
    result = Serialize_TPMS_ECC_POINT(value.ecc, buffer);
    if (result) {
      return result;
    }
  }
  return result;
}

TPM_RC Parse_TPMU_PUBLIC_ID(
    std::string* buffer,
    TPMI_ALG_PUBLIC selector,
    TPMU_PUBLIC_ID* value,
    std::string* value_bytes) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  if (selector == TPM_ALG_KEYEDHASH) {
    result = Parse_TPM2B_DIGEST(
        buffer,
        &value->keyed_hash,
        value_bytes);
    if (result) {
      return result;
    }
  }

  if (selector == TPM_ALG_RSA) {
    result = Parse_TPM2B_PUBLIC_KEY_RSA(
        buffer,
        &value->rsa,
        value_bytes);
    if (result) {
      return result;
    }
  }

  if (selector == TPM_ALG_SYMCIPHER) {
    result = Parse_TPM2B_DIGEST(
        buffer,
        &value->sym,
        value_bytes);
    if (result) {
      return result;
    }
  }

  if (selector == TPM_ALG_ECC) {
    result = Parse_TPMS_ECC_POINT(
        buffer,
        &value->ecc,
        value_bytes);
    if (result) {
      return result;
    }
  }
  return result;
}

TPM_RC Serialize_TPMT_PUBLIC(
    const TPMT_PUBLIC& value,
    std::string* buffer) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Serialize_TPMI_ALG_PUBLIC(value.type, buffer);
  if (result) {
    return result;
  }

  result = Serialize_TPMI_ALG_HASH(value.name_alg, buffer);
  if (result) {
    return result;
  }

  result = Serialize_TPMA_OBJECT(value.object_attributes, buffer);
  if (result) {
    return result;
  }

  result = Serialize_TPM2B_DIGEST(value.auth_policy, buffer);
  if (result) {
    return result;
  }

  result = Serialize_TPMU_PUBLIC_ID(
      value.unique,
      value.type,
      buffer);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Parse_TPMT_PUBLIC(
    std::string* buffer,
    TPMT_PUBLIC* value,
    std::string* value_bytes) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Parse_TPMI_ALG_PUBLIC(
      buffer,
      &value->type,
      value_bytes);
  if (result) {
    return result;
  }

  result = Parse_TPMI_ALG_HASH(
      buffer,
      &value->name_alg,
      value_bytes);
  if (result) {
    return result;
  }

  result = Parse_TPMA_OBJECT(
      buffer,
      &value->object_attributes,
      value_bytes);
  if (result) {
    return result;
  }

  result = Parse_TPM2B_DIGEST(
      buffer,
      &value->auth_policy,
      value_bytes);
  if (result) {
    return result;
  }

  result = Parse_TPMU_PUBLIC_ID(
      buffer,
      value->type,
      &value->unique,
      value_bytes);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Serialize_TPM2B_PUBLIC(
    const TPM2B_PUBLIC& value,
    std::string* buffer) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Serialize_UINT16(value.size, buffer);
  if (result) {
    return result;
  }

  result = Serialize_TPMT_PUBLIC(value.public_area, buffer);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Parse_TPM2B_PUBLIC(
    std::string* buffer,
    TPM2B_PUBLIC* value,
    std::string* value_bytes) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Parse_UINT16(
      buffer,
      &value->size,
      value_bytes);
  if (result) {
    return result;
  }

  result = Parse_TPMT_PUBLIC(
      buffer,
      &value->public_area,
      value_bytes);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Serialize_TPM2B_PRIVATE_VENDOR_SPECIFIC(
    const TPM2B_PRIVATE_VENDOR_SPECIFIC& value,
    std::string* buffer) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Serialize_UINT16(value.size, buffer);
  if (result) {
    return result;
  }

  if (arraysize(value.buffer) < value.size) {
    return TPM_RC_INSUFFICIENT;
  }
  for (uint32_t i = 0; i < value.size; ++i) {
    result = Serialize_BYTE(value.buffer[i], buffer);
    if (result) {
      return result;
    }
  }
  return result;
}

TPM_RC Parse_TPM2B_PRIVATE_VENDOR_SPECIFIC(
    std::string* buffer,
    TPM2B_PRIVATE_VENDOR_SPECIFIC* value,
    std::string* value_bytes) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Parse_UINT16(
      buffer,
      &value->size,
      value_bytes);
  if (result) {
    return result;
  }

  if (arraysize(value->buffer) < value->size) {
    return TPM_RC_INSUFFICIENT;
  }
  for (uint32_t i = 0; i < value->size; ++i) {
    result = Parse_BYTE(
        buffer,
        &value->buffer[i],
        value_bytes);
    if (result) {
      return result;
    }
  }
  return result;
}

TPM2B_PRIVATE_VENDOR_SPECIFIC Make_TPM2B_PRIVATE_VENDOR_SPECIFIC(
    const std::string& bytes) {
  TPM2B_PRIVATE_VENDOR_SPECIFIC tpm2b;
  CHECK(bytes.size() <= sizeof(tpm2b.buffer));
  memset(&tpm2b, 0, sizeof(TPM2B_PRIVATE_VENDOR_SPECIFIC));
  tpm2b.size = bytes.size();
  memcpy(tpm2b.buffer, bytes.data(), bytes.size());
  return tpm2b;
}

std::string StringFrom_TPM2B_PRIVATE_VENDOR_SPECIFIC(
    const TPM2B_PRIVATE_VENDOR_SPECIFIC& tpm2b) {
  const char* char_buffer = reinterpret_cast<const char*>(
      tpm2b.buffer);
  return std::string(char_buffer, tpm2b.size);
}

TPM_RC Serialize_TPMU_SENSITIVE_COMPOSITE(
    const TPMU_SENSITIVE_COMPOSITE& value,
    TPMI_ALG_PUBLIC selector,
    std::string* buffer) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  if (selector == TPM_ALG_KEYEDHASH) {
    result = Serialize_TPM2B_SENSITIVE_DATA(value.bits, buffer);
    if (result) {
      return result;
    }
  }

  if (selector == TPM_ALG_RSA) {
    result = Serialize_TPM2B_PRIVATE_KEY_RSA(value.rsa, buffer);
    if (result) {
      return result;
    }
  }

  if (selector == TPM_ALG_SYMCIPHER) {
    result = Serialize_TPM2B_SYM_KEY(value.sym, buffer);
    if (result) {
      return result;
    }
  }

  if (selector == TPM_ALG_ECC) {
    result = Serialize_TPM2B_ECC_PARAMETER(value.ecc, buffer);
    if (result) {
      return result;
    }
  }
  return result;
}

TPM_RC Parse_TPMU_SENSITIVE_COMPOSITE(
    std::string* buffer,
    TPMI_ALG_PUBLIC selector,
    TPMU_SENSITIVE_COMPOSITE* value,
    std::string* value_bytes) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  if (selector == TPM_ALG_KEYEDHASH) {
    result = Parse_TPM2B_SENSITIVE_DATA(
        buffer,
        &value->bits,
        value_bytes);
    if (result) {
      return result;
    }
  }

  if (selector == TPM_ALG_RSA) {
    result = Parse_TPM2B_PRIVATE_KEY_RSA(
        buffer,
        &value->rsa,
        value_bytes);
    if (result) {
      return result;
    }
  }

  if (selector == TPM_ALG_SYMCIPHER) {
    result = Parse_TPM2B_SYM_KEY(
        buffer,
        &value->sym,
        value_bytes);
    if (result) {
      return result;
    }
  }

  if (selector == TPM_ALG_ECC) {
    result = Parse_TPM2B_ECC_PARAMETER(
        buffer,
        &value->ecc,
        value_bytes);
    if (result) {
      return result;
    }
  }
  return result;
}

TPM_RC Serialize_TPMT_SENSITIVE(
    const TPMT_SENSITIVE& value,
    std::string* buffer) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Serialize_TPMI_ALG_PUBLIC(value.sensitive_type, buffer);
  if (result) {
    return result;
  }

  result = Serialize_TPM2B_AUTH(value.auth_value, buffer);
  if (result) {
    return result;
  }

  result = Serialize_TPM2B_DIGEST(value.seed_value, buffer);
  if (result) {
    return result;
  }

  result = Serialize_TPMU_SENSITIVE_COMPOSITE(
      value.sensitive,
      value.sensitive_type,
      buffer);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Parse_TPMT_SENSITIVE(
    std::string* buffer,
    TPMT_SENSITIVE* value,
    std::string* value_bytes) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Parse_TPMI_ALG_PUBLIC(
      buffer,
      &value->sensitive_type,
      value_bytes);
  if (result) {
    return result;
  }

  result = Parse_TPM2B_AUTH(
      buffer,
      &value->auth_value,
      value_bytes);
  if (result) {
    return result;
  }

  result = Parse_TPM2B_DIGEST(
      buffer,
      &value->seed_value,
      value_bytes);
  if (result) {
    return result;
  }

  result = Parse_TPMU_SENSITIVE_COMPOSITE(
      buffer,
      value->sensitive_type,
      &value->sensitive,
      value_bytes);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Serialize_TPM2B_SENSITIVE(
    const TPM2B_SENSITIVE& value,
    std::string* buffer) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Serialize_UINT16(value.size, buffer);
  if (result) {
    return result;
  }

  result = Serialize_TPMT_SENSITIVE(value.sensitive_area, buffer);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Parse_TPM2B_SENSITIVE(
    std::string* buffer,
    TPM2B_SENSITIVE* value,
    std::string* value_bytes) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Parse_UINT16(
      buffer,
      &value->size,
      value_bytes);
  if (result) {
    return result;
  }

  result = Parse_TPMT_SENSITIVE(
      buffer,
      &value->sensitive_area,
      value_bytes);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Serialize__PRIVATE(
    const _PRIVATE& value,
    std::string* buffer) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Serialize_TPM2B_DIGEST(value.integrity_outer, buffer);
  if (result) {
    return result;
  }

  result = Serialize_TPM2B_DIGEST(value.integrity_inner, buffer);
  if (result) {
    return result;
  }

  result = Serialize_TPMT_SENSITIVE(value.sensitive, buffer);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Parse__PRIVATE(
    std::string* buffer,
    _PRIVATE* value,
    std::string* value_bytes) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Parse_TPM2B_DIGEST(
      buffer,
      &value->integrity_outer,
      value_bytes);
  if (result) {
    return result;
  }

  result = Parse_TPM2B_DIGEST(
      buffer,
      &value->integrity_inner,
      value_bytes);
  if (result) {
    return result;
  }

  result = Parse_TPMT_SENSITIVE(
      buffer,
      &value->sensitive,
      value_bytes);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Serialize_TPM2B_PRIVATE(
    const TPM2B_PRIVATE& value,
    std::string* buffer) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Serialize_UINT16(value.size, buffer);
  if (result) {
    return result;
  }

  if (arraysize(value.buffer) < value.size) {
    return TPM_RC_INSUFFICIENT;
  }
  for (uint32_t i = 0; i < value.size; ++i) {
    result = Serialize_BYTE(value.buffer[i], buffer);
    if (result) {
      return result;
    }
  }
  return result;
}

TPM_RC Parse_TPM2B_PRIVATE(
    std::string* buffer,
    TPM2B_PRIVATE* value,
    std::string* value_bytes) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Parse_UINT16(
      buffer,
      &value->size,
      value_bytes);
  if (result) {
    return result;
  }

  if (arraysize(value->buffer) < value->size) {
    return TPM_RC_INSUFFICIENT;
  }
  for (uint32_t i = 0; i < value->size; ++i) {
    result = Parse_BYTE(
        buffer,
        &value->buffer[i],
        value_bytes);
    if (result) {
      return result;
    }
  }
  return result;
}

TPM2B_PRIVATE Make_TPM2B_PRIVATE(
    const std::string& bytes) {
  TPM2B_PRIVATE tpm2b;
  CHECK(bytes.size() <= sizeof(tpm2b.buffer));
  memset(&tpm2b, 0, sizeof(TPM2B_PRIVATE));
  tpm2b.size = bytes.size();
  memcpy(tpm2b.buffer, bytes.data(), bytes.size());
  return tpm2b;
}

std::string StringFrom_TPM2B_PRIVATE(
    const TPM2B_PRIVATE& tpm2b) {
  const char* char_buffer = reinterpret_cast<const char*>(
      tpm2b.buffer);
  return std::string(char_buffer, tpm2b.size);
}

TPM_RC Serialize__ID_OBJECT(
    const _ID_OBJECT& value,
    std::string* buffer) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Serialize_TPM2B_DIGEST(value.integrity_hmac, buffer);
  if (result) {
    return result;
  }

  result = Serialize_TPM2B_DIGEST(value.enc_identity, buffer);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Parse__ID_OBJECT(
    std::string* buffer,
    _ID_OBJECT* value,
    std::string* value_bytes) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Parse_TPM2B_DIGEST(
      buffer,
      &value->integrity_hmac,
      value_bytes);
  if (result) {
    return result;
  }

  result = Parse_TPM2B_DIGEST(
      buffer,
      &value->enc_identity,
      value_bytes);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Serialize_TPM2B_ID_OBJECT(
    const TPM2B_ID_OBJECT& value,
    std::string* buffer) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Serialize_UINT16(value.size, buffer);
  if (result) {
    return result;
  }

  if (arraysize(value.credential) < value.size) {
    return TPM_RC_INSUFFICIENT;
  }
  for (uint32_t i = 0; i < value.size; ++i) {
    result = Serialize_BYTE(value.credential[i], buffer);
    if (result) {
      return result;
    }
  }
  return result;
}

TPM_RC Parse_TPM2B_ID_OBJECT(
    std::string* buffer,
    TPM2B_ID_OBJECT* value,
    std::string* value_bytes) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Parse_UINT16(
      buffer,
      &value->size,
      value_bytes);
  if (result) {
    return result;
  }

  if (arraysize(value->credential) < value->size) {
    return TPM_RC_INSUFFICIENT;
  }
  for (uint32_t i = 0; i < value->size; ++i) {
    result = Parse_BYTE(
        buffer,
        &value->credential[i],
        value_bytes);
    if (result) {
      return result;
    }
  }
  return result;
}

TPM2B_ID_OBJECT Make_TPM2B_ID_OBJECT(
    const std::string& bytes) {
  TPM2B_ID_OBJECT tpm2b;
  CHECK(bytes.size() <= sizeof(tpm2b.credential));
  memset(&tpm2b, 0, sizeof(TPM2B_ID_OBJECT));
  tpm2b.size = bytes.size();
  memcpy(tpm2b.credential, bytes.data(), bytes.size());
  return tpm2b;
}

std::string StringFrom_TPM2B_ID_OBJECT(
    const TPM2B_ID_OBJECT& tpm2b) {
  const char* char_buffer = reinterpret_cast<const char*>(
      tpm2b.credential);
  return std::string(char_buffer, tpm2b.size);
}

TPM_RC Serialize_TPMS_NV_PUBLIC(
    const TPMS_NV_PUBLIC& value,
    std::string* buffer) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Serialize_TPMI_RH_NV_INDEX(value.nv_index, buffer);
  if (result) {
    return result;
  }

  result = Serialize_TPMI_ALG_HASH(value.name_alg, buffer);
  if (result) {
    return result;
  }

  result = Serialize_TPMA_NV(value.attributes, buffer);
  if (result) {
    return result;
  }

  result = Serialize_TPM2B_DIGEST(value.auth_policy, buffer);
  if (result) {
    return result;
  }

  result = Serialize_UINT16(value.data_size, buffer);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Parse_TPMS_NV_PUBLIC(
    std::string* buffer,
    TPMS_NV_PUBLIC* value,
    std::string* value_bytes) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Parse_TPMI_RH_NV_INDEX(
      buffer,
      &value->nv_index,
      value_bytes);
  if (result) {
    return result;
  }

  result = Parse_TPMI_ALG_HASH(
      buffer,
      &value->name_alg,
      value_bytes);
  if (result) {
    return result;
  }

  result = Parse_TPMA_NV(
      buffer,
      &value->attributes,
      value_bytes);
  if (result) {
    return result;
  }

  result = Parse_TPM2B_DIGEST(
      buffer,
      &value->auth_policy,
      value_bytes);
  if (result) {
    return result;
  }

  result = Parse_UINT16(
      buffer,
      &value->data_size,
      value_bytes);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Serialize_TPM2B_NV_PUBLIC(
    const TPM2B_NV_PUBLIC& value,
    std::string* buffer) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Serialize_UINT16(value.size, buffer);
  if (result) {
    return result;
  }

  result = Serialize_TPMS_NV_PUBLIC(value.nv_public, buffer);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Parse_TPM2B_NV_PUBLIC(
    std::string* buffer,
    TPM2B_NV_PUBLIC* value,
    std::string* value_bytes) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Parse_UINT16(
      buffer,
      &value->size,
      value_bytes);
  if (result) {
    return result;
  }

  result = Parse_TPMS_NV_PUBLIC(
      buffer,
      &value->nv_public,
      value_bytes);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Serialize_TPM2B_CONTEXT_SENSITIVE(
    const TPM2B_CONTEXT_SENSITIVE& value,
    std::string* buffer) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Serialize_UINT16(value.size, buffer);
  if (result) {
    return result;
  }

  if (arraysize(value.buffer) < value.size) {
    return TPM_RC_INSUFFICIENT;
  }
  for (uint32_t i = 0; i < value.size; ++i) {
    result = Serialize_BYTE(value.buffer[i], buffer);
    if (result) {
      return result;
    }
  }
  return result;
}

TPM_RC Parse_TPM2B_CONTEXT_SENSITIVE(
    std::string* buffer,
    TPM2B_CONTEXT_SENSITIVE* value,
    std::string* value_bytes) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Parse_UINT16(
      buffer,
      &value->size,
      value_bytes);
  if (result) {
    return result;
  }

  if (arraysize(value->buffer) < value->size) {
    return TPM_RC_INSUFFICIENT;
  }
  for (uint32_t i = 0; i < value->size; ++i) {
    result = Parse_BYTE(
        buffer,
        &value->buffer[i],
        value_bytes);
    if (result) {
      return result;
    }
  }
  return result;
}

TPM2B_CONTEXT_SENSITIVE Make_TPM2B_CONTEXT_SENSITIVE(
    const std::string& bytes) {
  TPM2B_CONTEXT_SENSITIVE tpm2b;
  CHECK(bytes.size() <= sizeof(tpm2b.buffer));
  memset(&tpm2b, 0, sizeof(TPM2B_CONTEXT_SENSITIVE));
  tpm2b.size = bytes.size();
  memcpy(tpm2b.buffer, bytes.data(), bytes.size());
  return tpm2b;
}

std::string StringFrom_TPM2B_CONTEXT_SENSITIVE(
    const TPM2B_CONTEXT_SENSITIVE& tpm2b) {
  const char* char_buffer = reinterpret_cast<const char*>(
      tpm2b.buffer);
  return std::string(char_buffer, tpm2b.size);
}

TPM_RC Serialize_TPMS_CONTEXT_DATA(
    const TPMS_CONTEXT_DATA& value,
    std::string* buffer) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Serialize_TPM2B_DIGEST(value.integrity, buffer);
  if (result) {
    return result;
  }

  result = Serialize_TPM2B_CONTEXT_SENSITIVE(value.encrypted, buffer);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Parse_TPMS_CONTEXT_DATA(
    std::string* buffer,
    TPMS_CONTEXT_DATA* value,
    std::string* value_bytes) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Parse_TPM2B_DIGEST(
      buffer,
      &value->integrity,
      value_bytes);
  if (result) {
    return result;
  }

  result = Parse_TPM2B_CONTEXT_SENSITIVE(
      buffer,
      &value->encrypted,
      value_bytes);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Serialize_TPM2B_CONTEXT_DATA(
    const TPM2B_CONTEXT_DATA& value,
    std::string* buffer) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Serialize_UINT16(value.size, buffer);
  if (result) {
    return result;
  }

  if (arraysize(value.buffer) < value.size) {
    return TPM_RC_INSUFFICIENT;
  }
  for (uint32_t i = 0; i < value.size; ++i) {
    result = Serialize_BYTE(value.buffer[i], buffer);
    if (result) {
      return result;
    }
  }
  return result;
}

TPM_RC Parse_TPM2B_CONTEXT_DATA(
    std::string* buffer,
    TPM2B_CONTEXT_DATA* value,
    std::string* value_bytes) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Parse_UINT16(
      buffer,
      &value->size,
      value_bytes);
  if (result) {
    return result;
  }

  if (arraysize(value->buffer) < value->size) {
    return TPM_RC_INSUFFICIENT;
  }
  for (uint32_t i = 0; i < value->size; ++i) {
    result = Parse_BYTE(
        buffer,
        &value->buffer[i],
        value_bytes);
    if (result) {
      return result;
    }
  }
  return result;
}

TPM2B_CONTEXT_DATA Make_TPM2B_CONTEXT_DATA(
    const std::string& bytes) {
  TPM2B_CONTEXT_DATA tpm2b;
  CHECK(bytes.size() <= sizeof(tpm2b.buffer));
  memset(&tpm2b, 0, sizeof(TPM2B_CONTEXT_DATA));
  tpm2b.size = bytes.size();
  memcpy(tpm2b.buffer, bytes.data(), bytes.size());
  return tpm2b;
}

std::string StringFrom_TPM2B_CONTEXT_DATA(
    const TPM2B_CONTEXT_DATA& tpm2b) {
  const char* char_buffer = reinterpret_cast<const char*>(
      tpm2b.buffer);
  return std::string(char_buffer, tpm2b.size);
}

TPM_RC Serialize_TPMS_CONTEXT(
    const TPMS_CONTEXT& value,
    std::string* buffer) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Serialize_UINT64(value.sequence, buffer);
  if (result) {
    return result;
  }

  result = Serialize_TPMI_DH_CONTEXT(value.saved_handle, buffer);
  if (result) {
    return result;
  }

  result = Serialize_TPMI_RH_HIERARCHY(value.hierarchy, buffer);
  if (result) {
    return result;
  }

  result = Serialize_TPM2B_CONTEXT_DATA(value.context_blob, buffer);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Parse_TPMS_CONTEXT(
    std::string* buffer,
    TPMS_CONTEXT* value,
    std::string* value_bytes) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Parse_UINT64(
      buffer,
      &value->sequence,
      value_bytes);
  if (result) {
    return result;
  }

  result = Parse_TPMI_DH_CONTEXT(
      buffer,
      &value->saved_handle,
      value_bytes);
  if (result) {
    return result;
  }

  result = Parse_TPMI_RH_HIERARCHY(
      buffer,
      &value->hierarchy,
      value_bytes);
  if (result) {
    return result;
  }

  result = Parse_TPM2B_CONTEXT_DATA(
      buffer,
      &value->context_blob,
      value_bytes);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Serialize_TPMS_CREATION_DATA(
    const TPMS_CREATION_DATA& value,
    std::string* buffer) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Serialize_TPML_PCR_SELECTION(value.pcr_select, buffer);
  if (result) {
    return result;
  }

  result = Serialize_TPM2B_DIGEST(value.pcr_digest, buffer);
  if (result) {
    return result;
  }

  result = Serialize_TPMA_LOCALITY(value.locality, buffer);
  if (result) {
    return result;
  }

  result = Serialize_TPM_ALG_ID(value.parent_name_alg, buffer);
  if (result) {
    return result;
  }

  result = Serialize_TPM2B_NAME(value.parent_name, buffer);
  if (result) {
    return result;
  }

  result = Serialize_TPM2B_NAME(value.parent_qualified_name, buffer);
  if (result) {
    return result;
  }

  result = Serialize_TPM2B_DATA(value.outside_info, buffer);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Parse_TPMS_CREATION_DATA(
    std::string* buffer,
    TPMS_CREATION_DATA* value,
    std::string* value_bytes) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Parse_TPML_PCR_SELECTION(
      buffer,
      &value->pcr_select,
      value_bytes);
  if (result) {
    return result;
  }

  result = Parse_TPM2B_DIGEST(
      buffer,
      &value->pcr_digest,
      value_bytes);
  if (result) {
    return result;
  }

  result = Parse_TPMA_LOCALITY(
      buffer,
      &value->locality,
      value_bytes);
  if (result) {
    return result;
  }

  result = Parse_TPM_ALG_ID(
      buffer,
      &value->parent_name_alg,
      value_bytes);
  if (result) {
    return result;
  }

  result = Parse_TPM2B_NAME(
      buffer,
      &value->parent_name,
      value_bytes);
  if (result) {
    return result;
  }

  result = Parse_TPM2B_NAME(
      buffer,
      &value->parent_qualified_name,
      value_bytes);
  if (result) {
    return result;
  }

  result = Parse_TPM2B_DATA(
      buffer,
      &value->outside_info,
      value_bytes);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Serialize_TPM2B_CREATION_DATA(
    const TPM2B_CREATION_DATA& value,
    std::string* buffer) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Serialize_UINT16(value.size, buffer);
  if (result) {
    return result;
  }

  result = Serialize_TPMS_CREATION_DATA(value.creation_data, buffer);
  if (result) {
    return result;
  }
  return result;
}

TPM_RC Parse_TPM2B_CREATION_DATA(
    std::string* buffer,
    TPM2B_CREATION_DATA* value,
    std::string* value_bytes) {
  TPM_RC result = TPM_RC_SUCCESS;
  VLOG(2) << __func__;

  result = Parse_UINT16(
      buffer,
      &value->size,
      value_bytes);
  if (result) {
    return result;
  }

  result = Parse_TPMS_CREATION_DATA(
      buffer,
      &value->creation_data,
      value_bytes);
  if (result) {
    return result;
  }
  return result;
}

void StartupErrorCallback(
    const Tpm::StartupResponse& callback,
    TPM_RC response_code) {
  callback.Run(response_code);
}

void StartupResponseParser(
    const Tpm::StartupResponse& callback,
    AuthorizationDelegate* authorization_delegate,
    const std::string& response) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(StartupErrorCallback, callback);
  std::string buffer(response);
  TPM_ST tag;
  std::string tag_bytes;
  rc = Parse_TPM_ST(
      &buffer,
      &tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  UINT32 response_size;
  std::string response_size_bytes;
  rc = Parse_UINT32(
      &buffer,
      &response_size,
      &response_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM_RC response_code;
  std::string response_code_bytes;
  rc = Parse_TPM_RC(
      &buffer,
      &response_code,
      &response_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (response_size != response.size()) {
    error_reporter.Run(TPM_RC_SIZE);
    return;
  }
  if (response_code != TPM_RC_SUCCESS) {
    error_reporter.Run(response_code);
    return;
  }
  TPM_CC command_code = TPM_CC_Startup;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string authorization_section_bytes;
  if (tag == TPM_ST_SESSIONS) {
    UINT32 parameter_section_size = buffer.size();
    rc = Parse_UINT32(&buffer, &parameter_section_size, NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    if (parameter_section_size > buffer.size()) {
      error_reporter.Run(TPM_RC_INSUFFICIENT);
      return;
    }
    authorization_section_bytes = buffer.substr(parameter_section_size);
    // Keep the parameter section in |buffer|.
    buffer.erase(parameter_section_size);
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(response_code_bytes.data(),
               response_code_bytes.size());
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(buffer.data(),
               buffer.size());
  std::string response_hash(32, 0);
  hash->Finish(string_as_array(&response_hash), response_hash.size());
  if (tag == TPM_ST_SESSIONS) {
    if (!authorization_delegate->CheckResponseAuthorization(
        response_hash,
        authorization_section_bytes)) {
      error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
      return;
    }
  }
  callback.Run(response_code);
}

void Tpm::Startup(
      TPM_SU startup_type,
      AuthorizationDelegate* authorization_delegate,
      const StartupResponse& callback) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(StartupErrorCallback, callback);
  base::Callback<void(const std::string&)> parser =
      base::Bind(StartupResponseParser,
                 callback,
                 authorization_delegate);
  TPMI_ST_COMMAND_TAG tag = TPM_ST_NO_SESSIONS;
  UINT32 command_size = 10;  // Header size.
  std::string handle_section_bytes;
  std::string parameter_section_bytes;
  TPM_CC command_code = TPM_CC_Startup;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string startup_type_bytes;
  rc = Serialize_TPM_SU(
      startup_type,
      &startup_type_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(startup_type_bytes.data(),
               startup_type_bytes.size());
  parameter_section_bytes += startup_type_bytes;
  command_size += startup_type_bytes.size();
  std::string command_hash(32, 0);
  hash->Finish(string_as_array(&command_hash), command_hash.size());
  std::string authorization_section_bytes;
  if (!authorization_delegate->GetCommandAuthorization(
      command_hash,
      &authorization_section_bytes)) {
    error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
    return;
  }
  std::string authorization_size_bytes;
  if (!authorization_section_bytes.empty()) {
    tag = TPM_ST_SESSIONS;
    std::string tmp;
    rc = Serialize_UINT32(authorization_section_bytes.size(),
                          &authorization_size_bytes);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    command_size += authorization_size_bytes.size() +
                    authorization_section_bytes.size();
  }
  std::string tag_bytes;
  rc = Serialize_TPMI_ST_COMMAND_TAG(
      tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command_size_bytes;
  rc = Serialize_UINT32(
      command_size,
      &command_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command = tag_bytes +
                        command_size_bytes +
                        command_code_bytes +
                        handle_section_bytes +
                        authorization_size_bytes +
                        authorization_section_bytes +
                        parameter_section_bytes;
  CHECK(command.size() == command_size) << "Command size mismatch!";
  transceiver_->SendCommand(command, parser);
}

void ShutdownErrorCallback(
    const Tpm::ShutdownResponse& callback,
    TPM_RC response_code) {
  callback.Run(response_code);
}

void ShutdownResponseParser(
    const Tpm::ShutdownResponse& callback,
    AuthorizationDelegate* authorization_delegate,
    const std::string& response) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(ShutdownErrorCallback, callback);
  std::string buffer(response);
  TPM_ST tag;
  std::string tag_bytes;
  rc = Parse_TPM_ST(
      &buffer,
      &tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  UINT32 response_size;
  std::string response_size_bytes;
  rc = Parse_UINT32(
      &buffer,
      &response_size,
      &response_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM_RC response_code;
  std::string response_code_bytes;
  rc = Parse_TPM_RC(
      &buffer,
      &response_code,
      &response_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (response_size != response.size()) {
    error_reporter.Run(TPM_RC_SIZE);
    return;
  }
  if (response_code != TPM_RC_SUCCESS) {
    error_reporter.Run(response_code);
    return;
  }
  TPM_CC command_code = TPM_CC_Shutdown;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string authorization_section_bytes;
  if (tag == TPM_ST_SESSIONS) {
    UINT32 parameter_section_size = buffer.size();
    rc = Parse_UINT32(&buffer, &parameter_section_size, NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    if (parameter_section_size > buffer.size()) {
      error_reporter.Run(TPM_RC_INSUFFICIENT);
      return;
    }
    authorization_section_bytes = buffer.substr(parameter_section_size);
    // Keep the parameter section in |buffer|.
    buffer.erase(parameter_section_size);
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(response_code_bytes.data(),
               response_code_bytes.size());
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(buffer.data(),
               buffer.size());
  std::string response_hash(32, 0);
  hash->Finish(string_as_array(&response_hash), response_hash.size());
  if (tag == TPM_ST_SESSIONS) {
    if (!authorization_delegate->CheckResponseAuthorization(
        response_hash,
        authorization_section_bytes)) {
      error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
      return;
    }
  }
  callback.Run(response_code);
}

void Tpm::Shutdown(
      TPM_SU shutdown_type,
      AuthorizationDelegate* authorization_delegate,
      const ShutdownResponse& callback) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(ShutdownErrorCallback, callback);
  base::Callback<void(const std::string&)> parser =
      base::Bind(ShutdownResponseParser,
                 callback,
                 authorization_delegate);
  TPMI_ST_COMMAND_TAG tag = TPM_ST_NO_SESSIONS;
  UINT32 command_size = 10;  // Header size.
  std::string handle_section_bytes;
  std::string parameter_section_bytes;
  TPM_CC command_code = TPM_CC_Shutdown;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string shutdown_type_bytes;
  rc = Serialize_TPM_SU(
      shutdown_type,
      &shutdown_type_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(shutdown_type_bytes.data(),
               shutdown_type_bytes.size());
  parameter_section_bytes += shutdown_type_bytes;
  command_size += shutdown_type_bytes.size();
  std::string command_hash(32, 0);
  hash->Finish(string_as_array(&command_hash), command_hash.size());
  std::string authorization_section_bytes;
  if (!authorization_delegate->GetCommandAuthorization(
      command_hash,
      &authorization_section_bytes)) {
    error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
    return;
  }
  std::string authorization_size_bytes;
  if (!authorization_section_bytes.empty()) {
    tag = TPM_ST_SESSIONS;
    std::string tmp;
    rc = Serialize_UINT32(authorization_section_bytes.size(),
                          &authorization_size_bytes);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    command_size += authorization_size_bytes.size() +
                    authorization_section_bytes.size();
  }
  std::string tag_bytes;
  rc = Serialize_TPMI_ST_COMMAND_TAG(
      tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command_size_bytes;
  rc = Serialize_UINT32(
      command_size,
      &command_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command = tag_bytes +
                        command_size_bytes +
                        command_code_bytes +
                        handle_section_bytes +
                        authorization_size_bytes +
                        authorization_section_bytes +
                        parameter_section_bytes;
  CHECK(command.size() == command_size) << "Command size mismatch!";
  transceiver_->SendCommand(command, parser);
}

void SelfTestErrorCallback(
    const Tpm::SelfTestResponse& callback,
    TPM_RC response_code) {
  callback.Run(response_code);
}

void SelfTestResponseParser(
    const Tpm::SelfTestResponse& callback,
    AuthorizationDelegate* authorization_delegate,
    const std::string& response) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(SelfTestErrorCallback, callback);
  std::string buffer(response);
  TPM_ST tag;
  std::string tag_bytes;
  rc = Parse_TPM_ST(
      &buffer,
      &tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  UINT32 response_size;
  std::string response_size_bytes;
  rc = Parse_UINT32(
      &buffer,
      &response_size,
      &response_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM_RC response_code;
  std::string response_code_bytes;
  rc = Parse_TPM_RC(
      &buffer,
      &response_code,
      &response_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (response_size != response.size()) {
    error_reporter.Run(TPM_RC_SIZE);
    return;
  }
  if (response_code != TPM_RC_SUCCESS) {
    error_reporter.Run(response_code);
    return;
  }
  TPM_CC command_code = TPM_CC_SelfTest;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string authorization_section_bytes;
  if (tag == TPM_ST_SESSIONS) {
    UINT32 parameter_section_size = buffer.size();
    rc = Parse_UINT32(&buffer, &parameter_section_size, NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    if (parameter_section_size > buffer.size()) {
      error_reporter.Run(TPM_RC_INSUFFICIENT);
      return;
    }
    authorization_section_bytes = buffer.substr(parameter_section_size);
    // Keep the parameter section in |buffer|.
    buffer.erase(parameter_section_size);
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(response_code_bytes.data(),
               response_code_bytes.size());
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(buffer.data(),
               buffer.size());
  std::string response_hash(32, 0);
  hash->Finish(string_as_array(&response_hash), response_hash.size());
  if (tag == TPM_ST_SESSIONS) {
    if (!authorization_delegate->CheckResponseAuthorization(
        response_hash,
        authorization_section_bytes)) {
      error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
      return;
    }
  }
  callback.Run(response_code);
}

void Tpm::SelfTest(
      TPMI_YES_NO full_test,
      AuthorizationDelegate* authorization_delegate,
      const SelfTestResponse& callback) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(SelfTestErrorCallback, callback);
  base::Callback<void(const std::string&)> parser =
      base::Bind(SelfTestResponseParser,
                 callback,
                 authorization_delegate);
  TPMI_ST_COMMAND_TAG tag = TPM_ST_NO_SESSIONS;
  UINT32 command_size = 10;  // Header size.
  std::string handle_section_bytes;
  std::string parameter_section_bytes;
  TPM_CC command_code = TPM_CC_SelfTest;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string full_test_bytes;
  rc = Serialize_TPMI_YES_NO(
      full_test,
      &full_test_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(full_test_bytes.data(),
               full_test_bytes.size());
  parameter_section_bytes += full_test_bytes;
  command_size += full_test_bytes.size();
  std::string command_hash(32, 0);
  hash->Finish(string_as_array(&command_hash), command_hash.size());
  std::string authorization_section_bytes;
  if (!authorization_delegate->GetCommandAuthorization(
      command_hash,
      &authorization_section_bytes)) {
    error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
    return;
  }
  std::string authorization_size_bytes;
  if (!authorization_section_bytes.empty()) {
    tag = TPM_ST_SESSIONS;
    std::string tmp;
    rc = Serialize_UINT32(authorization_section_bytes.size(),
                          &authorization_size_bytes);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    command_size += authorization_size_bytes.size() +
                    authorization_section_bytes.size();
  }
  std::string tag_bytes;
  rc = Serialize_TPMI_ST_COMMAND_TAG(
      tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command_size_bytes;
  rc = Serialize_UINT32(
      command_size,
      &command_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command = tag_bytes +
                        command_size_bytes +
                        command_code_bytes +
                        handle_section_bytes +
                        authorization_size_bytes +
                        authorization_section_bytes +
                        parameter_section_bytes;
  CHECK(command.size() == command_size) << "Command size mismatch!";
  transceiver_->SendCommand(command, parser);
}

void IncrementalSelfTestErrorCallback(
    const Tpm::IncrementalSelfTestResponse& callback,
    TPM_RC response_code) {
  callback.Run(response_code,
               TPML_ALG());
}

void IncrementalSelfTestResponseParser(
    const Tpm::IncrementalSelfTestResponse& callback,
    AuthorizationDelegate* authorization_delegate,
    const std::string& response) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(IncrementalSelfTestErrorCallback, callback);
  std::string buffer(response);
  TPM_ST tag;
  std::string tag_bytes;
  rc = Parse_TPM_ST(
      &buffer,
      &tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  UINT32 response_size;
  std::string response_size_bytes;
  rc = Parse_UINT32(
      &buffer,
      &response_size,
      &response_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM_RC response_code;
  std::string response_code_bytes;
  rc = Parse_TPM_RC(
      &buffer,
      &response_code,
      &response_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (response_size != response.size()) {
    error_reporter.Run(TPM_RC_SIZE);
    return;
  }
  if (response_code != TPM_RC_SUCCESS) {
    error_reporter.Run(response_code);
    return;
  }
  TPM_CC command_code = TPM_CC_IncrementalSelfTest;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string authorization_section_bytes;
  if (tag == TPM_ST_SESSIONS) {
    UINT32 parameter_section_size = buffer.size();
    rc = Parse_UINT32(&buffer, &parameter_section_size, NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    if (parameter_section_size > buffer.size()) {
      error_reporter.Run(TPM_RC_INSUFFICIENT);
      return;
    }
    authorization_section_bytes = buffer.substr(parameter_section_size);
    // Keep the parameter section in |buffer|.
    buffer.erase(parameter_section_size);
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(response_code_bytes.data(),
               response_code_bytes.size());
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(buffer.data(),
               buffer.size());
  std::string response_hash(32, 0);
  hash->Finish(string_as_array(&response_hash), response_hash.size());
  if (tag == TPM_ST_SESSIONS) {
    if (!authorization_delegate->CheckResponseAuthorization(
        response_hash,
        authorization_section_bytes)) {
      error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
      return;
    }
  }
  TPML_ALG to_do_list;
  std::string to_do_list_bytes;
  rc = Parse_TPML_ALG(
      &buffer,
      &to_do_list,
      &to_do_list_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (tag == TPM_ST_SESSIONS) {
    // Decrypt just the parameter data, not the size.
    std::string tmp = to_do_list_bytes.substr(2);
    if (!authorization_delegate->DecryptResponseParameter(&tmp)) {
      error_reporter.Run(TRUNKS_RC_ENCRYPTION_FAILED);
      return;
    }
    to_do_list_bytes.replace(2, std::string::npos, tmp);
    rc = Parse_TPML_ALG(
        &to_do_list_bytes,
        &to_do_list,
        NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
  }
  callback.Run(response_code,
               to_do_list);
}

void Tpm::IncrementalSelfTest(
      TPML_ALG to_test,
      AuthorizationDelegate* authorization_delegate,
      const IncrementalSelfTestResponse& callback) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(IncrementalSelfTestErrorCallback, callback);
  base::Callback<void(const std::string&)> parser =
      base::Bind(IncrementalSelfTestResponseParser,
                 callback,
                 authorization_delegate);
  TPMI_ST_COMMAND_TAG tag = TPM_ST_NO_SESSIONS;
  UINT32 command_size = 10;  // Header size.
  std::string handle_section_bytes;
  std::string parameter_section_bytes;
  TPM_CC command_code = TPM_CC_IncrementalSelfTest;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string to_test_bytes;
  rc = Serialize_TPML_ALG(
      to_test,
      &to_test_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(to_test_bytes.data(),
               to_test_bytes.size());
  parameter_section_bytes += to_test_bytes;
  command_size += to_test_bytes.size();
  std::string command_hash(32, 0);
  hash->Finish(string_as_array(&command_hash), command_hash.size());
  std::string authorization_section_bytes;
  if (!authorization_delegate->GetCommandAuthorization(
      command_hash,
      &authorization_section_bytes)) {
    error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
    return;
  }
  std::string authorization_size_bytes;
  if (!authorization_section_bytes.empty()) {
    tag = TPM_ST_SESSIONS;
    std::string tmp;
    rc = Serialize_UINT32(authorization_section_bytes.size(),
                          &authorization_size_bytes);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    command_size += authorization_size_bytes.size() +
                    authorization_section_bytes.size();
  }
  std::string tag_bytes;
  rc = Serialize_TPMI_ST_COMMAND_TAG(
      tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command_size_bytes;
  rc = Serialize_UINT32(
      command_size,
      &command_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command = tag_bytes +
                        command_size_bytes +
                        command_code_bytes +
                        handle_section_bytes +
                        authorization_size_bytes +
                        authorization_section_bytes +
                        parameter_section_bytes;
  CHECK(command.size() == command_size) << "Command size mismatch!";
  transceiver_->SendCommand(command, parser);
}

void GetTestResultErrorCallback(
    const Tpm::GetTestResultResponse& callback,
    TPM_RC response_code) {
  callback.Run(response_code,
               TPM2B_MAX_BUFFER(),
               TPM_RC());
}

void GetTestResultResponseParser(
    const Tpm::GetTestResultResponse& callback,
    AuthorizationDelegate* authorization_delegate,
    const std::string& response) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(GetTestResultErrorCallback, callback);
  std::string buffer(response);
  TPM_ST tag;
  std::string tag_bytes;
  rc = Parse_TPM_ST(
      &buffer,
      &tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  UINT32 response_size;
  std::string response_size_bytes;
  rc = Parse_UINT32(
      &buffer,
      &response_size,
      &response_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM_RC response_code;
  std::string response_code_bytes;
  rc = Parse_TPM_RC(
      &buffer,
      &response_code,
      &response_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (response_size != response.size()) {
    error_reporter.Run(TPM_RC_SIZE);
    return;
  }
  if (response_code != TPM_RC_SUCCESS) {
    error_reporter.Run(response_code);
    return;
  }
  TPM_CC command_code = TPM_CC_GetTestResult;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string authorization_section_bytes;
  if (tag == TPM_ST_SESSIONS) {
    UINT32 parameter_section_size = buffer.size();
    rc = Parse_UINT32(&buffer, &parameter_section_size, NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    if (parameter_section_size > buffer.size()) {
      error_reporter.Run(TPM_RC_INSUFFICIENT);
      return;
    }
    authorization_section_bytes = buffer.substr(parameter_section_size);
    // Keep the parameter section in |buffer|.
    buffer.erase(parameter_section_size);
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(response_code_bytes.data(),
               response_code_bytes.size());
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(buffer.data(),
               buffer.size());
  std::string response_hash(32, 0);
  hash->Finish(string_as_array(&response_hash), response_hash.size());
  if (tag == TPM_ST_SESSIONS) {
    if (!authorization_delegate->CheckResponseAuthorization(
        response_hash,
        authorization_section_bytes)) {
      error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
      return;
    }
  }
  TPM2B_MAX_BUFFER out_data;
  std::string out_data_bytes;
  rc = Parse_TPM2B_MAX_BUFFER(
      &buffer,
      &out_data,
      &out_data_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM_RC test_result;
  std::string test_result_bytes;
  rc = Parse_TPM_RC(
      &buffer,
      &test_result,
      &test_result_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (tag == TPM_ST_SESSIONS) {
    // Decrypt just the parameter data, not the size.
    std::string tmp = out_data_bytes.substr(2);
    if (!authorization_delegate->DecryptResponseParameter(&tmp)) {
      error_reporter.Run(TRUNKS_RC_ENCRYPTION_FAILED);
      return;
    }
    out_data_bytes.replace(2, std::string::npos, tmp);
    rc = Parse_TPM2B_MAX_BUFFER(
        &out_data_bytes,
        &out_data,
        NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
  }
  callback.Run(response_code,
               out_data,
               test_result);
}

void Tpm::GetTestResult(
      AuthorizationDelegate* authorization_delegate,
      const GetTestResultResponse& callback) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(GetTestResultErrorCallback, callback);
  base::Callback<void(const std::string&)> parser =
      base::Bind(GetTestResultResponseParser,
                 callback,
                 authorization_delegate);
  TPMI_ST_COMMAND_TAG tag = TPM_ST_NO_SESSIONS;
  UINT32 command_size = 10;  // Header size.
  std::string handle_section_bytes;
  std::string parameter_section_bytes;
  TPM_CC command_code = TPM_CC_GetTestResult;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  std::string command_hash(32, 0);
  hash->Finish(string_as_array(&command_hash), command_hash.size());
  std::string authorization_section_bytes;
  if (!authorization_delegate->GetCommandAuthorization(
      command_hash,
      &authorization_section_bytes)) {
    error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
    return;
  }
  std::string authorization_size_bytes;
  if (!authorization_section_bytes.empty()) {
    tag = TPM_ST_SESSIONS;
    std::string tmp;
    rc = Serialize_UINT32(authorization_section_bytes.size(),
                          &authorization_size_bytes);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    command_size += authorization_size_bytes.size() +
                    authorization_section_bytes.size();
  }
  std::string tag_bytes;
  rc = Serialize_TPMI_ST_COMMAND_TAG(
      tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command_size_bytes;
  rc = Serialize_UINT32(
      command_size,
      &command_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command = tag_bytes +
                        command_size_bytes +
                        command_code_bytes +
                        handle_section_bytes +
                        authorization_size_bytes +
                        authorization_section_bytes +
                        parameter_section_bytes;
  CHECK(command.size() == command_size) << "Command size mismatch!";
  transceiver_->SendCommand(command, parser);
}

void StartAuthSessionErrorCallback(
    const Tpm::StartAuthSessionResponse& callback,
    TPM_RC response_code) {
  callback.Run(response_code,
               TPMI_SH_AUTH_SESSION(),
               TPM2B_NONCE());
}

void StartAuthSessionResponseParser(
    const Tpm::StartAuthSessionResponse& callback,
    AuthorizationDelegate* authorization_delegate,
    const std::string& response) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(StartAuthSessionErrorCallback, callback);
  std::string buffer(response);
  TPM_ST tag;
  std::string tag_bytes;
  rc = Parse_TPM_ST(
      &buffer,
      &tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  UINT32 response_size;
  std::string response_size_bytes;
  rc = Parse_UINT32(
      &buffer,
      &response_size,
      &response_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM_RC response_code;
  std::string response_code_bytes;
  rc = Parse_TPM_RC(
      &buffer,
      &response_code,
      &response_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (response_size != response.size()) {
    error_reporter.Run(TPM_RC_SIZE);
    return;
  }
  if (response_code != TPM_RC_SUCCESS) {
    error_reporter.Run(response_code);
    return;
  }
  TPM_CC command_code = TPM_CC_StartAuthSession;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string authorization_section_bytes;
  if (tag == TPM_ST_SESSIONS) {
    UINT32 parameter_section_size = buffer.size();
    rc = Parse_UINT32(&buffer, &parameter_section_size, NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    if (parameter_section_size > buffer.size()) {
      error_reporter.Run(TPM_RC_INSUFFICIENT);
      return;
    }
    authorization_section_bytes = buffer.substr(parameter_section_size);
    // Keep the parameter section in |buffer|.
    buffer.erase(parameter_section_size);
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(response_code_bytes.data(),
               response_code_bytes.size());
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(buffer.data(),
               buffer.size());
  std::string response_hash(32, 0);
  hash->Finish(string_as_array(&response_hash), response_hash.size());
  if (tag == TPM_ST_SESSIONS) {
    if (!authorization_delegate->CheckResponseAuthorization(
        response_hash,
        authorization_section_bytes)) {
      error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
      return;
    }
  }
  TPMI_SH_AUTH_SESSION session_handle;
  std::string session_handle_bytes;
  rc = Parse_TPMI_SH_AUTH_SESSION(
      &buffer,
      &session_handle,
      &session_handle_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM2B_NONCE nonce_tpm;
  std::string nonce_tpm_bytes;
  rc = Parse_TPM2B_NONCE(
      &buffer,
      &nonce_tpm,
      &nonce_tpm_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (tag == TPM_ST_SESSIONS) {
    // Decrypt just the parameter data, not the size.
    std::string tmp = session_handle_bytes.substr(2);
    if (!authorization_delegate->DecryptResponseParameter(&tmp)) {
      error_reporter.Run(TRUNKS_RC_ENCRYPTION_FAILED);
      return;
    }
    session_handle_bytes.replace(2, std::string::npos, tmp);
    rc = Parse_TPMI_SH_AUTH_SESSION(
        &session_handle_bytes,
        &session_handle,
        NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
  }
  callback.Run(response_code,
               session_handle,
               nonce_tpm);
}

void Tpm::StartAuthSession(
      TPMI_DH_OBJECT tpm_key,
      const std::string& tpm_key_name,
      TPMI_DH_ENTITY bind,
      const std::string& bind_name,
      TPM2B_NONCE nonce_caller,
      TPM2B_ENCRYPTED_SECRET encrypted_salt,
      TPM_SE session_type,
      TPMT_SYM_DEF symmetric,
      TPMI_ALG_HASH auth_hash,
      AuthorizationDelegate* authorization_delegate,
      const StartAuthSessionResponse& callback) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(StartAuthSessionErrorCallback, callback);
  base::Callback<void(const std::string&)> parser =
      base::Bind(StartAuthSessionResponseParser,
                 callback,
                 authorization_delegate);
  TPMI_ST_COMMAND_TAG tag = TPM_ST_NO_SESSIONS;
  UINT32 command_size = 10;  // Header size.
  std::string handle_section_bytes;
  std::string parameter_section_bytes;
  TPM_CC command_code = TPM_CC_StartAuthSession;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string tpm_key_bytes;
  rc = Serialize_TPMI_DH_OBJECT(
      tpm_key,
      &tpm_key_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string bind_bytes;
  rc = Serialize_TPMI_DH_ENTITY(
      bind,
      &bind_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string nonce_caller_bytes;
  rc = Serialize_TPM2B_NONCE(
      nonce_caller,
      &nonce_caller_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string encrypted_salt_bytes;
  rc = Serialize_TPM2B_ENCRYPTED_SECRET(
      encrypted_salt,
      &encrypted_salt_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string session_type_bytes;
  rc = Serialize_TPM_SE(
      session_type,
      &session_type_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string symmetric_bytes;
  rc = Serialize_TPMT_SYM_DEF(
      symmetric,
      &symmetric_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string auth_hash_bytes;
  rc = Serialize_TPMI_ALG_HASH(
      auth_hash,
      &auth_hash_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  // Encrypt just the parameter data, not the size.
  std::string tmp = nonce_caller_bytes.substr(2);
  if (!authorization_delegate->EncryptCommandParameter(&tmp)) {
    error_reporter.Run(TRUNKS_RC_ENCRYPTION_FAILED);
    return;
  }
  nonce_caller_bytes.replace(2, std::string::npos, tmp);
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(tpm_key_name.data(),
               tpm_key_name.size());
  handle_section_bytes += tpm_key_bytes;
  command_size += tpm_key_bytes.size();
  hash->Update(bind_name.data(),
               bind_name.size());
  handle_section_bytes += bind_bytes;
  command_size += bind_bytes.size();
  hash->Update(nonce_caller_bytes.data(),
               nonce_caller_bytes.size());
  parameter_section_bytes += nonce_caller_bytes;
  command_size += nonce_caller_bytes.size();
  hash->Update(encrypted_salt_bytes.data(),
               encrypted_salt_bytes.size());
  parameter_section_bytes += encrypted_salt_bytes;
  command_size += encrypted_salt_bytes.size();
  hash->Update(session_type_bytes.data(),
               session_type_bytes.size());
  parameter_section_bytes += session_type_bytes;
  command_size += session_type_bytes.size();
  hash->Update(symmetric_bytes.data(),
               symmetric_bytes.size());
  parameter_section_bytes += symmetric_bytes;
  command_size += symmetric_bytes.size();
  hash->Update(auth_hash_bytes.data(),
               auth_hash_bytes.size());
  parameter_section_bytes += auth_hash_bytes;
  command_size += auth_hash_bytes.size();
  std::string command_hash(32, 0);
  hash->Finish(string_as_array(&command_hash), command_hash.size());
  std::string authorization_section_bytes;
  if (!authorization_delegate->GetCommandAuthorization(
      command_hash,
      &authorization_section_bytes)) {
    error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
    return;
  }
  std::string authorization_size_bytes;
  if (!authorization_section_bytes.empty()) {
    tag = TPM_ST_SESSIONS;
    std::string tmp;
    rc = Serialize_UINT32(authorization_section_bytes.size(),
                          &authorization_size_bytes);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    command_size += authorization_size_bytes.size() +
                    authorization_section_bytes.size();
  }
  std::string tag_bytes;
  rc = Serialize_TPMI_ST_COMMAND_TAG(
      tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command_size_bytes;
  rc = Serialize_UINT32(
      command_size,
      &command_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command = tag_bytes +
                        command_size_bytes +
                        command_code_bytes +
                        handle_section_bytes +
                        authorization_size_bytes +
                        authorization_section_bytes +
                        parameter_section_bytes;
  CHECK(command.size() == command_size) << "Command size mismatch!";
  transceiver_->SendCommand(command, parser);
}

void PolicyRestartErrorCallback(
    const Tpm::PolicyRestartResponse& callback,
    TPM_RC response_code) {
  callback.Run(response_code);
}

void PolicyRestartResponseParser(
    const Tpm::PolicyRestartResponse& callback,
    AuthorizationDelegate* authorization_delegate,
    const std::string& response) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(PolicyRestartErrorCallback, callback);
  std::string buffer(response);
  TPM_ST tag;
  std::string tag_bytes;
  rc = Parse_TPM_ST(
      &buffer,
      &tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  UINT32 response_size;
  std::string response_size_bytes;
  rc = Parse_UINT32(
      &buffer,
      &response_size,
      &response_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM_RC response_code;
  std::string response_code_bytes;
  rc = Parse_TPM_RC(
      &buffer,
      &response_code,
      &response_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (response_size != response.size()) {
    error_reporter.Run(TPM_RC_SIZE);
    return;
  }
  if (response_code != TPM_RC_SUCCESS) {
    error_reporter.Run(response_code);
    return;
  }
  TPM_CC command_code = TPM_CC_PolicyRestart;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string authorization_section_bytes;
  if (tag == TPM_ST_SESSIONS) {
    UINT32 parameter_section_size = buffer.size();
    rc = Parse_UINT32(&buffer, &parameter_section_size, NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    if (parameter_section_size > buffer.size()) {
      error_reporter.Run(TPM_RC_INSUFFICIENT);
      return;
    }
    authorization_section_bytes = buffer.substr(parameter_section_size);
    // Keep the parameter section in |buffer|.
    buffer.erase(parameter_section_size);
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(response_code_bytes.data(),
               response_code_bytes.size());
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(buffer.data(),
               buffer.size());
  std::string response_hash(32, 0);
  hash->Finish(string_as_array(&response_hash), response_hash.size());
  if (tag == TPM_ST_SESSIONS) {
    if (!authorization_delegate->CheckResponseAuthorization(
        response_hash,
        authorization_section_bytes)) {
      error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
      return;
    }
  }
  callback.Run(response_code);
}

void Tpm::PolicyRestart(
      TPMI_SH_POLICY session_handle,
      const std::string& session_handle_name,
      AuthorizationDelegate* authorization_delegate,
      const PolicyRestartResponse& callback) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(PolicyRestartErrorCallback, callback);
  base::Callback<void(const std::string&)> parser =
      base::Bind(PolicyRestartResponseParser,
                 callback,
                 authorization_delegate);
  TPMI_ST_COMMAND_TAG tag = TPM_ST_NO_SESSIONS;
  UINT32 command_size = 10;  // Header size.
  std::string handle_section_bytes;
  std::string parameter_section_bytes;
  TPM_CC command_code = TPM_CC_PolicyRestart;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string session_handle_bytes;
  rc = Serialize_TPMI_SH_POLICY(
      session_handle,
      &session_handle_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(session_handle_name.data(),
               session_handle_name.size());
  handle_section_bytes += session_handle_bytes;
  command_size += session_handle_bytes.size();
  std::string command_hash(32, 0);
  hash->Finish(string_as_array(&command_hash), command_hash.size());
  std::string authorization_section_bytes;
  if (!authorization_delegate->GetCommandAuthorization(
      command_hash,
      &authorization_section_bytes)) {
    error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
    return;
  }
  std::string authorization_size_bytes;
  if (!authorization_section_bytes.empty()) {
    tag = TPM_ST_SESSIONS;
    std::string tmp;
    rc = Serialize_UINT32(authorization_section_bytes.size(),
                          &authorization_size_bytes);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    command_size += authorization_size_bytes.size() +
                    authorization_section_bytes.size();
  }
  std::string tag_bytes;
  rc = Serialize_TPMI_ST_COMMAND_TAG(
      tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command_size_bytes;
  rc = Serialize_UINT32(
      command_size,
      &command_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command = tag_bytes +
                        command_size_bytes +
                        command_code_bytes +
                        handle_section_bytes +
                        authorization_size_bytes +
                        authorization_section_bytes +
                        parameter_section_bytes;
  CHECK(command.size() == command_size) << "Command size mismatch!";
  transceiver_->SendCommand(command, parser);
}

void CreateErrorCallback(
    const Tpm::CreateResponse& callback,
    TPM_RC response_code) {
  callback.Run(response_code,
               TPM2B_PRIVATE(),
               TPM2B_PUBLIC(),
               TPM2B_CREATION_DATA(),
               TPM2B_DIGEST(),
               TPMT_TK_CREATION());
}

void CreateResponseParser(
    const Tpm::CreateResponse& callback,
    AuthorizationDelegate* authorization_delegate,
    const std::string& response) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(CreateErrorCallback, callback);
  std::string buffer(response);
  TPM_ST tag;
  std::string tag_bytes;
  rc = Parse_TPM_ST(
      &buffer,
      &tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  UINT32 response_size;
  std::string response_size_bytes;
  rc = Parse_UINT32(
      &buffer,
      &response_size,
      &response_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM_RC response_code;
  std::string response_code_bytes;
  rc = Parse_TPM_RC(
      &buffer,
      &response_code,
      &response_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (response_size != response.size()) {
    error_reporter.Run(TPM_RC_SIZE);
    return;
  }
  if (response_code != TPM_RC_SUCCESS) {
    error_reporter.Run(response_code);
    return;
  }
  TPM_CC command_code = TPM_CC_Create;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string authorization_section_bytes;
  if (tag == TPM_ST_SESSIONS) {
    UINT32 parameter_section_size = buffer.size();
    rc = Parse_UINT32(&buffer, &parameter_section_size, NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    if (parameter_section_size > buffer.size()) {
      error_reporter.Run(TPM_RC_INSUFFICIENT);
      return;
    }
    authorization_section_bytes = buffer.substr(parameter_section_size);
    // Keep the parameter section in |buffer|.
    buffer.erase(parameter_section_size);
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(response_code_bytes.data(),
               response_code_bytes.size());
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(buffer.data(),
               buffer.size());
  std::string response_hash(32, 0);
  hash->Finish(string_as_array(&response_hash), response_hash.size());
  if (tag == TPM_ST_SESSIONS) {
    if (!authorization_delegate->CheckResponseAuthorization(
        response_hash,
        authorization_section_bytes)) {
      error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
      return;
    }
  }
  TPM2B_PRIVATE out_private;
  std::string out_private_bytes;
  rc = Parse_TPM2B_PRIVATE(
      &buffer,
      &out_private,
      &out_private_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM2B_PUBLIC out_public;
  std::string out_public_bytes;
  rc = Parse_TPM2B_PUBLIC(
      &buffer,
      &out_public,
      &out_public_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM2B_CREATION_DATA creation_data;
  std::string creation_data_bytes;
  rc = Parse_TPM2B_CREATION_DATA(
      &buffer,
      &creation_data,
      &creation_data_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM2B_DIGEST creation_hash;
  std::string creation_hash_bytes;
  rc = Parse_TPM2B_DIGEST(
      &buffer,
      &creation_hash,
      &creation_hash_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPMT_TK_CREATION creation_ticket;
  std::string creation_ticket_bytes;
  rc = Parse_TPMT_TK_CREATION(
      &buffer,
      &creation_ticket,
      &creation_ticket_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (tag == TPM_ST_SESSIONS) {
    // Decrypt just the parameter data, not the size.
    std::string tmp = out_private_bytes.substr(2);
    if (!authorization_delegate->DecryptResponseParameter(&tmp)) {
      error_reporter.Run(TRUNKS_RC_ENCRYPTION_FAILED);
      return;
    }
    out_private_bytes.replace(2, std::string::npos, tmp);
    rc = Parse_TPM2B_PRIVATE(
        &out_private_bytes,
        &out_private,
        NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
  }
  callback.Run(response_code,
               out_private,
               out_public,
               creation_data,
               creation_hash,
               creation_ticket);
}

void Tpm::Create(
      TPMI_DH_OBJECT parent_handle,
      const std::string& parent_handle_name,
      TPM2B_SENSITIVE_CREATE in_sensitive,
      TPM2B_PUBLIC in_public,
      TPM2B_DATA outside_info,
      TPML_PCR_SELECTION creation_pcr,
      AuthorizationDelegate* authorization_delegate,
      const CreateResponse& callback) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(CreateErrorCallback, callback);
  base::Callback<void(const std::string&)> parser =
      base::Bind(CreateResponseParser,
                 callback,
                 authorization_delegate);
  TPMI_ST_COMMAND_TAG tag = TPM_ST_NO_SESSIONS;
  UINT32 command_size = 10;  // Header size.
  std::string handle_section_bytes;
  std::string parameter_section_bytes;
  TPM_CC command_code = TPM_CC_Create;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string parent_handle_bytes;
  rc = Serialize_TPMI_DH_OBJECT(
      parent_handle,
      &parent_handle_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string in_sensitive_bytes;
  rc = Serialize_TPM2B_SENSITIVE_CREATE(
      in_sensitive,
      &in_sensitive_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string in_public_bytes;
  rc = Serialize_TPM2B_PUBLIC(
      in_public,
      &in_public_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string outside_info_bytes;
  rc = Serialize_TPM2B_DATA(
      outside_info,
      &outside_info_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string creation_pcr_bytes;
  rc = Serialize_TPML_PCR_SELECTION(
      creation_pcr,
      &creation_pcr_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  // Encrypt just the parameter data, not the size.
  std::string tmp = in_sensitive_bytes.substr(2);
  if (!authorization_delegate->EncryptCommandParameter(&tmp)) {
    error_reporter.Run(TRUNKS_RC_ENCRYPTION_FAILED);
    return;
  }
  in_sensitive_bytes.replace(2, std::string::npos, tmp);
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(parent_handle_name.data(),
               parent_handle_name.size());
  handle_section_bytes += parent_handle_bytes;
  command_size += parent_handle_bytes.size();
  hash->Update(in_sensitive_bytes.data(),
               in_sensitive_bytes.size());
  parameter_section_bytes += in_sensitive_bytes;
  command_size += in_sensitive_bytes.size();
  hash->Update(in_public_bytes.data(),
               in_public_bytes.size());
  parameter_section_bytes += in_public_bytes;
  command_size += in_public_bytes.size();
  hash->Update(outside_info_bytes.data(),
               outside_info_bytes.size());
  parameter_section_bytes += outside_info_bytes;
  command_size += outside_info_bytes.size();
  hash->Update(creation_pcr_bytes.data(),
               creation_pcr_bytes.size());
  parameter_section_bytes += creation_pcr_bytes;
  command_size += creation_pcr_bytes.size();
  std::string command_hash(32, 0);
  hash->Finish(string_as_array(&command_hash), command_hash.size());
  std::string authorization_section_bytes;
  if (!authorization_delegate->GetCommandAuthorization(
      command_hash,
      &authorization_section_bytes)) {
    error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
    return;
  }
  std::string authorization_size_bytes;
  if (!authorization_section_bytes.empty()) {
    tag = TPM_ST_SESSIONS;
    std::string tmp;
    rc = Serialize_UINT32(authorization_section_bytes.size(),
                          &authorization_size_bytes);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    command_size += authorization_size_bytes.size() +
                    authorization_section_bytes.size();
  }
  std::string tag_bytes;
  rc = Serialize_TPMI_ST_COMMAND_TAG(
      tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command_size_bytes;
  rc = Serialize_UINT32(
      command_size,
      &command_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command = tag_bytes +
                        command_size_bytes +
                        command_code_bytes +
                        handle_section_bytes +
                        authorization_size_bytes +
                        authorization_section_bytes +
                        parameter_section_bytes;
  CHECK(command.size() == command_size) << "Command size mismatch!";
  transceiver_->SendCommand(command, parser);
}

void LoadErrorCallback(
    const Tpm::LoadResponse& callback,
    TPM_RC response_code) {
  callback.Run(response_code,
               TPM_HANDLE(),
               TPM2B_NAME());
}

void LoadResponseParser(
    const Tpm::LoadResponse& callback,
    AuthorizationDelegate* authorization_delegate,
    const std::string& response) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(LoadErrorCallback, callback);
  std::string buffer(response);
  TPM_ST tag;
  std::string tag_bytes;
  rc = Parse_TPM_ST(
      &buffer,
      &tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  UINT32 response_size;
  std::string response_size_bytes;
  rc = Parse_UINT32(
      &buffer,
      &response_size,
      &response_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM_RC response_code;
  std::string response_code_bytes;
  rc = Parse_TPM_RC(
      &buffer,
      &response_code,
      &response_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (response_size != response.size()) {
    error_reporter.Run(TPM_RC_SIZE);
    return;
  }
  if (response_code != TPM_RC_SUCCESS) {
    error_reporter.Run(response_code);
    return;
  }
  TPM_CC command_code = TPM_CC_Load;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string authorization_section_bytes;
  if (tag == TPM_ST_SESSIONS) {
    UINT32 parameter_section_size = buffer.size();
    rc = Parse_UINT32(&buffer, &parameter_section_size, NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    if (parameter_section_size > buffer.size()) {
      error_reporter.Run(TPM_RC_INSUFFICIENT);
      return;
    }
    authorization_section_bytes = buffer.substr(parameter_section_size);
    // Keep the parameter section in |buffer|.
    buffer.erase(parameter_section_size);
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(response_code_bytes.data(),
               response_code_bytes.size());
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(buffer.data(),
               buffer.size());
  std::string response_hash(32, 0);
  hash->Finish(string_as_array(&response_hash), response_hash.size());
  if (tag == TPM_ST_SESSIONS) {
    if (!authorization_delegate->CheckResponseAuthorization(
        response_hash,
        authorization_section_bytes)) {
      error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
      return;
    }
  }
  TPM_HANDLE object_handle;
  std::string object_handle_bytes;
  rc = Parse_TPM_HANDLE(
      &buffer,
      &object_handle,
      &object_handle_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM2B_NAME name;
  std::string name_bytes;
  rc = Parse_TPM2B_NAME(
      &buffer,
      &name,
      &name_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (tag == TPM_ST_SESSIONS) {
    // Decrypt just the parameter data, not the size.
    std::string tmp = object_handle_bytes.substr(2);
    if (!authorization_delegate->DecryptResponseParameter(&tmp)) {
      error_reporter.Run(TRUNKS_RC_ENCRYPTION_FAILED);
      return;
    }
    object_handle_bytes.replace(2, std::string::npos, tmp);
    rc = Parse_TPM_HANDLE(
        &object_handle_bytes,
        &object_handle,
        NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
  }
  callback.Run(response_code,
               object_handle,
               name);
}

void Tpm::Load(
      TPMI_DH_OBJECT parent_handle,
      const std::string& parent_handle_name,
      TPM2B_PRIVATE in_private,
      TPM2B_PUBLIC in_public,
      AuthorizationDelegate* authorization_delegate,
      const LoadResponse& callback) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(LoadErrorCallback, callback);
  base::Callback<void(const std::string&)> parser =
      base::Bind(LoadResponseParser,
                 callback,
                 authorization_delegate);
  TPMI_ST_COMMAND_TAG tag = TPM_ST_NO_SESSIONS;
  UINT32 command_size = 10;  // Header size.
  std::string handle_section_bytes;
  std::string parameter_section_bytes;
  TPM_CC command_code = TPM_CC_Load;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string parent_handle_bytes;
  rc = Serialize_TPMI_DH_OBJECT(
      parent_handle,
      &parent_handle_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string in_private_bytes;
  rc = Serialize_TPM2B_PRIVATE(
      in_private,
      &in_private_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string in_public_bytes;
  rc = Serialize_TPM2B_PUBLIC(
      in_public,
      &in_public_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  // Encrypt just the parameter data, not the size.
  std::string tmp = in_private_bytes.substr(2);
  if (!authorization_delegate->EncryptCommandParameter(&tmp)) {
    error_reporter.Run(TRUNKS_RC_ENCRYPTION_FAILED);
    return;
  }
  in_private_bytes.replace(2, std::string::npos, tmp);
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(parent_handle_name.data(),
               parent_handle_name.size());
  handle_section_bytes += parent_handle_bytes;
  command_size += parent_handle_bytes.size();
  hash->Update(in_private_bytes.data(),
               in_private_bytes.size());
  parameter_section_bytes += in_private_bytes;
  command_size += in_private_bytes.size();
  hash->Update(in_public_bytes.data(),
               in_public_bytes.size());
  parameter_section_bytes += in_public_bytes;
  command_size += in_public_bytes.size();
  std::string command_hash(32, 0);
  hash->Finish(string_as_array(&command_hash), command_hash.size());
  std::string authorization_section_bytes;
  if (!authorization_delegate->GetCommandAuthorization(
      command_hash,
      &authorization_section_bytes)) {
    error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
    return;
  }
  std::string authorization_size_bytes;
  if (!authorization_section_bytes.empty()) {
    tag = TPM_ST_SESSIONS;
    std::string tmp;
    rc = Serialize_UINT32(authorization_section_bytes.size(),
                          &authorization_size_bytes);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    command_size += authorization_size_bytes.size() +
                    authorization_section_bytes.size();
  }
  std::string tag_bytes;
  rc = Serialize_TPMI_ST_COMMAND_TAG(
      tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command_size_bytes;
  rc = Serialize_UINT32(
      command_size,
      &command_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command = tag_bytes +
                        command_size_bytes +
                        command_code_bytes +
                        handle_section_bytes +
                        authorization_size_bytes +
                        authorization_section_bytes +
                        parameter_section_bytes;
  CHECK(command.size() == command_size) << "Command size mismatch!";
  transceiver_->SendCommand(command, parser);
}

void LoadExternalErrorCallback(
    const Tpm::LoadExternalResponse& callback,
    TPM_RC response_code) {
  callback.Run(response_code,
               TPM_HANDLE(),
               TPM2B_NAME());
}

void LoadExternalResponseParser(
    const Tpm::LoadExternalResponse& callback,
    AuthorizationDelegate* authorization_delegate,
    const std::string& response) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(LoadExternalErrorCallback, callback);
  std::string buffer(response);
  TPM_ST tag;
  std::string tag_bytes;
  rc = Parse_TPM_ST(
      &buffer,
      &tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  UINT32 response_size;
  std::string response_size_bytes;
  rc = Parse_UINT32(
      &buffer,
      &response_size,
      &response_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM_RC response_code;
  std::string response_code_bytes;
  rc = Parse_TPM_RC(
      &buffer,
      &response_code,
      &response_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (response_size != response.size()) {
    error_reporter.Run(TPM_RC_SIZE);
    return;
  }
  if (response_code != TPM_RC_SUCCESS) {
    error_reporter.Run(response_code);
    return;
  }
  TPM_CC command_code = TPM_CC_LoadExternal;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string authorization_section_bytes;
  if (tag == TPM_ST_SESSIONS) {
    UINT32 parameter_section_size = buffer.size();
    rc = Parse_UINT32(&buffer, &parameter_section_size, NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    if (parameter_section_size > buffer.size()) {
      error_reporter.Run(TPM_RC_INSUFFICIENT);
      return;
    }
    authorization_section_bytes = buffer.substr(parameter_section_size);
    // Keep the parameter section in |buffer|.
    buffer.erase(parameter_section_size);
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(response_code_bytes.data(),
               response_code_bytes.size());
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(buffer.data(),
               buffer.size());
  std::string response_hash(32, 0);
  hash->Finish(string_as_array(&response_hash), response_hash.size());
  if (tag == TPM_ST_SESSIONS) {
    if (!authorization_delegate->CheckResponseAuthorization(
        response_hash,
        authorization_section_bytes)) {
      error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
      return;
    }
  }
  TPM_HANDLE object_handle;
  std::string object_handle_bytes;
  rc = Parse_TPM_HANDLE(
      &buffer,
      &object_handle,
      &object_handle_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM2B_NAME name;
  std::string name_bytes;
  rc = Parse_TPM2B_NAME(
      &buffer,
      &name,
      &name_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (tag == TPM_ST_SESSIONS) {
    // Decrypt just the parameter data, not the size.
    std::string tmp = object_handle_bytes.substr(2);
    if (!authorization_delegate->DecryptResponseParameter(&tmp)) {
      error_reporter.Run(TRUNKS_RC_ENCRYPTION_FAILED);
      return;
    }
    object_handle_bytes.replace(2, std::string::npos, tmp);
    rc = Parse_TPM_HANDLE(
        &object_handle_bytes,
        &object_handle,
        NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
  }
  callback.Run(response_code,
               object_handle,
               name);
}

void Tpm::LoadExternal(
      TPMI_RH_HIERARCHY hierarchy,
      const std::string& hierarchy_name,
      TPM2B_SENSITIVE in_private,
      TPM2B_PUBLIC in_public,
      AuthorizationDelegate* authorization_delegate,
      const LoadExternalResponse& callback) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(LoadExternalErrorCallback, callback);
  base::Callback<void(const std::string&)> parser =
      base::Bind(LoadExternalResponseParser,
                 callback,
                 authorization_delegate);
  TPMI_ST_COMMAND_TAG tag = TPM_ST_NO_SESSIONS;
  UINT32 command_size = 10;  // Header size.
  std::string handle_section_bytes;
  std::string parameter_section_bytes;
  TPM_CC command_code = TPM_CC_LoadExternal;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string in_private_bytes;
  rc = Serialize_TPM2B_SENSITIVE(
      in_private,
      &in_private_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string in_public_bytes;
  rc = Serialize_TPM2B_PUBLIC(
      in_public,
      &in_public_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string hierarchy_bytes;
  rc = Serialize_TPMI_RH_HIERARCHY(
      hierarchy,
      &hierarchy_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  // Encrypt just the parameter data, not the size.
  std::string tmp = in_private_bytes.substr(2);
  if (!authorization_delegate->EncryptCommandParameter(&tmp)) {
    error_reporter.Run(TRUNKS_RC_ENCRYPTION_FAILED);
    return;
  }
  in_private_bytes.replace(2, std::string::npos, tmp);
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(hierarchy_name.data(),
               hierarchy_name.size());
  handle_section_bytes += hierarchy_bytes;
  command_size += hierarchy_bytes.size();
  hash->Update(in_private_bytes.data(),
               in_private_bytes.size());
  parameter_section_bytes += in_private_bytes;
  command_size += in_private_bytes.size();
  hash->Update(in_public_bytes.data(),
               in_public_bytes.size());
  parameter_section_bytes += in_public_bytes;
  command_size += in_public_bytes.size();
  std::string command_hash(32, 0);
  hash->Finish(string_as_array(&command_hash), command_hash.size());
  std::string authorization_section_bytes;
  if (!authorization_delegate->GetCommandAuthorization(
      command_hash,
      &authorization_section_bytes)) {
    error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
    return;
  }
  std::string authorization_size_bytes;
  if (!authorization_section_bytes.empty()) {
    tag = TPM_ST_SESSIONS;
    std::string tmp;
    rc = Serialize_UINT32(authorization_section_bytes.size(),
                          &authorization_size_bytes);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    command_size += authorization_size_bytes.size() +
                    authorization_section_bytes.size();
  }
  std::string tag_bytes;
  rc = Serialize_TPMI_ST_COMMAND_TAG(
      tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command_size_bytes;
  rc = Serialize_UINT32(
      command_size,
      &command_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command = tag_bytes +
                        command_size_bytes +
                        command_code_bytes +
                        handle_section_bytes +
                        authorization_size_bytes +
                        authorization_section_bytes +
                        parameter_section_bytes;
  CHECK(command.size() == command_size) << "Command size mismatch!";
  transceiver_->SendCommand(command, parser);
}

void ReadPublicErrorCallback(
    const Tpm::ReadPublicResponse& callback,
    TPM_RC response_code) {
  callback.Run(response_code,
               TPM2B_PUBLIC(),
               TPM2B_NAME(),
               TPM2B_NAME());
}

void ReadPublicResponseParser(
    const Tpm::ReadPublicResponse& callback,
    AuthorizationDelegate* authorization_delegate,
    const std::string& response) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(ReadPublicErrorCallback, callback);
  std::string buffer(response);
  TPM_ST tag;
  std::string tag_bytes;
  rc = Parse_TPM_ST(
      &buffer,
      &tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  UINT32 response_size;
  std::string response_size_bytes;
  rc = Parse_UINT32(
      &buffer,
      &response_size,
      &response_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM_RC response_code;
  std::string response_code_bytes;
  rc = Parse_TPM_RC(
      &buffer,
      &response_code,
      &response_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (response_size != response.size()) {
    error_reporter.Run(TPM_RC_SIZE);
    return;
  }
  if (response_code != TPM_RC_SUCCESS) {
    error_reporter.Run(response_code);
    return;
  }
  TPM_CC command_code = TPM_CC_ReadPublic;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string authorization_section_bytes;
  if (tag == TPM_ST_SESSIONS) {
    UINT32 parameter_section_size = buffer.size();
    rc = Parse_UINT32(&buffer, &parameter_section_size, NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    if (parameter_section_size > buffer.size()) {
      error_reporter.Run(TPM_RC_INSUFFICIENT);
      return;
    }
    authorization_section_bytes = buffer.substr(parameter_section_size);
    // Keep the parameter section in |buffer|.
    buffer.erase(parameter_section_size);
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(response_code_bytes.data(),
               response_code_bytes.size());
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(buffer.data(),
               buffer.size());
  std::string response_hash(32, 0);
  hash->Finish(string_as_array(&response_hash), response_hash.size());
  if (tag == TPM_ST_SESSIONS) {
    if (!authorization_delegate->CheckResponseAuthorization(
        response_hash,
        authorization_section_bytes)) {
      error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
      return;
    }
  }
  TPM2B_PUBLIC out_public;
  std::string out_public_bytes;
  rc = Parse_TPM2B_PUBLIC(
      &buffer,
      &out_public,
      &out_public_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM2B_NAME name;
  std::string name_bytes;
  rc = Parse_TPM2B_NAME(
      &buffer,
      &name,
      &name_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM2B_NAME qualified_name;
  std::string qualified_name_bytes;
  rc = Parse_TPM2B_NAME(
      &buffer,
      &qualified_name,
      &qualified_name_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (tag == TPM_ST_SESSIONS) {
    // Decrypt just the parameter data, not the size.
    std::string tmp = out_public_bytes.substr(2);
    if (!authorization_delegate->DecryptResponseParameter(&tmp)) {
      error_reporter.Run(TRUNKS_RC_ENCRYPTION_FAILED);
      return;
    }
    out_public_bytes.replace(2, std::string::npos, tmp);
    rc = Parse_TPM2B_PUBLIC(
        &out_public_bytes,
        &out_public,
        NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
  }
  callback.Run(response_code,
               out_public,
               name,
               qualified_name);
}

void Tpm::ReadPublic(
      TPMI_DH_OBJECT object_handle,
      const std::string& object_handle_name,
      AuthorizationDelegate* authorization_delegate,
      const ReadPublicResponse& callback) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(ReadPublicErrorCallback, callback);
  base::Callback<void(const std::string&)> parser =
      base::Bind(ReadPublicResponseParser,
                 callback,
                 authorization_delegate);
  TPMI_ST_COMMAND_TAG tag = TPM_ST_NO_SESSIONS;
  UINT32 command_size = 10;  // Header size.
  std::string handle_section_bytes;
  std::string parameter_section_bytes;
  TPM_CC command_code = TPM_CC_ReadPublic;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string object_handle_bytes;
  rc = Serialize_TPMI_DH_OBJECT(
      object_handle,
      &object_handle_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(object_handle_name.data(),
               object_handle_name.size());
  handle_section_bytes += object_handle_bytes;
  command_size += object_handle_bytes.size();
  std::string command_hash(32, 0);
  hash->Finish(string_as_array(&command_hash), command_hash.size());
  std::string authorization_section_bytes;
  if (!authorization_delegate->GetCommandAuthorization(
      command_hash,
      &authorization_section_bytes)) {
    error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
    return;
  }
  std::string authorization_size_bytes;
  if (!authorization_section_bytes.empty()) {
    tag = TPM_ST_SESSIONS;
    std::string tmp;
    rc = Serialize_UINT32(authorization_section_bytes.size(),
                          &authorization_size_bytes);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    command_size += authorization_size_bytes.size() +
                    authorization_section_bytes.size();
  }
  std::string tag_bytes;
  rc = Serialize_TPMI_ST_COMMAND_TAG(
      tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command_size_bytes;
  rc = Serialize_UINT32(
      command_size,
      &command_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command = tag_bytes +
                        command_size_bytes +
                        command_code_bytes +
                        handle_section_bytes +
                        authorization_size_bytes +
                        authorization_section_bytes +
                        parameter_section_bytes;
  CHECK(command.size() == command_size) << "Command size mismatch!";
  transceiver_->SendCommand(command, parser);
}

void ActivateCredentialErrorCallback(
    const Tpm::ActivateCredentialResponse& callback,
    TPM_RC response_code) {
  callback.Run(response_code,
               TPM2B_DIGEST());
}

void ActivateCredentialResponseParser(
    const Tpm::ActivateCredentialResponse& callback,
    AuthorizationDelegate* authorization_delegate,
    const std::string& response) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(ActivateCredentialErrorCallback, callback);
  std::string buffer(response);
  TPM_ST tag;
  std::string tag_bytes;
  rc = Parse_TPM_ST(
      &buffer,
      &tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  UINT32 response_size;
  std::string response_size_bytes;
  rc = Parse_UINT32(
      &buffer,
      &response_size,
      &response_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM_RC response_code;
  std::string response_code_bytes;
  rc = Parse_TPM_RC(
      &buffer,
      &response_code,
      &response_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (response_size != response.size()) {
    error_reporter.Run(TPM_RC_SIZE);
    return;
  }
  if (response_code != TPM_RC_SUCCESS) {
    error_reporter.Run(response_code);
    return;
  }
  TPM_CC command_code = TPM_CC_ActivateCredential;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string authorization_section_bytes;
  if (tag == TPM_ST_SESSIONS) {
    UINT32 parameter_section_size = buffer.size();
    rc = Parse_UINT32(&buffer, &parameter_section_size, NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    if (parameter_section_size > buffer.size()) {
      error_reporter.Run(TPM_RC_INSUFFICIENT);
      return;
    }
    authorization_section_bytes = buffer.substr(parameter_section_size);
    // Keep the parameter section in |buffer|.
    buffer.erase(parameter_section_size);
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(response_code_bytes.data(),
               response_code_bytes.size());
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(buffer.data(),
               buffer.size());
  std::string response_hash(32, 0);
  hash->Finish(string_as_array(&response_hash), response_hash.size());
  if (tag == TPM_ST_SESSIONS) {
    if (!authorization_delegate->CheckResponseAuthorization(
        response_hash,
        authorization_section_bytes)) {
      error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
      return;
    }
  }
  TPM2B_DIGEST cert_info;
  std::string cert_info_bytes;
  rc = Parse_TPM2B_DIGEST(
      &buffer,
      &cert_info,
      &cert_info_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (tag == TPM_ST_SESSIONS) {
    // Decrypt just the parameter data, not the size.
    std::string tmp = cert_info_bytes.substr(2);
    if (!authorization_delegate->DecryptResponseParameter(&tmp)) {
      error_reporter.Run(TRUNKS_RC_ENCRYPTION_FAILED);
      return;
    }
    cert_info_bytes.replace(2, std::string::npos, tmp);
    rc = Parse_TPM2B_DIGEST(
        &cert_info_bytes,
        &cert_info,
        NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
  }
  callback.Run(response_code,
               cert_info);
}

void Tpm::ActivateCredential(
      TPMI_DH_OBJECT activate_handle,
      const std::string& activate_handle_name,
      TPMI_DH_OBJECT key_handle,
      const std::string& key_handle_name,
      TPM2B_ID_OBJECT credential_blob,
      TPM2B_ENCRYPTED_SECRET secret,
      AuthorizationDelegate* authorization_delegate,
      const ActivateCredentialResponse& callback) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(ActivateCredentialErrorCallback, callback);
  base::Callback<void(const std::string&)> parser =
      base::Bind(ActivateCredentialResponseParser,
                 callback,
                 authorization_delegate);
  TPMI_ST_COMMAND_TAG tag = TPM_ST_NO_SESSIONS;
  UINT32 command_size = 10;  // Header size.
  std::string handle_section_bytes;
  std::string parameter_section_bytes;
  TPM_CC command_code = TPM_CC_ActivateCredential;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string activate_handle_bytes;
  rc = Serialize_TPMI_DH_OBJECT(
      activate_handle,
      &activate_handle_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string key_handle_bytes;
  rc = Serialize_TPMI_DH_OBJECT(
      key_handle,
      &key_handle_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string credential_blob_bytes;
  rc = Serialize_TPM2B_ID_OBJECT(
      credential_blob,
      &credential_blob_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string secret_bytes;
  rc = Serialize_TPM2B_ENCRYPTED_SECRET(
      secret,
      &secret_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  // Encrypt just the parameter data, not the size.
  std::string tmp = credential_blob_bytes.substr(2);
  if (!authorization_delegate->EncryptCommandParameter(&tmp)) {
    error_reporter.Run(TRUNKS_RC_ENCRYPTION_FAILED);
    return;
  }
  credential_blob_bytes.replace(2, std::string::npos, tmp);
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(activate_handle_name.data(),
               activate_handle_name.size());
  handle_section_bytes += activate_handle_bytes;
  command_size += activate_handle_bytes.size();
  hash->Update(key_handle_name.data(),
               key_handle_name.size());
  handle_section_bytes += key_handle_bytes;
  command_size += key_handle_bytes.size();
  hash->Update(credential_blob_bytes.data(),
               credential_blob_bytes.size());
  parameter_section_bytes += credential_blob_bytes;
  command_size += credential_blob_bytes.size();
  hash->Update(secret_bytes.data(),
               secret_bytes.size());
  parameter_section_bytes += secret_bytes;
  command_size += secret_bytes.size();
  std::string command_hash(32, 0);
  hash->Finish(string_as_array(&command_hash), command_hash.size());
  std::string authorization_section_bytes;
  if (!authorization_delegate->GetCommandAuthorization(
      command_hash,
      &authorization_section_bytes)) {
    error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
    return;
  }
  std::string authorization_size_bytes;
  if (!authorization_section_bytes.empty()) {
    tag = TPM_ST_SESSIONS;
    std::string tmp;
    rc = Serialize_UINT32(authorization_section_bytes.size(),
                          &authorization_size_bytes);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    command_size += authorization_size_bytes.size() +
                    authorization_section_bytes.size();
  }
  std::string tag_bytes;
  rc = Serialize_TPMI_ST_COMMAND_TAG(
      tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command_size_bytes;
  rc = Serialize_UINT32(
      command_size,
      &command_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command = tag_bytes +
                        command_size_bytes +
                        command_code_bytes +
                        handle_section_bytes +
                        authorization_size_bytes +
                        authorization_section_bytes +
                        parameter_section_bytes;
  CHECK(command.size() == command_size) << "Command size mismatch!";
  transceiver_->SendCommand(command, parser);
}

void MakeCredentialErrorCallback(
    const Tpm::MakeCredentialResponse& callback,
    TPM_RC response_code) {
  callback.Run(response_code,
               TPM2B_ID_OBJECT());
}

void MakeCredentialResponseParser(
    const Tpm::MakeCredentialResponse& callback,
    AuthorizationDelegate* authorization_delegate,
    const std::string& response) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(MakeCredentialErrorCallback, callback);
  std::string buffer(response);
  TPM_ST tag;
  std::string tag_bytes;
  rc = Parse_TPM_ST(
      &buffer,
      &tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  UINT32 response_size;
  std::string response_size_bytes;
  rc = Parse_UINT32(
      &buffer,
      &response_size,
      &response_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM_RC response_code;
  std::string response_code_bytes;
  rc = Parse_TPM_RC(
      &buffer,
      &response_code,
      &response_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (response_size != response.size()) {
    error_reporter.Run(TPM_RC_SIZE);
    return;
  }
  if (response_code != TPM_RC_SUCCESS) {
    error_reporter.Run(response_code);
    return;
  }
  TPM_CC command_code = TPM_CC_MakeCredential;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string authorization_section_bytes;
  if (tag == TPM_ST_SESSIONS) {
    UINT32 parameter_section_size = buffer.size();
    rc = Parse_UINT32(&buffer, &parameter_section_size, NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    if (parameter_section_size > buffer.size()) {
      error_reporter.Run(TPM_RC_INSUFFICIENT);
      return;
    }
    authorization_section_bytes = buffer.substr(parameter_section_size);
    // Keep the parameter section in |buffer|.
    buffer.erase(parameter_section_size);
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(response_code_bytes.data(),
               response_code_bytes.size());
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(buffer.data(),
               buffer.size());
  std::string response_hash(32, 0);
  hash->Finish(string_as_array(&response_hash), response_hash.size());
  if (tag == TPM_ST_SESSIONS) {
    if (!authorization_delegate->CheckResponseAuthorization(
        response_hash,
        authorization_section_bytes)) {
      error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
      return;
    }
  }
  TPM2B_ID_OBJECT credential_blob;
  std::string credential_blob_bytes;
  rc = Parse_TPM2B_ID_OBJECT(
      &buffer,
      &credential_blob,
      &credential_blob_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (tag == TPM_ST_SESSIONS) {
    // Decrypt just the parameter data, not the size.
    std::string tmp = credential_blob_bytes.substr(2);
    if (!authorization_delegate->DecryptResponseParameter(&tmp)) {
      error_reporter.Run(TRUNKS_RC_ENCRYPTION_FAILED);
      return;
    }
    credential_blob_bytes.replace(2, std::string::npos, tmp);
    rc = Parse_TPM2B_ID_OBJECT(
        &credential_blob_bytes,
        &credential_blob,
        NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
  }
  callback.Run(response_code,
               credential_blob);
}

void Tpm::MakeCredential(
      TPMI_DH_OBJECT handle,
      const std::string& handle_name,
      TPM2B_DIGEST credential,
      TPM2B_NAME object_name,
      AuthorizationDelegate* authorization_delegate,
      const MakeCredentialResponse& callback) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(MakeCredentialErrorCallback, callback);
  base::Callback<void(const std::string&)> parser =
      base::Bind(MakeCredentialResponseParser,
                 callback,
                 authorization_delegate);
  TPMI_ST_COMMAND_TAG tag = TPM_ST_NO_SESSIONS;
  UINT32 command_size = 10;  // Header size.
  std::string handle_section_bytes;
  std::string parameter_section_bytes;
  TPM_CC command_code = TPM_CC_MakeCredential;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string handle_bytes;
  rc = Serialize_TPMI_DH_OBJECT(
      handle,
      &handle_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string credential_bytes;
  rc = Serialize_TPM2B_DIGEST(
      credential,
      &credential_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string object_name_bytes;
  rc = Serialize_TPM2B_NAME(
      object_name,
      &object_name_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  // Encrypt just the parameter data, not the size.
  std::string tmp = credential_bytes.substr(2);
  if (!authorization_delegate->EncryptCommandParameter(&tmp)) {
    error_reporter.Run(TRUNKS_RC_ENCRYPTION_FAILED);
    return;
  }
  credential_bytes.replace(2, std::string::npos, tmp);
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(handle_name.data(),
               handle_name.size());
  handle_section_bytes += handle_bytes;
  command_size += handle_bytes.size();
  hash->Update(credential_bytes.data(),
               credential_bytes.size());
  parameter_section_bytes += credential_bytes;
  command_size += credential_bytes.size();
  hash->Update(object_name_bytes.data(),
               object_name_bytes.size());
  parameter_section_bytes += object_name_bytes;
  command_size += object_name_bytes.size();
  std::string command_hash(32, 0);
  hash->Finish(string_as_array(&command_hash), command_hash.size());
  std::string authorization_section_bytes;
  if (!authorization_delegate->GetCommandAuthorization(
      command_hash,
      &authorization_section_bytes)) {
    error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
    return;
  }
  std::string authorization_size_bytes;
  if (!authorization_section_bytes.empty()) {
    tag = TPM_ST_SESSIONS;
    std::string tmp;
    rc = Serialize_UINT32(authorization_section_bytes.size(),
                          &authorization_size_bytes);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    command_size += authorization_size_bytes.size() +
                    authorization_section_bytes.size();
  }
  std::string tag_bytes;
  rc = Serialize_TPMI_ST_COMMAND_TAG(
      tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command_size_bytes;
  rc = Serialize_UINT32(
      command_size,
      &command_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command = tag_bytes +
                        command_size_bytes +
                        command_code_bytes +
                        handle_section_bytes +
                        authorization_size_bytes +
                        authorization_section_bytes +
                        parameter_section_bytes;
  CHECK(command.size() == command_size) << "Command size mismatch!";
  transceiver_->SendCommand(command, parser);
}

void UnsealErrorCallback(
    const Tpm::UnsealResponse& callback,
    TPM_RC response_code) {
  callback.Run(response_code,
               TPM2B_SENSITIVE_DATA());
}

void UnsealResponseParser(
    const Tpm::UnsealResponse& callback,
    AuthorizationDelegate* authorization_delegate,
    const std::string& response) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(UnsealErrorCallback, callback);
  std::string buffer(response);
  TPM_ST tag;
  std::string tag_bytes;
  rc = Parse_TPM_ST(
      &buffer,
      &tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  UINT32 response_size;
  std::string response_size_bytes;
  rc = Parse_UINT32(
      &buffer,
      &response_size,
      &response_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM_RC response_code;
  std::string response_code_bytes;
  rc = Parse_TPM_RC(
      &buffer,
      &response_code,
      &response_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (response_size != response.size()) {
    error_reporter.Run(TPM_RC_SIZE);
    return;
  }
  if (response_code != TPM_RC_SUCCESS) {
    error_reporter.Run(response_code);
    return;
  }
  TPM_CC command_code = TPM_CC_Unseal;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string authorization_section_bytes;
  if (tag == TPM_ST_SESSIONS) {
    UINT32 parameter_section_size = buffer.size();
    rc = Parse_UINT32(&buffer, &parameter_section_size, NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    if (parameter_section_size > buffer.size()) {
      error_reporter.Run(TPM_RC_INSUFFICIENT);
      return;
    }
    authorization_section_bytes = buffer.substr(parameter_section_size);
    // Keep the parameter section in |buffer|.
    buffer.erase(parameter_section_size);
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(response_code_bytes.data(),
               response_code_bytes.size());
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(buffer.data(),
               buffer.size());
  std::string response_hash(32, 0);
  hash->Finish(string_as_array(&response_hash), response_hash.size());
  if (tag == TPM_ST_SESSIONS) {
    if (!authorization_delegate->CheckResponseAuthorization(
        response_hash,
        authorization_section_bytes)) {
      error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
      return;
    }
  }
  TPM2B_SENSITIVE_DATA out_data;
  std::string out_data_bytes;
  rc = Parse_TPM2B_SENSITIVE_DATA(
      &buffer,
      &out_data,
      &out_data_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (tag == TPM_ST_SESSIONS) {
    // Decrypt just the parameter data, not the size.
    std::string tmp = out_data_bytes.substr(2);
    if (!authorization_delegate->DecryptResponseParameter(&tmp)) {
      error_reporter.Run(TRUNKS_RC_ENCRYPTION_FAILED);
      return;
    }
    out_data_bytes.replace(2, std::string::npos, tmp);
    rc = Parse_TPM2B_SENSITIVE_DATA(
        &out_data_bytes,
        &out_data,
        NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
  }
  callback.Run(response_code,
               out_data);
}

void Tpm::Unseal(
      TPMI_DH_OBJECT item_handle,
      const std::string& item_handle_name,
      AuthorizationDelegate* authorization_delegate,
      const UnsealResponse& callback) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(UnsealErrorCallback, callback);
  base::Callback<void(const std::string&)> parser =
      base::Bind(UnsealResponseParser,
                 callback,
                 authorization_delegate);
  TPMI_ST_COMMAND_TAG tag = TPM_ST_NO_SESSIONS;
  UINT32 command_size = 10;  // Header size.
  std::string handle_section_bytes;
  std::string parameter_section_bytes;
  TPM_CC command_code = TPM_CC_Unseal;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string item_handle_bytes;
  rc = Serialize_TPMI_DH_OBJECT(
      item_handle,
      &item_handle_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(item_handle_name.data(),
               item_handle_name.size());
  handle_section_bytes += item_handle_bytes;
  command_size += item_handle_bytes.size();
  std::string command_hash(32, 0);
  hash->Finish(string_as_array(&command_hash), command_hash.size());
  std::string authorization_section_bytes;
  if (!authorization_delegate->GetCommandAuthorization(
      command_hash,
      &authorization_section_bytes)) {
    error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
    return;
  }
  std::string authorization_size_bytes;
  if (!authorization_section_bytes.empty()) {
    tag = TPM_ST_SESSIONS;
    std::string tmp;
    rc = Serialize_UINT32(authorization_section_bytes.size(),
                          &authorization_size_bytes);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    command_size += authorization_size_bytes.size() +
                    authorization_section_bytes.size();
  }
  std::string tag_bytes;
  rc = Serialize_TPMI_ST_COMMAND_TAG(
      tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command_size_bytes;
  rc = Serialize_UINT32(
      command_size,
      &command_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command = tag_bytes +
                        command_size_bytes +
                        command_code_bytes +
                        handle_section_bytes +
                        authorization_size_bytes +
                        authorization_section_bytes +
                        parameter_section_bytes;
  CHECK(command.size() == command_size) << "Command size mismatch!";
  transceiver_->SendCommand(command, parser);
}

void ObjectChangeAuthErrorCallback(
    const Tpm::ObjectChangeAuthResponse& callback,
    TPM_RC response_code) {
  callback.Run(response_code,
               TPM2B_PRIVATE());
}

void ObjectChangeAuthResponseParser(
    const Tpm::ObjectChangeAuthResponse& callback,
    AuthorizationDelegate* authorization_delegate,
    const std::string& response) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(ObjectChangeAuthErrorCallback, callback);
  std::string buffer(response);
  TPM_ST tag;
  std::string tag_bytes;
  rc = Parse_TPM_ST(
      &buffer,
      &tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  UINT32 response_size;
  std::string response_size_bytes;
  rc = Parse_UINT32(
      &buffer,
      &response_size,
      &response_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM_RC response_code;
  std::string response_code_bytes;
  rc = Parse_TPM_RC(
      &buffer,
      &response_code,
      &response_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (response_size != response.size()) {
    error_reporter.Run(TPM_RC_SIZE);
    return;
  }
  if (response_code != TPM_RC_SUCCESS) {
    error_reporter.Run(response_code);
    return;
  }
  TPM_CC command_code = TPM_CC_ObjectChangeAuth;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string authorization_section_bytes;
  if (tag == TPM_ST_SESSIONS) {
    UINT32 parameter_section_size = buffer.size();
    rc = Parse_UINT32(&buffer, &parameter_section_size, NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    if (parameter_section_size > buffer.size()) {
      error_reporter.Run(TPM_RC_INSUFFICIENT);
      return;
    }
    authorization_section_bytes = buffer.substr(parameter_section_size);
    // Keep the parameter section in |buffer|.
    buffer.erase(parameter_section_size);
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(response_code_bytes.data(),
               response_code_bytes.size());
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(buffer.data(),
               buffer.size());
  std::string response_hash(32, 0);
  hash->Finish(string_as_array(&response_hash), response_hash.size());
  if (tag == TPM_ST_SESSIONS) {
    if (!authorization_delegate->CheckResponseAuthorization(
        response_hash,
        authorization_section_bytes)) {
      error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
      return;
    }
  }
  TPM2B_PRIVATE out_private;
  std::string out_private_bytes;
  rc = Parse_TPM2B_PRIVATE(
      &buffer,
      &out_private,
      &out_private_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (tag == TPM_ST_SESSIONS) {
    // Decrypt just the parameter data, not the size.
    std::string tmp = out_private_bytes.substr(2);
    if (!authorization_delegate->DecryptResponseParameter(&tmp)) {
      error_reporter.Run(TRUNKS_RC_ENCRYPTION_FAILED);
      return;
    }
    out_private_bytes.replace(2, std::string::npos, tmp);
    rc = Parse_TPM2B_PRIVATE(
        &out_private_bytes,
        &out_private,
        NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
  }
  callback.Run(response_code,
               out_private);
}

void Tpm::ObjectChangeAuth(
      TPMI_DH_OBJECT object_handle,
      const std::string& object_handle_name,
      TPMI_DH_OBJECT parent_handle,
      const std::string& parent_handle_name,
      TPM2B_AUTH new_auth,
      AuthorizationDelegate* authorization_delegate,
      const ObjectChangeAuthResponse& callback) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(ObjectChangeAuthErrorCallback, callback);
  base::Callback<void(const std::string&)> parser =
      base::Bind(ObjectChangeAuthResponseParser,
                 callback,
                 authorization_delegate);
  TPMI_ST_COMMAND_TAG tag = TPM_ST_NO_SESSIONS;
  UINT32 command_size = 10;  // Header size.
  std::string handle_section_bytes;
  std::string parameter_section_bytes;
  TPM_CC command_code = TPM_CC_ObjectChangeAuth;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string object_handle_bytes;
  rc = Serialize_TPMI_DH_OBJECT(
      object_handle,
      &object_handle_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string parent_handle_bytes;
  rc = Serialize_TPMI_DH_OBJECT(
      parent_handle,
      &parent_handle_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string new_auth_bytes;
  rc = Serialize_TPM2B_AUTH(
      new_auth,
      &new_auth_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  // Encrypt just the parameter data, not the size.
  std::string tmp = new_auth_bytes.substr(2);
  if (!authorization_delegate->EncryptCommandParameter(&tmp)) {
    error_reporter.Run(TRUNKS_RC_ENCRYPTION_FAILED);
    return;
  }
  new_auth_bytes.replace(2, std::string::npos, tmp);
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(object_handle_name.data(),
               object_handle_name.size());
  handle_section_bytes += object_handle_bytes;
  command_size += object_handle_bytes.size();
  hash->Update(parent_handle_name.data(),
               parent_handle_name.size());
  handle_section_bytes += parent_handle_bytes;
  command_size += parent_handle_bytes.size();
  hash->Update(new_auth_bytes.data(),
               new_auth_bytes.size());
  parameter_section_bytes += new_auth_bytes;
  command_size += new_auth_bytes.size();
  std::string command_hash(32, 0);
  hash->Finish(string_as_array(&command_hash), command_hash.size());
  std::string authorization_section_bytes;
  if (!authorization_delegate->GetCommandAuthorization(
      command_hash,
      &authorization_section_bytes)) {
    error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
    return;
  }
  std::string authorization_size_bytes;
  if (!authorization_section_bytes.empty()) {
    tag = TPM_ST_SESSIONS;
    std::string tmp;
    rc = Serialize_UINT32(authorization_section_bytes.size(),
                          &authorization_size_bytes);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    command_size += authorization_size_bytes.size() +
                    authorization_section_bytes.size();
  }
  std::string tag_bytes;
  rc = Serialize_TPMI_ST_COMMAND_TAG(
      tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command_size_bytes;
  rc = Serialize_UINT32(
      command_size,
      &command_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command = tag_bytes +
                        command_size_bytes +
                        command_code_bytes +
                        handle_section_bytes +
                        authorization_size_bytes +
                        authorization_section_bytes +
                        parameter_section_bytes;
  CHECK(command.size() == command_size) << "Command size mismatch!";
  transceiver_->SendCommand(command, parser);
}

void DuplicateErrorCallback(
    const Tpm::DuplicateResponse& callback,
    TPM_RC response_code) {
  callback.Run(response_code,
               TPM2B_DATA(),
               TPM2B_PRIVATE());
}

void DuplicateResponseParser(
    const Tpm::DuplicateResponse& callback,
    AuthorizationDelegate* authorization_delegate,
    const std::string& response) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(DuplicateErrorCallback, callback);
  std::string buffer(response);
  TPM_ST tag;
  std::string tag_bytes;
  rc = Parse_TPM_ST(
      &buffer,
      &tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  UINT32 response_size;
  std::string response_size_bytes;
  rc = Parse_UINT32(
      &buffer,
      &response_size,
      &response_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM_RC response_code;
  std::string response_code_bytes;
  rc = Parse_TPM_RC(
      &buffer,
      &response_code,
      &response_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (response_size != response.size()) {
    error_reporter.Run(TPM_RC_SIZE);
    return;
  }
  if (response_code != TPM_RC_SUCCESS) {
    error_reporter.Run(response_code);
    return;
  }
  TPM_CC command_code = TPM_CC_Duplicate;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string authorization_section_bytes;
  if (tag == TPM_ST_SESSIONS) {
    UINT32 parameter_section_size = buffer.size();
    rc = Parse_UINT32(&buffer, &parameter_section_size, NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    if (parameter_section_size > buffer.size()) {
      error_reporter.Run(TPM_RC_INSUFFICIENT);
      return;
    }
    authorization_section_bytes = buffer.substr(parameter_section_size);
    // Keep the parameter section in |buffer|.
    buffer.erase(parameter_section_size);
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(response_code_bytes.data(),
               response_code_bytes.size());
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(buffer.data(),
               buffer.size());
  std::string response_hash(32, 0);
  hash->Finish(string_as_array(&response_hash), response_hash.size());
  if (tag == TPM_ST_SESSIONS) {
    if (!authorization_delegate->CheckResponseAuthorization(
        response_hash,
        authorization_section_bytes)) {
      error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
      return;
    }
  }
  TPM2B_DATA encryption_key_out;
  std::string encryption_key_out_bytes;
  rc = Parse_TPM2B_DATA(
      &buffer,
      &encryption_key_out,
      &encryption_key_out_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM2B_PRIVATE duplicate;
  std::string duplicate_bytes;
  rc = Parse_TPM2B_PRIVATE(
      &buffer,
      &duplicate,
      &duplicate_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (tag == TPM_ST_SESSIONS) {
    // Decrypt just the parameter data, not the size.
    std::string tmp = encryption_key_out_bytes.substr(2);
    if (!authorization_delegate->DecryptResponseParameter(&tmp)) {
      error_reporter.Run(TRUNKS_RC_ENCRYPTION_FAILED);
      return;
    }
    encryption_key_out_bytes.replace(2, std::string::npos, tmp);
    rc = Parse_TPM2B_DATA(
        &encryption_key_out_bytes,
        &encryption_key_out,
        NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
  }
  callback.Run(response_code,
               encryption_key_out,
               duplicate);
}

void Tpm::Duplicate(
      TPMI_DH_OBJECT object_handle,
      const std::string& object_handle_name,
      TPMI_DH_OBJECT new_parent_handle,
      const std::string& new_parent_handle_name,
      TPM2B_DATA encryption_key_in,
      TPMT_SYM_DEF_OBJECT symmetric_alg,
      AuthorizationDelegate* authorization_delegate,
      const DuplicateResponse& callback) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(DuplicateErrorCallback, callback);
  base::Callback<void(const std::string&)> parser =
      base::Bind(DuplicateResponseParser,
                 callback,
                 authorization_delegate);
  TPMI_ST_COMMAND_TAG tag = TPM_ST_NO_SESSIONS;
  UINT32 command_size = 10;  // Header size.
  std::string handle_section_bytes;
  std::string parameter_section_bytes;
  TPM_CC command_code = TPM_CC_Duplicate;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string object_handle_bytes;
  rc = Serialize_TPMI_DH_OBJECT(
      object_handle,
      &object_handle_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string new_parent_handle_bytes;
  rc = Serialize_TPMI_DH_OBJECT(
      new_parent_handle,
      &new_parent_handle_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string encryption_key_in_bytes;
  rc = Serialize_TPM2B_DATA(
      encryption_key_in,
      &encryption_key_in_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string symmetric_alg_bytes;
  rc = Serialize_TPMT_SYM_DEF_OBJECT(
      symmetric_alg,
      &symmetric_alg_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  // Encrypt just the parameter data, not the size.
  std::string tmp = encryption_key_in_bytes.substr(2);
  if (!authorization_delegate->EncryptCommandParameter(&tmp)) {
    error_reporter.Run(TRUNKS_RC_ENCRYPTION_FAILED);
    return;
  }
  encryption_key_in_bytes.replace(2, std::string::npos, tmp);
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(object_handle_name.data(),
               object_handle_name.size());
  handle_section_bytes += object_handle_bytes;
  command_size += object_handle_bytes.size();
  hash->Update(new_parent_handle_name.data(),
               new_parent_handle_name.size());
  handle_section_bytes += new_parent_handle_bytes;
  command_size += new_parent_handle_bytes.size();
  hash->Update(encryption_key_in_bytes.data(),
               encryption_key_in_bytes.size());
  parameter_section_bytes += encryption_key_in_bytes;
  command_size += encryption_key_in_bytes.size();
  hash->Update(symmetric_alg_bytes.data(),
               symmetric_alg_bytes.size());
  parameter_section_bytes += symmetric_alg_bytes;
  command_size += symmetric_alg_bytes.size();
  std::string command_hash(32, 0);
  hash->Finish(string_as_array(&command_hash), command_hash.size());
  std::string authorization_section_bytes;
  if (!authorization_delegate->GetCommandAuthorization(
      command_hash,
      &authorization_section_bytes)) {
    error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
    return;
  }
  std::string authorization_size_bytes;
  if (!authorization_section_bytes.empty()) {
    tag = TPM_ST_SESSIONS;
    std::string tmp;
    rc = Serialize_UINT32(authorization_section_bytes.size(),
                          &authorization_size_bytes);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    command_size += authorization_size_bytes.size() +
                    authorization_section_bytes.size();
  }
  std::string tag_bytes;
  rc = Serialize_TPMI_ST_COMMAND_TAG(
      tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command_size_bytes;
  rc = Serialize_UINT32(
      command_size,
      &command_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command = tag_bytes +
                        command_size_bytes +
                        command_code_bytes +
                        handle_section_bytes +
                        authorization_size_bytes +
                        authorization_section_bytes +
                        parameter_section_bytes;
  CHECK(command.size() == command_size) << "Command size mismatch!";
  transceiver_->SendCommand(command, parser);
}

void RewrapErrorCallback(
    const Tpm::RewrapResponse& callback,
    TPM_RC response_code) {
  callback.Run(response_code,
               TPM2B_PRIVATE());
}

void RewrapResponseParser(
    const Tpm::RewrapResponse& callback,
    AuthorizationDelegate* authorization_delegate,
    const std::string& response) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(RewrapErrorCallback, callback);
  std::string buffer(response);
  TPM_ST tag;
  std::string tag_bytes;
  rc = Parse_TPM_ST(
      &buffer,
      &tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  UINT32 response_size;
  std::string response_size_bytes;
  rc = Parse_UINT32(
      &buffer,
      &response_size,
      &response_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM_RC response_code;
  std::string response_code_bytes;
  rc = Parse_TPM_RC(
      &buffer,
      &response_code,
      &response_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (response_size != response.size()) {
    error_reporter.Run(TPM_RC_SIZE);
    return;
  }
  if (response_code != TPM_RC_SUCCESS) {
    error_reporter.Run(response_code);
    return;
  }
  TPM_CC command_code = TPM_CC_Rewrap;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string authorization_section_bytes;
  if (tag == TPM_ST_SESSIONS) {
    UINT32 parameter_section_size = buffer.size();
    rc = Parse_UINT32(&buffer, &parameter_section_size, NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    if (parameter_section_size > buffer.size()) {
      error_reporter.Run(TPM_RC_INSUFFICIENT);
      return;
    }
    authorization_section_bytes = buffer.substr(parameter_section_size);
    // Keep the parameter section in |buffer|.
    buffer.erase(parameter_section_size);
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(response_code_bytes.data(),
               response_code_bytes.size());
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(buffer.data(),
               buffer.size());
  std::string response_hash(32, 0);
  hash->Finish(string_as_array(&response_hash), response_hash.size());
  if (tag == TPM_ST_SESSIONS) {
    if (!authorization_delegate->CheckResponseAuthorization(
        response_hash,
        authorization_section_bytes)) {
      error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
      return;
    }
  }
  TPM2B_PRIVATE out_duplicate;
  std::string out_duplicate_bytes;
  rc = Parse_TPM2B_PRIVATE(
      &buffer,
      &out_duplicate,
      &out_duplicate_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (tag == TPM_ST_SESSIONS) {
    // Decrypt just the parameter data, not the size.
    std::string tmp = out_duplicate_bytes.substr(2);
    if (!authorization_delegate->DecryptResponseParameter(&tmp)) {
      error_reporter.Run(TRUNKS_RC_ENCRYPTION_FAILED);
      return;
    }
    out_duplicate_bytes.replace(2, std::string::npos, tmp);
    rc = Parse_TPM2B_PRIVATE(
        &out_duplicate_bytes,
        &out_duplicate,
        NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
  }
  callback.Run(response_code,
               out_duplicate);
}

void Tpm::Rewrap(
      TPMI_DH_OBJECT old_parent,
      const std::string& old_parent_name,
      TPMI_DH_OBJECT new_parent,
      const std::string& new_parent_name,
      TPM2B_PRIVATE in_duplicate,
      TPM2B_NAME name,
      AuthorizationDelegate* authorization_delegate,
      const RewrapResponse& callback) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(RewrapErrorCallback, callback);
  base::Callback<void(const std::string&)> parser =
      base::Bind(RewrapResponseParser,
                 callback,
                 authorization_delegate);
  TPMI_ST_COMMAND_TAG tag = TPM_ST_NO_SESSIONS;
  UINT32 command_size = 10;  // Header size.
  std::string handle_section_bytes;
  std::string parameter_section_bytes;
  TPM_CC command_code = TPM_CC_Rewrap;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string old_parent_bytes;
  rc = Serialize_TPMI_DH_OBJECT(
      old_parent,
      &old_parent_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string new_parent_bytes;
  rc = Serialize_TPMI_DH_OBJECT(
      new_parent,
      &new_parent_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string in_duplicate_bytes;
  rc = Serialize_TPM2B_PRIVATE(
      in_duplicate,
      &in_duplicate_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string name_bytes;
  rc = Serialize_TPM2B_NAME(
      name,
      &name_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  // Encrypt just the parameter data, not the size.
  std::string tmp = in_duplicate_bytes.substr(2);
  if (!authorization_delegate->EncryptCommandParameter(&tmp)) {
    error_reporter.Run(TRUNKS_RC_ENCRYPTION_FAILED);
    return;
  }
  in_duplicate_bytes.replace(2, std::string::npos, tmp);
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(old_parent_name.data(),
               old_parent_name.size());
  handle_section_bytes += old_parent_bytes;
  command_size += old_parent_bytes.size();
  hash->Update(new_parent_name.data(),
               new_parent_name.size());
  handle_section_bytes += new_parent_bytes;
  command_size += new_parent_bytes.size();
  hash->Update(in_duplicate_bytes.data(),
               in_duplicate_bytes.size());
  parameter_section_bytes += in_duplicate_bytes;
  command_size += in_duplicate_bytes.size();
  hash->Update(name_bytes.data(),
               name_bytes.size());
  parameter_section_bytes += name_bytes;
  command_size += name_bytes.size();
  std::string command_hash(32, 0);
  hash->Finish(string_as_array(&command_hash), command_hash.size());
  std::string authorization_section_bytes;
  if (!authorization_delegate->GetCommandAuthorization(
      command_hash,
      &authorization_section_bytes)) {
    error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
    return;
  }
  std::string authorization_size_bytes;
  if (!authorization_section_bytes.empty()) {
    tag = TPM_ST_SESSIONS;
    std::string tmp;
    rc = Serialize_UINT32(authorization_section_bytes.size(),
                          &authorization_size_bytes);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    command_size += authorization_size_bytes.size() +
                    authorization_section_bytes.size();
  }
  std::string tag_bytes;
  rc = Serialize_TPMI_ST_COMMAND_TAG(
      tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command_size_bytes;
  rc = Serialize_UINT32(
      command_size,
      &command_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command = tag_bytes +
                        command_size_bytes +
                        command_code_bytes +
                        handle_section_bytes +
                        authorization_size_bytes +
                        authorization_section_bytes +
                        parameter_section_bytes;
  CHECK(command.size() == command_size) << "Command size mismatch!";
  transceiver_->SendCommand(command, parser);
}

void ImportErrorCallback(
    const Tpm::ImportResponse& callback,
    TPM_RC response_code) {
  callback.Run(response_code,
               TPM2B_PRIVATE());
}

void ImportResponseParser(
    const Tpm::ImportResponse& callback,
    AuthorizationDelegate* authorization_delegate,
    const std::string& response) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(ImportErrorCallback, callback);
  std::string buffer(response);
  TPM_ST tag;
  std::string tag_bytes;
  rc = Parse_TPM_ST(
      &buffer,
      &tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  UINT32 response_size;
  std::string response_size_bytes;
  rc = Parse_UINT32(
      &buffer,
      &response_size,
      &response_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM_RC response_code;
  std::string response_code_bytes;
  rc = Parse_TPM_RC(
      &buffer,
      &response_code,
      &response_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (response_size != response.size()) {
    error_reporter.Run(TPM_RC_SIZE);
    return;
  }
  if (response_code != TPM_RC_SUCCESS) {
    error_reporter.Run(response_code);
    return;
  }
  TPM_CC command_code = TPM_CC_Import;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string authorization_section_bytes;
  if (tag == TPM_ST_SESSIONS) {
    UINT32 parameter_section_size = buffer.size();
    rc = Parse_UINT32(&buffer, &parameter_section_size, NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    if (parameter_section_size > buffer.size()) {
      error_reporter.Run(TPM_RC_INSUFFICIENT);
      return;
    }
    authorization_section_bytes = buffer.substr(parameter_section_size);
    // Keep the parameter section in |buffer|.
    buffer.erase(parameter_section_size);
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(response_code_bytes.data(),
               response_code_bytes.size());
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(buffer.data(),
               buffer.size());
  std::string response_hash(32, 0);
  hash->Finish(string_as_array(&response_hash), response_hash.size());
  if (tag == TPM_ST_SESSIONS) {
    if (!authorization_delegate->CheckResponseAuthorization(
        response_hash,
        authorization_section_bytes)) {
      error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
      return;
    }
  }
  TPM2B_PRIVATE out_private;
  std::string out_private_bytes;
  rc = Parse_TPM2B_PRIVATE(
      &buffer,
      &out_private,
      &out_private_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (tag == TPM_ST_SESSIONS) {
    // Decrypt just the parameter data, not the size.
    std::string tmp = out_private_bytes.substr(2);
    if (!authorization_delegate->DecryptResponseParameter(&tmp)) {
      error_reporter.Run(TRUNKS_RC_ENCRYPTION_FAILED);
      return;
    }
    out_private_bytes.replace(2, std::string::npos, tmp);
    rc = Parse_TPM2B_PRIVATE(
        &out_private_bytes,
        &out_private,
        NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
  }
  callback.Run(response_code,
               out_private);
}

void Tpm::Import(
      TPMI_DH_OBJECT parent_handle,
      const std::string& parent_handle_name,
      TPM2B_DATA encryption_key,
      TPM2B_PUBLIC object_public,
      TPM2B_PRIVATE duplicate,
      TPM2B_ENCRYPTED_SECRET in_sym_seed,
      TPMT_SYM_DEF_OBJECT symmetric_alg,
      AuthorizationDelegate* authorization_delegate,
      const ImportResponse& callback) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(ImportErrorCallback, callback);
  base::Callback<void(const std::string&)> parser =
      base::Bind(ImportResponseParser,
                 callback,
                 authorization_delegate);
  TPMI_ST_COMMAND_TAG tag = TPM_ST_NO_SESSIONS;
  UINT32 command_size = 10;  // Header size.
  std::string handle_section_bytes;
  std::string parameter_section_bytes;
  TPM_CC command_code = TPM_CC_Import;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string parent_handle_bytes;
  rc = Serialize_TPMI_DH_OBJECT(
      parent_handle,
      &parent_handle_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string encryption_key_bytes;
  rc = Serialize_TPM2B_DATA(
      encryption_key,
      &encryption_key_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string object_public_bytes;
  rc = Serialize_TPM2B_PUBLIC(
      object_public,
      &object_public_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string duplicate_bytes;
  rc = Serialize_TPM2B_PRIVATE(
      duplicate,
      &duplicate_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string in_sym_seed_bytes;
  rc = Serialize_TPM2B_ENCRYPTED_SECRET(
      in_sym_seed,
      &in_sym_seed_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string symmetric_alg_bytes;
  rc = Serialize_TPMT_SYM_DEF_OBJECT(
      symmetric_alg,
      &symmetric_alg_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  // Encrypt just the parameter data, not the size.
  std::string tmp = encryption_key_bytes.substr(2);
  if (!authorization_delegate->EncryptCommandParameter(&tmp)) {
    error_reporter.Run(TRUNKS_RC_ENCRYPTION_FAILED);
    return;
  }
  encryption_key_bytes.replace(2, std::string::npos, tmp);
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(parent_handle_name.data(),
               parent_handle_name.size());
  handle_section_bytes += parent_handle_bytes;
  command_size += parent_handle_bytes.size();
  hash->Update(encryption_key_bytes.data(),
               encryption_key_bytes.size());
  parameter_section_bytes += encryption_key_bytes;
  command_size += encryption_key_bytes.size();
  hash->Update(object_public_bytes.data(),
               object_public_bytes.size());
  parameter_section_bytes += object_public_bytes;
  command_size += object_public_bytes.size();
  hash->Update(duplicate_bytes.data(),
               duplicate_bytes.size());
  parameter_section_bytes += duplicate_bytes;
  command_size += duplicate_bytes.size();
  hash->Update(in_sym_seed_bytes.data(),
               in_sym_seed_bytes.size());
  parameter_section_bytes += in_sym_seed_bytes;
  command_size += in_sym_seed_bytes.size();
  hash->Update(symmetric_alg_bytes.data(),
               symmetric_alg_bytes.size());
  parameter_section_bytes += symmetric_alg_bytes;
  command_size += symmetric_alg_bytes.size();
  std::string command_hash(32, 0);
  hash->Finish(string_as_array(&command_hash), command_hash.size());
  std::string authorization_section_bytes;
  if (!authorization_delegate->GetCommandAuthorization(
      command_hash,
      &authorization_section_bytes)) {
    error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
    return;
  }
  std::string authorization_size_bytes;
  if (!authorization_section_bytes.empty()) {
    tag = TPM_ST_SESSIONS;
    std::string tmp;
    rc = Serialize_UINT32(authorization_section_bytes.size(),
                          &authorization_size_bytes);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    command_size += authorization_size_bytes.size() +
                    authorization_section_bytes.size();
  }
  std::string tag_bytes;
  rc = Serialize_TPMI_ST_COMMAND_TAG(
      tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command_size_bytes;
  rc = Serialize_UINT32(
      command_size,
      &command_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command = tag_bytes +
                        command_size_bytes +
                        command_code_bytes +
                        handle_section_bytes +
                        authorization_size_bytes +
                        authorization_section_bytes +
                        parameter_section_bytes;
  CHECK(command.size() == command_size) << "Command size mismatch!";
  transceiver_->SendCommand(command, parser);
}

void RSA_EncryptErrorCallback(
    const Tpm::RSA_EncryptResponse& callback,
    TPM_RC response_code) {
  callback.Run(response_code,
               TPM2B_PUBLIC_KEY_RSA());
}

void RSA_EncryptResponseParser(
    const Tpm::RSA_EncryptResponse& callback,
    AuthorizationDelegate* authorization_delegate,
    const std::string& response) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(RSA_EncryptErrorCallback, callback);
  std::string buffer(response);
  TPM_ST tag;
  std::string tag_bytes;
  rc = Parse_TPM_ST(
      &buffer,
      &tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  UINT32 response_size;
  std::string response_size_bytes;
  rc = Parse_UINT32(
      &buffer,
      &response_size,
      &response_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM_RC response_code;
  std::string response_code_bytes;
  rc = Parse_TPM_RC(
      &buffer,
      &response_code,
      &response_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (response_size != response.size()) {
    error_reporter.Run(TPM_RC_SIZE);
    return;
  }
  if (response_code != TPM_RC_SUCCESS) {
    error_reporter.Run(response_code);
    return;
  }
  TPM_CC command_code = TPM_CC_RSA_Encrypt;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string authorization_section_bytes;
  if (tag == TPM_ST_SESSIONS) {
    UINT32 parameter_section_size = buffer.size();
    rc = Parse_UINT32(&buffer, &parameter_section_size, NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    if (parameter_section_size > buffer.size()) {
      error_reporter.Run(TPM_RC_INSUFFICIENT);
      return;
    }
    authorization_section_bytes = buffer.substr(parameter_section_size);
    // Keep the parameter section in |buffer|.
    buffer.erase(parameter_section_size);
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(response_code_bytes.data(),
               response_code_bytes.size());
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(buffer.data(),
               buffer.size());
  std::string response_hash(32, 0);
  hash->Finish(string_as_array(&response_hash), response_hash.size());
  if (tag == TPM_ST_SESSIONS) {
    if (!authorization_delegate->CheckResponseAuthorization(
        response_hash,
        authorization_section_bytes)) {
      error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
      return;
    }
  }
  TPM2B_PUBLIC_KEY_RSA out_data;
  std::string out_data_bytes;
  rc = Parse_TPM2B_PUBLIC_KEY_RSA(
      &buffer,
      &out_data,
      &out_data_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (tag == TPM_ST_SESSIONS) {
    // Decrypt just the parameter data, not the size.
    std::string tmp = out_data_bytes.substr(2);
    if (!authorization_delegate->DecryptResponseParameter(&tmp)) {
      error_reporter.Run(TRUNKS_RC_ENCRYPTION_FAILED);
      return;
    }
    out_data_bytes.replace(2, std::string::npos, tmp);
    rc = Parse_TPM2B_PUBLIC_KEY_RSA(
        &out_data_bytes,
        &out_data,
        NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
  }
  callback.Run(response_code,
               out_data);
}

void Tpm::RSA_Encrypt(
      TPMI_DH_OBJECT key_handle,
      const std::string& key_handle_name,
      TPM2B_PUBLIC_KEY_RSA message,
      TPMT_RSA_DECRYPT in_scheme,
      TPM2B_DATA label,
      AuthorizationDelegate* authorization_delegate,
      const RSA_EncryptResponse& callback) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(RSA_EncryptErrorCallback, callback);
  base::Callback<void(const std::string&)> parser =
      base::Bind(RSA_EncryptResponseParser,
                 callback,
                 authorization_delegate);
  TPMI_ST_COMMAND_TAG tag = TPM_ST_NO_SESSIONS;
  UINT32 command_size = 10;  // Header size.
  std::string handle_section_bytes;
  std::string parameter_section_bytes;
  TPM_CC command_code = TPM_CC_RSA_Encrypt;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string key_handle_bytes;
  rc = Serialize_TPMI_DH_OBJECT(
      key_handle,
      &key_handle_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string message_bytes;
  rc = Serialize_TPM2B_PUBLIC_KEY_RSA(
      message,
      &message_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string in_scheme_bytes;
  rc = Serialize_TPMT_RSA_DECRYPT(
      in_scheme,
      &in_scheme_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string label_bytes;
  rc = Serialize_TPM2B_DATA(
      label,
      &label_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  // Encrypt just the parameter data, not the size.
  std::string tmp = message_bytes.substr(2);
  if (!authorization_delegate->EncryptCommandParameter(&tmp)) {
    error_reporter.Run(TRUNKS_RC_ENCRYPTION_FAILED);
    return;
  }
  message_bytes.replace(2, std::string::npos, tmp);
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(key_handle_name.data(),
               key_handle_name.size());
  handle_section_bytes += key_handle_bytes;
  command_size += key_handle_bytes.size();
  hash->Update(message_bytes.data(),
               message_bytes.size());
  parameter_section_bytes += message_bytes;
  command_size += message_bytes.size();
  hash->Update(in_scheme_bytes.data(),
               in_scheme_bytes.size());
  parameter_section_bytes += in_scheme_bytes;
  command_size += in_scheme_bytes.size();
  hash->Update(label_bytes.data(),
               label_bytes.size());
  parameter_section_bytes += label_bytes;
  command_size += label_bytes.size();
  std::string command_hash(32, 0);
  hash->Finish(string_as_array(&command_hash), command_hash.size());
  std::string authorization_section_bytes;
  if (!authorization_delegate->GetCommandAuthorization(
      command_hash,
      &authorization_section_bytes)) {
    error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
    return;
  }
  std::string authorization_size_bytes;
  if (!authorization_section_bytes.empty()) {
    tag = TPM_ST_SESSIONS;
    std::string tmp;
    rc = Serialize_UINT32(authorization_section_bytes.size(),
                          &authorization_size_bytes);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    command_size += authorization_size_bytes.size() +
                    authorization_section_bytes.size();
  }
  std::string tag_bytes;
  rc = Serialize_TPMI_ST_COMMAND_TAG(
      tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command_size_bytes;
  rc = Serialize_UINT32(
      command_size,
      &command_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command = tag_bytes +
                        command_size_bytes +
                        command_code_bytes +
                        handle_section_bytes +
                        authorization_size_bytes +
                        authorization_section_bytes +
                        parameter_section_bytes;
  CHECK(command.size() == command_size) << "Command size mismatch!";
  transceiver_->SendCommand(command, parser);
}

void RSA_DecryptErrorCallback(
    const Tpm::RSA_DecryptResponse& callback,
    TPM_RC response_code) {
  callback.Run(response_code,
               TPM2B_PUBLIC_KEY_RSA());
}

void RSA_DecryptResponseParser(
    const Tpm::RSA_DecryptResponse& callback,
    AuthorizationDelegate* authorization_delegate,
    const std::string& response) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(RSA_DecryptErrorCallback, callback);
  std::string buffer(response);
  TPM_ST tag;
  std::string tag_bytes;
  rc = Parse_TPM_ST(
      &buffer,
      &tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  UINT32 response_size;
  std::string response_size_bytes;
  rc = Parse_UINT32(
      &buffer,
      &response_size,
      &response_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM_RC response_code;
  std::string response_code_bytes;
  rc = Parse_TPM_RC(
      &buffer,
      &response_code,
      &response_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (response_size != response.size()) {
    error_reporter.Run(TPM_RC_SIZE);
    return;
  }
  if (response_code != TPM_RC_SUCCESS) {
    error_reporter.Run(response_code);
    return;
  }
  TPM_CC command_code = TPM_CC_RSA_Decrypt;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string authorization_section_bytes;
  if (tag == TPM_ST_SESSIONS) {
    UINT32 parameter_section_size = buffer.size();
    rc = Parse_UINT32(&buffer, &parameter_section_size, NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    if (parameter_section_size > buffer.size()) {
      error_reporter.Run(TPM_RC_INSUFFICIENT);
      return;
    }
    authorization_section_bytes = buffer.substr(parameter_section_size);
    // Keep the parameter section in |buffer|.
    buffer.erase(parameter_section_size);
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(response_code_bytes.data(),
               response_code_bytes.size());
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(buffer.data(),
               buffer.size());
  std::string response_hash(32, 0);
  hash->Finish(string_as_array(&response_hash), response_hash.size());
  if (tag == TPM_ST_SESSIONS) {
    if (!authorization_delegate->CheckResponseAuthorization(
        response_hash,
        authorization_section_bytes)) {
      error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
      return;
    }
  }
  TPM2B_PUBLIC_KEY_RSA message;
  std::string message_bytes;
  rc = Parse_TPM2B_PUBLIC_KEY_RSA(
      &buffer,
      &message,
      &message_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (tag == TPM_ST_SESSIONS) {
    // Decrypt just the parameter data, not the size.
    std::string tmp = message_bytes.substr(2);
    if (!authorization_delegate->DecryptResponseParameter(&tmp)) {
      error_reporter.Run(TRUNKS_RC_ENCRYPTION_FAILED);
      return;
    }
    message_bytes.replace(2, std::string::npos, tmp);
    rc = Parse_TPM2B_PUBLIC_KEY_RSA(
        &message_bytes,
        &message,
        NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
  }
  callback.Run(response_code,
               message);
}

void Tpm::RSA_Decrypt(
      TPMI_DH_OBJECT key_handle,
      const std::string& key_handle_name,
      TPM2B_PUBLIC_KEY_RSA cipher_text,
      TPMT_RSA_DECRYPT in_scheme,
      TPM2B_DATA label,
      AuthorizationDelegate* authorization_delegate,
      const RSA_DecryptResponse& callback) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(RSA_DecryptErrorCallback, callback);
  base::Callback<void(const std::string&)> parser =
      base::Bind(RSA_DecryptResponseParser,
                 callback,
                 authorization_delegate);
  TPMI_ST_COMMAND_TAG tag = TPM_ST_NO_SESSIONS;
  UINT32 command_size = 10;  // Header size.
  std::string handle_section_bytes;
  std::string parameter_section_bytes;
  TPM_CC command_code = TPM_CC_RSA_Decrypt;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string key_handle_bytes;
  rc = Serialize_TPMI_DH_OBJECT(
      key_handle,
      &key_handle_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string cipher_text_bytes;
  rc = Serialize_TPM2B_PUBLIC_KEY_RSA(
      cipher_text,
      &cipher_text_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string in_scheme_bytes;
  rc = Serialize_TPMT_RSA_DECRYPT(
      in_scheme,
      &in_scheme_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string label_bytes;
  rc = Serialize_TPM2B_DATA(
      label,
      &label_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  // Encrypt just the parameter data, not the size.
  std::string tmp = cipher_text_bytes.substr(2);
  if (!authorization_delegate->EncryptCommandParameter(&tmp)) {
    error_reporter.Run(TRUNKS_RC_ENCRYPTION_FAILED);
    return;
  }
  cipher_text_bytes.replace(2, std::string::npos, tmp);
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(key_handle_name.data(),
               key_handle_name.size());
  handle_section_bytes += key_handle_bytes;
  command_size += key_handle_bytes.size();
  hash->Update(cipher_text_bytes.data(),
               cipher_text_bytes.size());
  parameter_section_bytes += cipher_text_bytes;
  command_size += cipher_text_bytes.size();
  hash->Update(in_scheme_bytes.data(),
               in_scheme_bytes.size());
  parameter_section_bytes += in_scheme_bytes;
  command_size += in_scheme_bytes.size();
  hash->Update(label_bytes.data(),
               label_bytes.size());
  parameter_section_bytes += label_bytes;
  command_size += label_bytes.size();
  std::string command_hash(32, 0);
  hash->Finish(string_as_array(&command_hash), command_hash.size());
  std::string authorization_section_bytes;
  if (!authorization_delegate->GetCommandAuthorization(
      command_hash,
      &authorization_section_bytes)) {
    error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
    return;
  }
  std::string authorization_size_bytes;
  if (!authorization_section_bytes.empty()) {
    tag = TPM_ST_SESSIONS;
    std::string tmp;
    rc = Serialize_UINT32(authorization_section_bytes.size(),
                          &authorization_size_bytes);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    command_size += authorization_size_bytes.size() +
                    authorization_section_bytes.size();
  }
  std::string tag_bytes;
  rc = Serialize_TPMI_ST_COMMAND_TAG(
      tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command_size_bytes;
  rc = Serialize_UINT32(
      command_size,
      &command_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command = tag_bytes +
                        command_size_bytes +
                        command_code_bytes +
                        handle_section_bytes +
                        authorization_size_bytes +
                        authorization_section_bytes +
                        parameter_section_bytes;
  CHECK(command.size() == command_size) << "Command size mismatch!";
  transceiver_->SendCommand(command, parser);
}

void ECDH_KeyGenErrorCallback(
    const Tpm::ECDH_KeyGenResponse& callback,
    TPM_RC response_code) {
  callback.Run(response_code,
               TPM2B_ECC_POINT(),
               TPM2B_ECC_POINT());
}

void ECDH_KeyGenResponseParser(
    const Tpm::ECDH_KeyGenResponse& callback,
    AuthorizationDelegate* authorization_delegate,
    const std::string& response) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(ECDH_KeyGenErrorCallback, callback);
  std::string buffer(response);
  TPM_ST tag;
  std::string tag_bytes;
  rc = Parse_TPM_ST(
      &buffer,
      &tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  UINT32 response_size;
  std::string response_size_bytes;
  rc = Parse_UINT32(
      &buffer,
      &response_size,
      &response_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM_RC response_code;
  std::string response_code_bytes;
  rc = Parse_TPM_RC(
      &buffer,
      &response_code,
      &response_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (response_size != response.size()) {
    error_reporter.Run(TPM_RC_SIZE);
    return;
  }
  if (response_code != TPM_RC_SUCCESS) {
    error_reporter.Run(response_code);
    return;
  }
  TPM_CC command_code = TPM_CC_ECDH_KeyGen;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string authorization_section_bytes;
  if (tag == TPM_ST_SESSIONS) {
    UINT32 parameter_section_size = buffer.size();
    rc = Parse_UINT32(&buffer, &parameter_section_size, NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    if (parameter_section_size > buffer.size()) {
      error_reporter.Run(TPM_RC_INSUFFICIENT);
      return;
    }
    authorization_section_bytes = buffer.substr(parameter_section_size);
    // Keep the parameter section in |buffer|.
    buffer.erase(parameter_section_size);
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(response_code_bytes.data(),
               response_code_bytes.size());
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(buffer.data(),
               buffer.size());
  std::string response_hash(32, 0);
  hash->Finish(string_as_array(&response_hash), response_hash.size());
  if (tag == TPM_ST_SESSIONS) {
    if (!authorization_delegate->CheckResponseAuthorization(
        response_hash,
        authorization_section_bytes)) {
      error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
      return;
    }
  }
  TPM2B_ECC_POINT z_point;
  std::string z_point_bytes;
  rc = Parse_TPM2B_ECC_POINT(
      &buffer,
      &z_point,
      &z_point_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM2B_ECC_POINT pub_point;
  std::string pub_point_bytes;
  rc = Parse_TPM2B_ECC_POINT(
      &buffer,
      &pub_point,
      &pub_point_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (tag == TPM_ST_SESSIONS) {
    // Decrypt just the parameter data, not the size.
    std::string tmp = z_point_bytes.substr(2);
    if (!authorization_delegate->DecryptResponseParameter(&tmp)) {
      error_reporter.Run(TRUNKS_RC_ENCRYPTION_FAILED);
      return;
    }
    z_point_bytes.replace(2, std::string::npos, tmp);
    rc = Parse_TPM2B_ECC_POINT(
        &z_point_bytes,
        &z_point,
        NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
  }
  callback.Run(response_code,
               z_point,
               pub_point);
}

void Tpm::ECDH_KeyGen(
      TPMI_DH_OBJECT key_handle,
      const std::string& key_handle_name,
      AuthorizationDelegate* authorization_delegate,
      const ECDH_KeyGenResponse& callback) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(ECDH_KeyGenErrorCallback, callback);
  base::Callback<void(const std::string&)> parser =
      base::Bind(ECDH_KeyGenResponseParser,
                 callback,
                 authorization_delegate);
  TPMI_ST_COMMAND_TAG tag = TPM_ST_NO_SESSIONS;
  UINT32 command_size = 10;  // Header size.
  std::string handle_section_bytes;
  std::string parameter_section_bytes;
  TPM_CC command_code = TPM_CC_ECDH_KeyGen;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string key_handle_bytes;
  rc = Serialize_TPMI_DH_OBJECT(
      key_handle,
      &key_handle_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(key_handle_name.data(),
               key_handle_name.size());
  handle_section_bytes += key_handle_bytes;
  command_size += key_handle_bytes.size();
  std::string command_hash(32, 0);
  hash->Finish(string_as_array(&command_hash), command_hash.size());
  std::string authorization_section_bytes;
  if (!authorization_delegate->GetCommandAuthorization(
      command_hash,
      &authorization_section_bytes)) {
    error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
    return;
  }
  std::string authorization_size_bytes;
  if (!authorization_section_bytes.empty()) {
    tag = TPM_ST_SESSIONS;
    std::string tmp;
    rc = Serialize_UINT32(authorization_section_bytes.size(),
                          &authorization_size_bytes);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    command_size += authorization_size_bytes.size() +
                    authorization_section_bytes.size();
  }
  std::string tag_bytes;
  rc = Serialize_TPMI_ST_COMMAND_TAG(
      tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command_size_bytes;
  rc = Serialize_UINT32(
      command_size,
      &command_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command = tag_bytes +
                        command_size_bytes +
                        command_code_bytes +
                        handle_section_bytes +
                        authorization_size_bytes +
                        authorization_section_bytes +
                        parameter_section_bytes;
  CHECK(command.size() == command_size) << "Command size mismatch!";
  transceiver_->SendCommand(command, parser);
}

void ECDH_ZGenErrorCallback(
    const Tpm::ECDH_ZGenResponse& callback,
    TPM_RC response_code) {
  callback.Run(response_code,
               TPM2B_ECC_POINT());
}

void ECDH_ZGenResponseParser(
    const Tpm::ECDH_ZGenResponse& callback,
    AuthorizationDelegate* authorization_delegate,
    const std::string& response) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(ECDH_ZGenErrorCallback, callback);
  std::string buffer(response);
  TPM_ST tag;
  std::string tag_bytes;
  rc = Parse_TPM_ST(
      &buffer,
      &tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  UINT32 response_size;
  std::string response_size_bytes;
  rc = Parse_UINT32(
      &buffer,
      &response_size,
      &response_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM_RC response_code;
  std::string response_code_bytes;
  rc = Parse_TPM_RC(
      &buffer,
      &response_code,
      &response_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (response_size != response.size()) {
    error_reporter.Run(TPM_RC_SIZE);
    return;
  }
  if (response_code != TPM_RC_SUCCESS) {
    error_reporter.Run(response_code);
    return;
  }
  TPM_CC command_code = TPM_CC_ECDH_ZGen;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string authorization_section_bytes;
  if (tag == TPM_ST_SESSIONS) {
    UINT32 parameter_section_size = buffer.size();
    rc = Parse_UINT32(&buffer, &parameter_section_size, NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    if (parameter_section_size > buffer.size()) {
      error_reporter.Run(TPM_RC_INSUFFICIENT);
      return;
    }
    authorization_section_bytes = buffer.substr(parameter_section_size);
    // Keep the parameter section in |buffer|.
    buffer.erase(parameter_section_size);
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(response_code_bytes.data(),
               response_code_bytes.size());
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(buffer.data(),
               buffer.size());
  std::string response_hash(32, 0);
  hash->Finish(string_as_array(&response_hash), response_hash.size());
  if (tag == TPM_ST_SESSIONS) {
    if (!authorization_delegate->CheckResponseAuthorization(
        response_hash,
        authorization_section_bytes)) {
      error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
      return;
    }
  }
  TPM2B_ECC_POINT out_point;
  std::string out_point_bytes;
  rc = Parse_TPM2B_ECC_POINT(
      &buffer,
      &out_point,
      &out_point_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (tag == TPM_ST_SESSIONS) {
    // Decrypt just the parameter data, not the size.
    std::string tmp = out_point_bytes.substr(2);
    if (!authorization_delegate->DecryptResponseParameter(&tmp)) {
      error_reporter.Run(TRUNKS_RC_ENCRYPTION_FAILED);
      return;
    }
    out_point_bytes.replace(2, std::string::npos, tmp);
    rc = Parse_TPM2B_ECC_POINT(
        &out_point_bytes,
        &out_point,
        NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
  }
  callback.Run(response_code,
               out_point);
}

void Tpm::ECDH_ZGen(
      TPMI_DH_OBJECT key_handle,
      const std::string& key_handle_name,
      TPM2B_ECC_POINT in_point,
      AuthorizationDelegate* authorization_delegate,
      const ECDH_ZGenResponse& callback) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(ECDH_ZGenErrorCallback, callback);
  base::Callback<void(const std::string&)> parser =
      base::Bind(ECDH_ZGenResponseParser,
                 callback,
                 authorization_delegate);
  TPMI_ST_COMMAND_TAG tag = TPM_ST_NO_SESSIONS;
  UINT32 command_size = 10;  // Header size.
  std::string handle_section_bytes;
  std::string parameter_section_bytes;
  TPM_CC command_code = TPM_CC_ECDH_ZGen;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string key_handle_bytes;
  rc = Serialize_TPMI_DH_OBJECT(
      key_handle,
      &key_handle_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string in_point_bytes;
  rc = Serialize_TPM2B_ECC_POINT(
      in_point,
      &in_point_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  // Encrypt just the parameter data, not the size.
  std::string tmp = in_point_bytes.substr(2);
  if (!authorization_delegate->EncryptCommandParameter(&tmp)) {
    error_reporter.Run(TRUNKS_RC_ENCRYPTION_FAILED);
    return;
  }
  in_point_bytes.replace(2, std::string::npos, tmp);
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(key_handle_name.data(),
               key_handle_name.size());
  handle_section_bytes += key_handle_bytes;
  command_size += key_handle_bytes.size();
  hash->Update(in_point_bytes.data(),
               in_point_bytes.size());
  parameter_section_bytes += in_point_bytes;
  command_size += in_point_bytes.size();
  std::string command_hash(32, 0);
  hash->Finish(string_as_array(&command_hash), command_hash.size());
  std::string authorization_section_bytes;
  if (!authorization_delegate->GetCommandAuthorization(
      command_hash,
      &authorization_section_bytes)) {
    error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
    return;
  }
  std::string authorization_size_bytes;
  if (!authorization_section_bytes.empty()) {
    tag = TPM_ST_SESSIONS;
    std::string tmp;
    rc = Serialize_UINT32(authorization_section_bytes.size(),
                          &authorization_size_bytes);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    command_size += authorization_size_bytes.size() +
                    authorization_section_bytes.size();
  }
  std::string tag_bytes;
  rc = Serialize_TPMI_ST_COMMAND_TAG(
      tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command_size_bytes;
  rc = Serialize_UINT32(
      command_size,
      &command_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command = tag_bytes +
                        command_size_bytes +
                        command_code_bytes +
                        handle_section_bytes +
                        authorization_size_bytes +
                        authorization_section_bytes +
                        parameter_section_bytes;
  CHECK(command.size() == command_size) << "Command size mismatch!";
  transceiver_->SendCommand(command, parser);
}

void ECC_ParametersErrorCallback(
    const Tpm::ECC_ParametersResponse& callback,
    TPM_RC response_code) {
  callback.Run(response_code,
               TPMS_ALGORITHM_DETAIL_ECC());
}

void ECC_ParametersResponseParser(
    const Tpm::ECC_ParametersResponse& callback,
    AuthorizationDelegate* authorization_delegate,
    const std::string& response) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(ECC_ParametersErrorCallback, callback);
  std::string buffer(response);
  TPM_ST tag;
  std::string tag_bytes;
  rc = Parse_TPM_ST(
      &buffer,
      &tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  UINT32 response_size;
  std::string response_size_bytes;
  rc = Parse_UINT32(
      &buffer,
      &response_size,
      &response_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM_RC response_code;
  std::string response_code_bytes;
  rc = Parse_TPM_RC(
      &buffer,
      &response_code,
      &response_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (response_size != response.size()) {
    error_reporter.Run(TPM_RC_SIZE);
    return;
  }
  if (response_code != TPM_RC_SUCCESS) {
    error_reporter.Run(response_code);
    return;
  }
  TPM_CC command_code = TPM_CC_ECC_Parameters;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string authorization_section_bytes;
  if (tag == TPM_ST_SESSIONS) {
    UINT32 parameter_section_size = buffer.size();
    rc = Parse_UINT32(&buffer, &parameter_section_size, NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    if (parameter_section_size > buffer.size()) {
      error_reporter.Run(TPM_RC_INSUFFICIENT);
      return;
    }
    authorization_section_bytes = buffer.substr(parameter_section_size);
    // Keep the parameter section in |buffer|.
    buffer.erase(parameter_section_size);
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(response_code_bytes.data(),
               response_code_bytes.size());
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(buffer.data(),
               buffer.size());
  std::string response_hash(32, 0);
  hash->Finish(string_as_array(&response_hash), response_hash.size());
  if (tag == TPM_ST_SESSIONS) {
    if (!authorization_delegate->CheckResponseAuthorization(
        response_hash,
        authorization_section_bytes)) {
      error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
      return;
    }
  }
  TPMS_ALGORITHM_DETAIL_ECC parameters;
  std::string parameters_bytes;
  rc = Parse_TPMS_ALGORITHM_DETAIL_ECC(
      &buffer,
      &parameters,
      &parameters_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (tag == TPM_ST_SESSIONS) {
    // Decrypt just the parameter data, not the size.
    std::string tmp = parameters_bytes.substr(2);
    if (!authorization_delegate->DecryptResponseParameter(&tmp)) {
      error_reporter.Run(TRUNKS_RC_ENCRYPTION_FAILED);
      return;
    }
    parameters_bytes.replace(2, std::string::npos, tmp);
    rc = Parse_TPMS_ALGORITHM_DETAIL_ECC(
        &parameters_bytes,
        &parameters,
        NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
  }
  callback.Run(response_code,
               parameters);
}

void Tpm::ECC_Parameters(
      TPMI_ECC_CURVE curve_id,
      AuthorizationDelegate* authorization_delegate,
      const ECC_ParametersResponse& callback) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(ECC_ParametersErrorCallback, callback);
  base::Callback<void(const std::string&)> parser =
      base::Bind(ECC_ParametersResponseParser,
                 callback,
                 authorization_delegate);
  TPMI_ST_COMMAND_TAG tag = TPM_ST_NO_SESSIONS;
  UINT32 command_size = 10;  // Header size.
  std::string handle_section_bytes;
  std::string parameter_section_bytes;
  TPM_CC command_code = TPM_CC_ECC_Parameters;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string curve_id_bytes;
  rc = Serialize_TPMI_ECC_CURVE(
      curve_id,
      &curve_id_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(curve_id_bytes.data(),
               curve_id_bytes.size());
  parameter_section_bytes += curve_id_bytes;
  command_size += curve_id_bytes.size();
  std::string command_hash(32, 0);
  hash->Finish(string_as_array(&command_hash), command_hash.size());
  std::string authorization_section_bytes;
  if (!authorization_delegate->GetCommandAuthorization(
      command_hash,
      &authorization_section_bytes)) {
    error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
    return;
  }
  std::string authorization_size_bytes;
  if (!authorization_section_bytes.empty()) {
    tag = TPM_ST_SESSIONS;
    std::string tmp;
    rc = Serialize_UINT32(authorization_section_bytes.size(),
                          &authorization_size_bytes);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    command_size += authorization_size_bytes.size() +
                    authorization_section_bytes.size();
  }
  std::string tag_bytes;
  rc = Serialize_TPMI_ST_COMMAND_TAG(
      tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command_size_bytes;
  rc = Serialize_UINT32(
      command_size,
      &command_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command = tag_bytes +
                        command_size_bytes +
                        command_code_bytes +
                        handle_section_bytes +
                        authorization_size_bytes +
                        authorization_section_bytes +
                        parameter_section_bytes;
  CHECK(command.size() == command_size) << "Command size mismatch!";
  transceiver_->SendCommand(command, parser);
}

void ZGen_2PhaseErrorCallback(
    const Tpm::ZGen_2PhaseResponse& callback,
    TPM_RC response_code) {
  callback.Run(response_code,
               TPM2B_ECC_POINT(),
               TPM2B_ECC_POINT());
}

void ZGen_2PhaseResponseParser(
    const Tpm::ZGen_2PhaseResponse& callback,
    AuthorizationDelegate* authorization_delegate,
    const std::string& response) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(ZGen_2PhaseErrorCallback, callback);
  std::string buffer(response);
  TPM_ST tag;
  std::string tag_bytes;
  rc = Parse_TPM_ST(
      &buffer,
      &tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  UINT32 response_size;
  std::string response_size_bytes;
  rc = Parse_UINT32(
      &buffer,
      &response_size,
      &response_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM_RC response_code;
  std::string response_code_bytes;
  rc = Parse_TPM_RC(
      &buffer,
      &response_code,
      &response_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (response_size != response.size()) {
    error_reporter.Run(TPM_RC_SIZE);
    return;
  }
  if (response_code != TPM_RC_SUCCESS) {
    error_reporter.Run(response_code);
    return;
  }
  TPM_CC command_code = TPM_CC_ZGen_2Phase;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string authorization_section_bytes;
  if (tag == TPM_ST_SESSIONS) {
    UINT32 parameter_section_size = buffer.size();
    rc = Parse_UINT32(&buffer, &parameter_section_size, NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    if (parameter_section_size > buffer.size()) {
      error_reporter.Run(TPM_RC_INSUFFICIENT);
      return;
    }
    authorization_section_bytes = buffer.substr(parameter_section_size);
    // Keep the parameter section in |buffer|.
    buffer.erase(parameter_section_size);
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(response_code_bytes.data(),
               response_code_bytes.size());
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(buffer.data(),
               buffer.size());
  std::string response_hash(32, 0);
  hash->Finish(string_as_array(&response_hash), response_hash.size());
  if (tag == TPM_ST_SESSIONS) {
    if (!authorization_delegate->CheckResponseAuthorization(
        response_hash,
        authorization_section_bytes)) {
      error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
      return;
    }
  }
  TPM2B_ECC_POINT out_z1;
  std::string out_z1_bytes;
  rc = Parse_TPM2B_ECC_POINT(
      &buffer,
      &out_z1,
      &out_z1_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM2B_ECC_POINT out_z2;
  std::string out_z2_bytes;
  rc = Parse_TPM2B_ECC_POINT(
      &buffer,
      &out_z2,
      &out_z2_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (tag == TPM_ST_SESSIONS) {
    // Decrypt just the parameter data, not the size.
    std::string tmp = out_z1_bytes.substr(2);
    if (!authorization_delegate->DecryptResponseParameter(&tmp)) {
      error_reporter.Run(TRUNKS_RC_ENCRYPTION_FAILED);
      return;
    }
    out_z1_bytes.replace(2, std::string::npos, tmp);
    rc = Parse_TPM2B_ECC_POINT(
        &out_z1_bytes,
        &out_z1,
        NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
  }
  callback.Run(response_code,
               out_z1,
               out_z2);
}

void Tpm::ZGen_2Phase(
      TPMI_DH_OBJECT key_a,
      const std::string& key_a_name,
      TPM2B_ECC_POINT in_qs_b,
      TPM2B_ECC_POINT in_qe_b,
      TPMI_ECC_KEY_EXCHANGE in_scheme,
      UINT16 counter,
      AuthorizationDelegate* authorization_delegate,
      const ZGen_2PhaseResponse& callback) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(ZGen_2PhaseErrorCallback, callback);
  base::Callback<void(const std::string&)> parser =
      base::Bind(ZGen_2PhaseResponseParser,
                 callback,
                 authorization_delegate);
  TPMI_ST_COMMAND_TAG tag = TPM_ST_NO_SESSIONS;
  UINT32 command_size = 10;  // Header size.
  std::string handle_section_bytes;
  std::string parameter_section_bytes;
  TPM_CC command_code = TPM_CC_ZGen_2Phase;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string key_a_bytes;
  rc = Serialize_TPMI_DH_OBJECT(
      key_a,
      &key_a_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string in_qs_b_bytes;
  rc = Serialize_TPM2B_ECC_POINT(
      in_qs_b,
      &in_qs_b_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string in_qe_b_bytes;
  rc = Serialize_TPM2B_ECC_POINT(
      in_qe_b,
      &in_qe_b_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string in_scheme_bytes;
  rc = Serialize_TPMI_ECC_KEY_EXCHANGE(
      in_scheme,
      &in_scheme_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string counter_bytes;
  rc = Serialize_UINT16(
      counter,
      &counter_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  // Encrypt just the parameter data, not the size.
  std::string tmp = in_qs_b_bytes.substr(2);
  if (!authorization_delegate->EncryptCommandParameter(&tmp)) {
    error_reporter.Run(TRUNKS_RC_ENCRYPTION_FAILED);
    return;
  }
  in_qs_b_bytes.replace(2, std::string::npos, tmp);
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(key_a_name.data(),
               key_a_name.size());
  handle_section_bytes += key_a_bytes;
  command_size += key_a_bytes.size();
  hash->Update(in_qs_b_bytes.data(),
               in_qs_b_bytes.size());
  parameter_section_bytes += in_qs_b_bytes;
  command_size += in_qs_b_bytes.size();
  hash->Update(in_qe_b_bytes.data(),
               in_qe_b_bytes.size());
  parameter_section_bytes += in_qe_b_bytes;
  command_size += in_qe_b_bytes.size();
  hash->Update(in_scheme_bytes.data(),
               in_scheme_bytes.size());
  parameter_section_bytes += in_scheme_bytes;
  command_size += in_scheme_bytes.size();
  hash->Update(counter_bytes.data(),
               counter_bytes.size());
  parameter_section_bytes += counter_bytes;
  command_size += counter_bytes.size();
  std::string command_hash(32, 0);
  hash->Finish(string_as_array(&command_hash), command_hash.size());
  std::string authorization_section_bytes;
  if (!authorization_delegate->GetCommandAuthorization(
      command_hash,
      &authorization_section_bytes)) {
    error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
    return;
  }
  std::string authorization_size_bytes;
  if (!authorization_section_bytes.empty()) {
    tag = TPM_ST_SESSIONS;
    std::string tmp;
    rc = Serialize_UINT32(authorization_section_bytes.size(),
                          &authorization_size_bytes);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    command_size += authorization_size_bytes.size() +
                    authorization_section_bytes.size();
  }
  std::string tag_bytes;
  rc = Serialize_TPMI_ST_COMMAND_TAG(
      tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command_size_bytes;
  rc = Serialize_UINT32(
      command_size,
      &command_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command = tag_bytes +
                        command_size_bytes +
                        command_code_bytes +
                        handle_section_bytes +
                        authorization_size_bytes +
                        authorization_section_bytes +
                        parameter_section_bytes;
  CHECK(command.size() == command_size) << "Command size mismatch!";
  transceiver_->SendCommand(command, parser);
}

void EncryptDecryptErrorCallback(
    const Tpm::EncryptDecryptResponse& callback,
    TPM_RC response_code) {
  callback.Run(response_code,
               TPM2B_MAX_BUFFER(),
               TPM2B_IV());
}

void EncryptDecryptResponseParser(
    const Tpm::EncryptDecryptResponse& callback,
    AuthorizationDelegate* authorization_delegate,
    const std::string& response) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(EncryptDecryptErrorCallback, callback);
  std::string buffer(response);
  TPM_ST tag;
  std::string tag_bytes;
  rc = Parse_TPM_ST(
      &buffer,
      &tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  UINT32 response_size;
  std::string response_size_bytes;
  rc = Parse_UINT32(
      &buffer,
      &response_size,
      &response_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM_RC response_code;
  std::string response_code_bytes;
  rc = Parse_TPM_RC(
      &buffer,
      &response_code,
      &response_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (response_size != response.size()) {
    error_reporter.Run(TPM_RC_SIZE);
    return;
  }
  if (response_code != TPM_RC_SUCCESS) {
    error_reporter.Run(response_code);
    return;
  }
  TPM_CC command_code = TPM_CC_EncryptDecrypt;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string authorization_section_bytes;
  if (tag == TPM_ST_SESSIONS) {
    UINT32 parameter_section_size = buffer.size();
    rc = Parse_UINT32(&buffer, &parameter_section_size, NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    if (parameter_section_size > buffer.size()) {
      error_reporter.Run(TPM_RC_INSUFFICIENT);
      return;
    }
    authorization_section_bytes = buffer.substr(parameter_section_size);
    // Keep the parameter section in |buffer|.
    buffer.erase(parameter_section_size);
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(response_code_bytes.data(),
               response_code_bytes.size());
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(buffer.data(),
               buffer.size());
  std::string response_hash(32, 0);
  hash->Finish(string_as_array(&response_hash), response_hash.size());
  if (tag == TPM_ST_SESSIONS) {
    if (!authorization_delegate->CheckResponseAuthorization(
        response_hash,
        authorization_section_bytes)) {
      error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
      return;
    }
  }
  TPM2B_MAX_BUFFER out_data;
  std::string out_data_bytes;
  rc = Parse_TPM2B_MAX_BUFFER(
      &buffer,
      &out_data,
      &out_data_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM2B_IV iv_out;
  std::string iv_out_bytes;
  rc = Parse_TPM2B_IV(
      &buffer,
      &iv_out,
      &iv_out_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (tag == TPM_ST_SESSIONS) {
    // Decrypt just the parameter data, not the size.
    std::string tmp = out_data_bytes.substr(2);
    if (!authorization_delegate->DecryptResponseParameter(&tmp)) {
      error_reporter.Run(TRUNKS_RC_ENCRYPTION_FAILED);
      return;
    }
    out_data_bytes.replace(2, std::string::npos, tmp);
    rc = Parse_TPM2B_MAX_BUFFER(
        &out_data_bytes,
        &out_data,
        NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
  }
  callback.Run(response_code,
               out_data,
               iv_out);
}

void Tpm::EncryptDecrypt(
      TPMI_DH_OBJECT key_handle,
      const std::string& key_handle_name,
      TPMI_YES_NO decrypt,
      TPMI_ALG_SYM_MODE mode,
      TPM2B_IV iv_in,
      TPM2B_MAX_BUFFER in_data,
      AuthorizationDelegate* authorization_delegate,
      const EncryptDecryptResponse& callback) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(EncryptDecryptErrorCallback, callback);
  base::Callback<void(const std::string&)> parser =
      base::Bind(EncryptDecryptResponseParser,
                 callback,
                 authorization_delegate);
  TPMI_ST_COMMAND_TAG tag = TPM_ST_NO_SESSIONS;
  UINT32 command_size = 10;  // Header size.
  std::string handle_section_bytes;
  std::string parameter_section_bytes;
  TPM_CC command_code = TPM_CC_EncryptDecrypt;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string key_handle_bytes;
  rc = Serialize_TPMI_DH_OBJECT(
      key_handle,
      &key_handle_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string decrypt_bytes;
  rc = Serialize_TPMI_YES_NO(
      decrypt,
      &decrypt_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string mode_bytes;
  rc = Serialize_TPMI_ALG_SYM_MODE(
      mode,
      &mode_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string iv_in_bytes;
  rc = Serialize_TPM2B_IV(
      iv_in,
      &iv_in_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string in_data_bytes;
  rc = Serialize_TPM2B_MAX_BUFFER(
      in_data,
      &in_data_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(key_handle_name.data(),
               key_handle_name.size());
  handle_section_bytes += key_handle_bytes;
  command_size += key_handle_bytes.size();
  hash->Update(decrypt_bytes.data(),
               decrypt_bytes.size());
  parameter_section_bytes += decrypt_bytes;
  command_size += decrypt_bytes.size();
  hash->Update(mode_bytes.data(),
               mode_bytes.size());
  parameter_section_bytes += mode_bytes;
  command_size += mode_bytes.size();
  hash->Update(iv_in_bytes.data(),
               iv_in_bytes.size());
  parameter_section_bytes += iv_in_bytes;
  command_size += iv_in_bytes.size();
  hash->Update(in_data_bytes.data(),
               in_data_bytes.size());
  parameter_section_bytes += in_data_bytes;
  command_size += in_data_bytes.size();
  std::string command_hash(32, 0);
  hash->Finish(string_as_array(&command_hash), command_hash.size());
  std::string authorization_section_bytes;
  if (!authorization_delegate->GetCommandAuthorization(
      command_hash,
      &authorization_section_bytes)) {
    error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
    return;
  }
  std::string authorization_size_bytes;
  if (!authorization_section_bytes.empty()) {
    tag = TPM_ST_SESSIONS;
    std::string tmp;
    rc = Serialize_UINT32(authorization_section_bytes.size(),
                          &authorization_size_bytes);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    command_size += authorization_size_bytes.size() +
                    authorization_section_bytes.size();
  }
  std::string tag_bytes;
  rc = Serialize_TPMI_ST_COMMAND_TAG(
      tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command_size_bytes;
  rc = Serialize_UINT32(
      command_size,
      &command_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command = tag_bytes +
                        command_size_bytes +
                        command_code_bytes +
                        handle_section_bytes +
                        authorization_size_bytes +
                        authorization_section_bytes +
                        parameter_section_bytes;
  CHECK(command.size() == command_size) << "Command size mismatch!";
  transceiver_->SendCommand(command, parser);
}

void HashErrorCallback(
    const Tpm::HashResponse& callback,
    TPM_RC response_code) {
  callback.Run(response_code,
               TPM2B_DIGEST(),
               TPMT_TK_HASHCHECK());
}

void HashResponseParser(
    const Tpm::HashResponse& callback,
    AuthorizationDelegate* authorization_delegate,
    const std::string& response) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(HashErrorCallback, callback);
  std::string buffer(response);
  TPM_ST tag;
  std::string tag_bytes;
  rc = Parse_TPM_ST(
      &buffer,
      &tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  UINT32 response_size;
  std::string response_size_bytes;
  rc = Parse_UINT32(
      &buffer,
      &response_size,
      &response_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM_RC response_code;
  std::string response_code_bytes;
  rc = Parse_TPM_RC(
      &buffer,
      &response_code,
      &response_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (response_size != response.size()) {
    error_reporter.Run(TPM_RC_SIZE);
    return;
  }
  if (response_code != TPM_RC_SUCCESS) {
    error_reporter.Run(response_code);
    return;
  }
  TPM_CC command_code = TPM_CC_Hash;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string authorization_section_bytes;
  if (tag == TPM_ST_SESSIONS) {
    UINT32 parameter_section_size = buffer.size();
    rc = Parse_UINT32(&buffer, &parameter_section_size, NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    if (parameter_section_size > buffer.size()) {
      error_reporter.Run(TPM_RC_INSUFFICIENT);
      return;
    }
    authorization_section_bytes = buffer.substr(parameter_section_size);
    // Keep the parameter section in |buffer|.
    buffer.erase(parameter_section_size);
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(response_code_bytes.data(),
               response_code_bytes.size());
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(buffer.data(),
               buffer.size());
  std::string response_hash(32, 0);
  hash->Finish(string_as_array(&response_hash), response_hash.size());
  if (tag == TPM_ST_SESSIONS) {
    if (!authorization_delegate->CheckResponseAuthorization(
        response_hash,
        authorization_section_bytes)) {
      error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
      return;
    }
  }
  TPM2B_DIGEST out_hash;
  std::string out_hash_bytes;
  rc = Parse_TPM2B_DIGEST(
      &buffer,
      &out_hash,
      &out_hash_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPMT_TK_HASHCHECK validation;
  std::string validation_bytes;
  rc = Parse_TPMT_TK_HASHCHECK(
      &buffer,
      &validation,
      &validation_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (tag == TPM_ST_SESSIONS) {
    // Decrypt just the parameter data, not the size.
    std::string tmp = out_hash_bytes.substr(2);
    if (!authorization_delegate->DecryptResponseParameter(&tmp)) {
      error_reporter.Run(TRUNKS_RC_ENCRYPTION_FAILED);
      return;
    }
    out_hash_bytes.replace(2, std::string::npos, tmp);
    rc = Parse_TPM2B_DIGEST(
        &out_hash_bytes,
        &out_hash,
        NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
  }
  callback.Run(response_code,
               out_hash,
               validation);
}

void Tpm::Hash(
      TPMI_RH_HIERARCHY hierarchy,
      const std::string& hierarchy_name,
      TPM2B_MAX_BUFFER data,
      TPMI_ALG_HASH hash_alg,
      AuthorizationDelegate* authorization_delegate,
      const HashResponse& callback) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(HashErrorCallback, callback);
  base::Callback<void(const std::string&)> parser =
      base::Bind(HashResponseParser,
                 callback,
                 authorization_delegate);
  TPMI_ST_COMMAND_TAG tag = TPM_ST_NO_SESSIONS;
  UINT32 command_size = 10;  // Header size.
  std::string handle_section_bytes;
  std::string parameter_section_bytes;
  TPM_CC command_code = TPM_CC_Hash;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string data_bytes;
  rc = Serialize_TPM2B_MAX_BUFFER(
      data,
      &data_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string hash_alg_bytes;
  rc = Serialize_TPMI_ALG_HASH(
      hash_alg,
      &hash_alg_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string hierarchy_bytes;
  rc = Serialize_TPMI_RH_HIERARCHY(
      hierarchy,
      &hierarchy_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  // Encrypt just the parameter data, not the size.
  std::string tmp = data_bytes.substr(2);
  if (!authorization_delegate->EncryptCommandParameter(&tmp)) {
    error_reporter.Run(TRUNKS_RC_ENCRYPTION_FAILED);
    return;
  }
  data_bytes.replace(2, std::string::npos, tmp);
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(hierarchy_name.data(),
               hierarchy_name.size());
  handle_section_bytes += hierarchy_bytes;
  command_size += hierarchy_bytes.size();
  hash->Update(data_bytes.data(),
               data_bytes.size());
  parameter_section_bytes += data_bytes;
  command_size += data_bytes.size();
  hash->Update(hash_alg_bytes.data(),
               hash_alg_bytes.size());
  parameter_section_bytes += hash_alg_bytes;
  command_size += hash_alg_bytes.size();
  std::string command_hash(32, 0);
  hash->Finish(string_as_array(&command_hash), command_hash.size());
  std::string authorization_section_bytes;
  if (!authorization_delegate->GetCommandAuthorization(
      command_hash,
      &authorization_section_bytes)) {
    error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
    return;
  }
  std::string authorization_size_bytes;
  if (!authorization_section_bytes.empty()) {
    tag = TPM_ST_SESSIONS;
    std::string tmp;
    rc = Serialize_UINT32(authorization_section_bytes.size(),
                          &authorization_size_bytes);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    command_size += authorization_size_bytes.size() +
                    authorization_section_bytes.size();
  }
  std::string tag_bytes;
  rc = Serialize_TPMI_ST_COMMAND_TAG(
      tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command_size_bytes;
  rc = Serialize_UINT32(
      command_size,
      &command_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command = tag_bytes +
                        command_size_bytes +
                        command_code_bytes +
                        handle_section_bytes +
                        authorization_size_bytes +
                        authorization_section_bytes +
                        parameter_section_bytes;
  CHECK(command.size() == command_size) << "Command size mismatch!";
  transceiver_->SendCommand(command, parser);
}

void HMACErrorCallback(
    const Tpm::HMACResponse& callback,
    TPM_RC response_code) {
  callback.Run(response_code,
               TPM2B_DIGEST());
}

void HMACResponseParser(
    const Tpm::HMACResponse& callback,
    AuthorizationDelegate* authorization_delegate,
    const std::string& response) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(HMACErrorCallback, callback);
  std::string buffer(response);
  TPM_ST tag;
  std::string tag_bytes;
  rc = Parse_TPM_ST(
      &buffer,
      &tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  UINT32 response_size;
  std::string response_size_bytes;
  rc = Parse_UINT32(
      &buffer,
      &response_size,
      &response_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM_RC response_code;
  std::string response_code_bytes;
  rc = Parse_TPM_RC(
      &buffer,
      &response_code,
      &response_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (response_size != response.size()) {
    error_reporter.Run(TPM_RC_SIZE);
    return;
  }
  if (response_code != TPM_RC_SUCCESS) {
    error_reporter.Run(response_code);
    return;
  }
  TPM_CC command_code = TPM_CC_HMAC;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string authorization_section_bytes;
  if (tag == TPM_ST_SESSIONS) {
    UINT32 parameter_section_size = buffer.size();
    rc = Parse_UINT32(&buffer, &parameter_section_size, NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    if (parameter_section_size > buffer.size()) {
      error_reporter.Run(TPM_RC_INSUFFICIENT);
      return;
    }
    authorization_section_bytes = buffer.substr(parameter_section_size);
    // Keep the parameter section in |buffer|.
    buffer.erase(parameter_section_size);
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(response_code_bytes.data(),
               response_code_bytes.size());
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(buffer.data(),
               buffer.size());
  std::string response_hash(32, 0);
  hash->Finish(string_as_array(&response_hash), response_hash.size());
  if (tag == TPM_ST_SESSIONS) {
    if (!authorization_delegate->CheckResponseAuthorization(
        response_hash,
        authorization_section_bytes)) {
      error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
      return;
    }
  }
  TPM2B_DIGEST out_hmac;
  std::string out_hmac_bytes;
  rc = Parse_TPM2B_DIGEST(
      &buffer,
      &out_hmac,
      &out_hmac_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (tag == TPM_ST_SESSIONS) {
    // Decrypt just the parameter data, not the size.
    std::string tmp = out_hmac_bytes.substr(2);
    if (!authorization_delegate->DecryptResponseParameter(&tmp)) {
      error_reporter.Run(TRUNKS_RC_ENCRYPTION_FAILED);
      return;
    }
    out_hmac_bytes.replace(2, std::string::npos, tmp);
    rc = Parse_TPM2B_DIGEST(
        &out_hmac_bytes,
        &out_hmac,
        NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
  }
  callback.Run(response_code,
               out_hmac);
}

void Tpm::HMAC(
      TPMI_DH_OBJECT handle,
      const std::string& handle_name,
      TPM2B_MAX_BUFFER buffer,
      TPMI_ALG_HASH hash_alg,
      AuthorizationDelegate* authorization_delegate,
      const HMACResponse& callback) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(HMACErrorCallback, callback);
  base::Callback<void(const std::string&)> parser =
      base::Bind(HMACResponseParser,
                 callback,
                 authorization_delegate);
  TPMI_ST_COMMAND_TAG tag = TPM_ST_NO_SESSIONS;
  UINT32 command_size = 10;  // Header size.
  std::string handle_section_bytes;
  std::string parameter_section_bytes;
  TPM_CC command_code = TPM_CC_HMAC;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string handle_bytes;
  rc = Serialize_TPMI_DH_OBJECT(
      handle,
      &handle_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string buffer_bytes;
  rc = Serialize_TPM2B_MAX_BUFFER(
      buffer,
      &buffer_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string hash_alg_bytes;
  rc = Serialize_TPMI_ALG_HASH(
      hash_alg,
      &hash_alg_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  // Encrypt just the parameter data, not the size.
  std::string tmp = buffer_bytes.substr(2);
  if (!authorization_delegate->EncryptCommandParameter(&tmp)) {
    error_reporter.Run(TRUNKS_RC_ENCRYPTION_FAILED);
    return;
  }
  buffer_bytes.replace(2, std::string::npos, tmp);
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(handle_name.data(),
               handle_name.size());
  handle_section_bytes += handle_bytes;
  command_size += handle_bytes.size();
  hash->Update(buffer_bytes.data(),
               buffer_bytes.size());
  parameter_section_bytes += buffer_bytes;
  command_size += buffer_bytes.size();
  hash->Update(hash_alg_bytes.data(),
               hash_alg_bytes.size());
  parameter_section_bytes += hash_alg_bytes;
  command_size += hash_alg_bytes.size();
  std::string command_hash(32, 0);
  hash->Finish(string_as_array(&command_hash), command_hash.size());
  std::string authorization_section_bytes;
  if (!authorization_delegate->GetCommandAuthorization(
      command_hash,
      &authorization_section_bytes)) {
    error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
    return;
  }
  std::string authorization_size_bytes;
  if (!authorization_section_bytes.empty()) {
    tag = TPM_ST_SESSIONS;
    std::string tmp;
    rc = Serialize_UINT32(authorization_section_bytes.size(),
                          &authorization_size_bytes);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    command_size += authorization_size_bytes.size() +
                    authorization_section_bytes.size();
  }
  std::string tag_bytes;
  rc = Serialize_TPMI_ST_COMMAND_TAG(
      tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command_size_bytes;
  rc = Serialize_UINT32(
      command_size,
      &command_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command = tag_bytes +
                        command_size_bytes +
                        command_code_bytes +
                        handle_section_bytes +
                        authorization_size_bytes +
                        authorization_section_bytes +
                        parameter_section_bytes;
  CHECK(command.size() == command_size) << "Command size mismatch!";
  transceiver_->SendCommand(command, parser);
}

void GetRandomErrorCallback(
    const Tpm::GetRandomResponse& callback,
    TPM_RC response_code) {
  callback.Run(response_code,
               TPM2B_DIGEST());
}

void GetRandomResponseParser(
    const Tpm::GetRandomResponse& callback,
    AuthorizationDelegate* authorization_delegate,
    const std::string& response) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(GetRandomErrorCallback, callback);
  std::string buffer(response);
  TPM_ST tag;
  std::string tag_bytes;
  rc = Parse_TPM_ST(
      &buffer,
      &tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  UINT32 response_size;
  std::string response_size_bytes;
  rc = Parse_UINT32(
      &buffer,
      &response_size,
      &response_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM_RC response_code;
  std::string response_code_bytes;
  rc = Parse_TPM_RC(
      &buffer,
      &response_code,
      &response_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (response_size != response.size()) {
    error_reporter.Run(TPM_RC_SIZE);
    return;
  }
  if (response_code != TPM_RC_SUCCESS) {
    error_reporter.Run(response_code);
    return;
  }
  TPM_CC command_code = TPM_CC_GetRandom;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string authorization_section_bytes;
  if (tag == TPM_ST_SESSIONS) {
    UINT32 parameter_section_size = buffer.size();
    rc = Parse_UINT32(&buffer, &parameter_section_size, NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    if (parameter_section_size > buffer.size()) {
      error_reporter.Run(TPM_RC_INSUFFICIENT);
      return;
    }
    authorization_section_bytes = buffer.substr(parameter_section_size);
    // Keep the parameter section in |buffer|.
    buffer.erase(parameter_section_size);
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(response_code_bytes.data(),
               response_code_bytes.size());
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(buffer.data(),
               buffer.size());
  std::string response_hash(32, 0);
  hash->Finish(string_as_array(&response_hash), response_hash.size());
  if (tag == TPM_ST_SESSIONS) {
    if (!authorization_delegate->CheckResponseAuthorization(
        response_hash,
        authorization_section_bytes)) {
      error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
      return;
    }
  }
  TPM2B_DIGEST random_bytes;
  std::string random_bytes_bytes;
  rc = Parse_TPM2B_DIGEST(
      &buffer,
      &random_bytes,
      &random_bytes_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (tag == TPM_ST_SESSIONS) {
    // Decrypt just the parameter data, not the size.
    std::string tmp = random_bytes_bytes.substr(2);
    if (!authorization_delegate->DecryptResponseParameter(&tmp)) {
      error_reporter.Run(TRUNKS_RC_ENCRYPTION_FAILED);
      return;
    }
    random_bytes_bytes.replace(2, std::string::npos, tmp);
    rc = Parse_TPM2B_DIGEST(
        &random_bytes_bytes,
        &random_bytes,
        NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
  }
  callback.Run(response_code,
               random_bytes);
}

void Tpm::GetRandom(
      UINT16 bytes_requested,
      AuthorizationDelegate* authorization_delegate,
      const GetRandomResponse& callback) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(GetRandomErrorCallback, callback);
  base::Callback<void(const std::string&)> parser =
      base::Bind(GetRandomResponseParser,
                 callback,
                 authorization_delegate);
  TPMI_ST_COMMAND_TAG tag = TPM_ST_NO_SESSIONS;
  UINT32 command_size = 10;  // Header size.
  std::string handle_section_bytes;
  std::string parameter_section_bytes;
  TPM_CC command_code = TPM_CC_GetRandom;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string bytes_requested_bytes;
  rc = Serialize_UINT16(
      bytes_requested,
      &bytes_requested_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(bytes_requested_bytes.data(),
               bytes_requested_bytes.size());
  parameter_section_bytes += bytes_requested_bytes;
  command_size += bytes_requested_bytes.size();
  std::string command_hash(32, 0);
  hash->Finish(string_as_array(&command_hash), command_hash.size());
  std::string authorization_section_bytes;
  if (!authorization_delegate->GetCommandAuthorization(
      command_hash,
      &authorization_section_bytes)) {
    error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
    return;
  }
  std::string authorization_size_bytes;
  if (!authorization_section_bytes.empty()) {
    tag = TPM_ST_SESSIONS;
    std::string tmp;
    rc = Serialize_UINT32(authorization_section_bytes.size(),
                          &authorization_size_bytes);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    command_size += authorization_size_bytes.size() +
                    authorization_section_bytes.size();
  }
  std::string tag_bytes;
  rc = Serialize_TPMI_ST_COMMAND_TAG(
      tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command_size_bytes;
  rc = Serialize_UINT32(
      command_size,
      &command_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command = tag_bytes +
                        command_size_bytes +
                        command_code_bytes +
                        handle_section_bytes +
                        authorization_size_bytes +
                        authorization_section_bytes +
                        parameter_section_bytes;
  CHECK(command.size() == command_size) << "Command size mismatch!";
  transceiver_->SendCommand(command, parser);
}

void StirRandomErrorCallback(
    const Tpm::StirRandomResponse& callback,
    TPM_RC response_code) {
  callback.Run(response_code);
}

void StirRandomResponseParser(
    const Tpm::StirRandomResponse& callback,
    AuthorizationDelegate* authorization_delegate,
    const std::string& response) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(StirRandomErrorCallback, callback);
  std::string buffer(response);
  TPM_ST tag;
  std::string tag_bytes;
  rc = Parse_TPM_ST(
      &buffer,
      &tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  UINT32 response_size;
  std::string response_size_bytes;
  rc = Parse_UINT32(
      &buffer,
      &response_size,
      &response_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM_RC response_code;
  std::string response_code_bytes;
  rc = Parse_TPM_RC(
      &buffer,
      &response_code,
      &response_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (response_size != response.size()) {
    error_reporter.Run(TPM_RC_SIZE);
    return;
  }
  if (response_code != TPM_RC_SUCCESS) {
    error_reporter.Run(response_code);
    return;
  }
  TPM_CC command_code = TPM_CC_StirRandom;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string authorization_section_bytes;
  if (tag == TPM_ST_SESSIONS) {
    UINT32 parameter_section_size = buffer.size();
    rc = Parse_UINT32(&buffer, &parameter_section_size, NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    if (parameter_section_size > buffer.size()) {
      error_reporter.Run(TPM_RC_INSUFFICIENT);
      return;
    }
    authorization_section_bytes = buffer.substr(parameter_section_size);
    // Keep the parameter section in |buffer|.
    buffer.erase(parameter_section_size);
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(response_code_bytes.data(),
               response_code_bytes.size());
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(buffer.data(),
               buffer.size());
  std::string response_hash(32, 0);
  hash->Finish(string_as_array(&response_hash), response_hash.size());
  if (tag == TPM_ST_SESSIONS) {
    if (!authorization_delegate->CheckResponseAuthorization(
        response_hash,
        authorization_section_bytes)) {
      error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
      return;
    }
  }
  callback.Run(response_code);
}

void Tpm::StirRandom(
      TPM2B_SENSITIVE_DATA in_data,
      AuthorizationDelegate* authorization_delegate,
      const StirRandomResponse& callback) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(StirRandomErrorCallback, callback);
  base::Callback<void(const std::string&)> parser =
      base::Bind(StirRandomResponseParser,
                 callback,
                 authorization_delegate);
  TPMI_ST_COMMAND_TAG tag = TPM_ST_NO_SESSIONS;
  UINT32 command_size = 10;  // Header size.
  std::string handle_section_bytes;
  std::string parameter_section_bytes;
  TPM_CC command_code = TPM_CC_StirRandom;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string in_data_bytes;
  rc = Serialize_TPM2B_SENSITIVE_DATA(
      in_data,
      &in_data_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  // Encrypt just the parameter data, not the size.
  std::string tmp = in_data_bytes.substr(2);
  if (!authorization_delegate->EncryptCommandParameter(&tmp)) {
    error_reporter.Run(TRUNKS_RC_ENCRYPTION_FAILED);
    return;
  }
  in_data_bytes.replace(2, std::string::npos, tmp);
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(in_data_bytes.data(),
               in_data_bytes.size());
  parameter_section_bytes += in_data_bytes;
  command_size += in_data_bytes.size();
  std::string command_hash(32, 0);
  hash->Finish(string_as_array(&command_hash), command_hash.size());
  std::string authorization_section_bytes;
  if (!authorization_delegate->GetCommandAuthorization(
      command_hash,
      &authorization_section_bytes)) {
    error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
    return;
  }
  std::string authorization_size_bytes;
  if (!authorization_section_bytes.empty()) {
    tag = TPM_ST_SESSIONS;
    std::string tmp;
    rc = Serialize_UINT32(authorization_section_bytes.size(),
                          &authorization_size_bytes);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    command_size += authorization_size_bytes.size() +
                    authorization_section_bytes.size();
  }
  std::string tag_bytes;
  rc = Serialize_TPMI_ST_COMMAND_TAG(
      tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command_size_bytes;
  rc = Serialize_UINT32(
      command_size,
      &command_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command = tag_bytes +
                        command_size_bytes +
                        command_code_bytes +
                        handle_section_bytes +
                        authorization_size_bytes +
                        authorization_section_bytes +
                        parameter_section_bytes;
  CHECK(command.size() == command_size) << "Command size mismatch!";
  transceiver_->SendCommand(command, parser);
}

void HMAC_StartErrorCallback(
    const Tpm::HMAC_StartResponse& callback,
    TPM_RC response_code) {
  callback.Run(response_code,
               TPMI_DH_OBJECT());
}

void HMAC_StartResponseParser(
    const Tpm::HMAC_StartResponse& callback,
    AuthorizationDelegate* authorization_delegate,
    const std::string& response) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(HMAC_StartErrorCallback, callback);
  std::string buffer(response);
  TPM_ST tag;
  std::string tag_bytes;
  rc = Parse_TPM_ST(
      &buffer,
      &tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  UINT32 response_size;
  std::string response_size_bytes;
  rc = Parse_UINT32(
      &buffer,
      &response_size,
      &response_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM_RC response_code;
  std::string response_code_bytes;
  rc = Parse_TPM_RC(
      &buffer,
      &response_code,
      &response_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (response_size != response.size()) {
    error_reporter.Run(TPM_RC_SIZE);
    return;
  }
  if (response_code != TPM_RC_SUCCESS) {
    error_reporter.Run(response_code);
    return;
  }
  TPM_CC command_code = TPM_CC_HMAC_Start;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string authorization_section_bytes;
  if (tag == TPM_ST_SESSIONS) {
    UINT32 parameter_section_size = buffer.size();
    rc = Parse_UINT32(&buffer, &parameter_section_size, NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    if (parameter_section_size > buffer.size()) {
      error_reporter.Run(TPM_RC_INSUFFICIENT);
      return;
    }
    authorization_section_bytes = buffer.substr(parameter_section_size);
    // Keep the parameter section in |buffer|.
    buffer.erase(parameter_section_size);
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(response_code_bytes.data(),
               response_code_bytes.size());
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(buffer.data(),
               buffer.size());
  std::string response_hash(32, 0);
  hash->Finish(string_as_array(&response_hash), response_hash.size());
  if (tag == TPM_ST_SESSIONS) {
    if (!authorization_delegate->CheckResponseAuthorization(
        response_hash,
        authorization_section_bytes)) {
      error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
      return;
    }
  }
  TPMI_DH_OBJECT sequence_handle;
  std::string sequence_handle_bytes;
  rc = Parse_TPMI_DH_OBJECT(
      &buffer,
      &sequence_handle,
      &sequence_handle_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (tag == TPM_ST_SESSIONS) {
    // Decrypt just the parameter data, not the size.
    std::string tmp = sequence_handle_bytes.substr(2);
    if (!authorization_delegate->DecryptResponseParameter(&tmp)) {
      error_reporter.Run(TRUNKS_RC_ENCRYPTION_FAILED);
      return;
    }
    sequence_handle_bytes.replace(2, std::string::npos, tmp);
    rc = Parse_TPMI_DH_OBJECT(
        &sequence_handle_bytes,
        &sequence_handle,
        NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
  }
  callback.Run(response_code,
               sequence_handle);
}

void Tpm::HMAC_Start(
      TPMI_DH_OBJECT handle,
      const std::string& handle_name,
      TPM2B_AUTH auth,
      TPMI_ALG_HASH hash_alg,
      AuthorizationDelegate* authorization_delegate,
      const HMAC_StartResponse& callback) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(HMAC_StartErrorCallback, callback);
  base::Callback<void(const std::string&)> parser =
      base::Bind(HMAC_StartResponseParser,
                 callback,
                 authorization_delegate);
  TPMI_ST_COMMAND_TAG tag = TPM_ST_NO_SESSIONS;
  UINT32 command_size = 10;  // Header size.
  std::string handle_section_bytes;
  std::string parameter_section_bytes;
  TPM_CC command_code = TPM_CC_HMAC_Start;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string handle_bytes;
  rc = Serialize_TPMI_DH_OBJECT(
      handle,
      &handle_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string auth_bytes;
  rc = Serialize_TPM2B_AUTH(
      auth,
      &auth_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string hash_alg_bytes;
  rc = Serialize_TPMI_ALG_HASH(
      hash_alg,
      &hash_alg_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  // Encrypt just the parameter data, not the size.
  std::string tmp = auth_bytes.substr(2);
  if (!authorization_delegate->EncryptCommandParameter(&tmp)) {
    error_reporter.Run(TRUNKS_RC_ENCRYPTION_FAILED);
    return;
  }
  auth_bytes.replace(2, std::string::npos, tmp);
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(handle_name.data(),
               handle_name.size());
  handle_section_bytes += handle_bytes;
  command_size += handle_bytes.size();
  hash->Update(auth_bytes.data(),
               auth_bytes.size());
  parameter_section_bytes += auth_bytes;
  command_size += auth_bytes.size();
  hash->Update(hash_alg_bytes.data(),
               hash_alg_bytes.size());
  parameter_section_bytes += hash_alg_bytes;
  command_size += hash_alg_bytes.size();
  std::string command_hash(32, 0);
  hash->Finish(string_as_array(&command_hash), command_hash.size());
  std::string authorization_section_bytes;
  if (!authorization_delegate->GetCommandAuthorization(
      command_hash,
      &authorization_section_bytes)) {
    error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
    return;
  }
  std::string authorization_size_bytes;
  if (!authorization_section_bytes.empty()) {
    tag = TPM_ST_SESSIONS;
    std::string tmp;
    rc = Serialize_UINT32(authorization_section_bytes.size(),
                          &authorization_size_bytes);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    command_size += authorization_size_bytes.size() +
                    authorization_section_bytes.size();
  }
  std::string tag_bytes;
  rc = Serialize_TPMI_ST_COMMAND_TAG(
      tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command_size_bytes;
  rc = Serialize_UINT32(
      command_size,
      &command_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command = tag_bytes +
                        command_size_bytes +
                        command_code_bytes +
                        handle_section_bytes +
                        authorization_size_bytes +
                        authorization_section_bytes +
                        parameter_section_bytes;
  CHECK(command.size() == command_size) << "Command size mismatch!";
  transceiver_->SendCommand(command, parser);
}

void HashSequenceStartErrorCallback(
    const Tpm::HashSequenceStartResponse& callback,
    TPM_RC response_code) {
  callback.Run(response_code,
               TPMI_DH_OBJECT());
}

void HashSequenceStartResponseParser(
    const Tpm::HashSequenceStartResponse& callback,
    AuthorizationDelegate* authorization_delegate,
    const std::string& response) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(HashSequenceStartErrorCallback, callback);
  std::string buffer(response);
  TPM_ST tag;
  std::string tag_bytes;
  rc = Parse_TPM_ST(
      &buffer,
      &tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  UINT32 response_size;
  std::string response_size_bytes;
  rc = Parse_UINT32(
      &buffer,
      &response_size,
      &response_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM_RC response_code;
  std::string response_code_bytes;
  rc = Parse_TPM_RC(
      &buffer,
      &response_code,
      &response_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (response_size != response.size()) {
    error_reporter.Run(TPM_RC_SIZE);
    return;
  }
  if (response_code != TPM_RC_SUCCESS) {
    error_reporter.Run(response_code);
    return;
  }
  TPM_CC command_code = TPM_CC_HashSequenceStart;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string authorization_section_bytes;
  if (tag == TPM_ST_SESSIONS) {
    UINT32 parameter_section_size = buffer.size();
    rc = Parse_UINT32(&buffer, &parameter_section_size, NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    if (parameter_section_size > buffer.size()) {
      error_reporter.Run(TPM_RC_INSUFFICIENT);
      return;
    }
    authorization_section_bytes = buffer.substr(parameter_section_size);
    // Keep the parameter section in |buffer|.
    buffer.erase(parameter_section_size);
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(response_code_bytes.data(),
               response_code_bytes.size());
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(buffer.data(),
               buffer.size());
  std::string response_hash(32, 0);
  hash->Finish(string_as_array(&response_hash), response_hash.size());
  if (tag == TPM_ST_SESSIONS) {
    if (!authorization_delegate->CheckResponseAuthorization(
        response_hash,
        authorization_section_bytes)) {
      error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
      return;
    }
  }
  TPMI_DH_OBJECT sequence_handle;
  std::string sequence_handle_bytes;
  rc = Parse_TPMI_DH_OBJECT(
      &buffer,
      &sequence_handle,
      &sequence_handle_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (tag == TPM_ST_SESSIONS) {
    // Decrypt just the parameter data, not the size.
    std::string tmp = sequence_handle_bytes.substr(2);
    if (!authorization_delegate->DecryptResponseParameter(&tmp)) {
      error_reporter.Run(TRUNKS_RC_ENCRYPTION_FAILED);
      return;
    }
    sequence_handle_bytes.replace(2, std::string::npos, tmp);
    rc = Parse_TPMI_DH_OBJECT(
        &sequence_handle_bytes,
        &sequence_handle,
        NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
  }
  callback.Run(response_code,
               sequence_handle);
}

void Tpm::HashSequenceStart(
      TPM2B_AUTH auth,
      TPMI_ALG_HASH hash_alg,
      AuthorizationDelegate* authorization_delegate,
      const HashSequenceStartResponse& callback) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(HashSequenceStartErrorCallback, callback);
  base::Callback<void(const std::string&)> parser =
      base::Bind(HashSequenceStartResponseParser,
                 callback,
                 authorization_delegate);
  TPMI_ST_COMMAND_TAG tag = TPM_ST_NO_SESSIONS;
  UINT32 command_size = 10;  // Header size.
  std::string handle_section_bytes;
  std::string parameter_section_bytes;
  TPM_CC command_code = TPM_CC_HashSequenceStart;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string auth_bytes;
  rc = Serialize_TPM2B_AUTH(
      auth,
      &auth_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string hash_alg_bytes;
  rc = Serialize_TPMI_ALG_HASH(
      hash_alg,
      &hash_alg_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  // Encrypt just the parameter data, not the size.
  std::string tmp = auth_bytes.substr(2);
  if (!authorization_delegate->EncryptCommandParameter(&tmp)) {
    error_reporter.Run(TRUNKS_RC_ENCRYPTION_FAILED);
    return;
  }
  auth_bytes.replace(2, std::string::npos, tmp);
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(auth_bytes.data(),
               auth_bytes.size());
  parameter_section_bytes += auth_bytes;
  command_size += auth_bytes.size();
  hash->Update(hash_alg_bytes.data(),
               hash_alg_bytes.size());
  parameter_section_bytes += hash_alg_bytes;
  command_size += hash_alg_bytes.size();
  std::string command_hash(32, 0);
  hash->Finish(string_as_array(&command_hash), command_hash.size());
  std::string authorization_section_bytes;
  if (!authorization_delegate->GetCommandAuthorization(
      command_hash,
      &authorization_section_bytes)) {
    error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
    return;
  }
  std::string authorization_size_bytes;
  if (!authorization_section_bytes.empty()) {
    tag = TPM_ST_SESSIONS;
    std::string tmp;
    rc = Serialize_UINT32(authorization_section_bytes.size(),
                          &authorization_size_bytes);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    command_size += authorization_size_bytes.size() +
                    authorization_section_bytes.size();
  }
  std::string tag_bytes;
  rc = Serialize_TPMI_ST_COMMAND_TAG(
      tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command_size_bytes;
  rc = Serialize_UINT32(
      command_size,
      &command_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command = tag_bytes +
                        command_size_bytes +
                        command_code_bytes +
                        handle_section_bytes +
                        authorization_size_bytes +
                        authorization_section_bytes +
                        parameter_section_bytes;
  CHECK(command.size() == command_size) << "Command size mismatch!";
  transceiver_->SendCommand(command, parser);
}

void SequenceUpdateErrorCallback(
    const Tpm::SequenceUpdateResponse& callback,
    TPM_RC response_code) {
  callback.Run(response_code);
}

void SequenceUpdateResponseParser(
    const Tpm::SequenceUpdateResponse& callback,
    AuthorizationDelegate* authorization_delegate,
    const std::string& response) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(SequenceUpdateErrorCallback, callback);
  std::string buffer(response);
  TPM_ST tag;
  std::string tag_bytes;
  rc = Parse_TPM_ST(
      &buffer,
      &tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  UINT32 response_size;
  std::string response_size_bytes;
  rc = Parse_UINT32(
      &buffer,
      &response_size,
      &response_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM_RC response_code;
  std::string response_code_bytes;
  rc = Parse_TPM_RC(
      &buffer,
      &response_code,
      &response_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (response_size != response.size()) {
    error_reporter.Run(TPM_RC_SIZE);
    return;
  }
  if (response_code != TPM_RC_SUCCESS) {
    error_reporter.Run(response_code);
    return;
  }
  TPM_CC command_code = TPM_CC_SequenceUpdate;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string authorization_section_bytes;
  if (tag == TPM_ST_SESSIONS) {
    UINT32 parameter_section_size = buffer.size();
    rc = Parse_UINT32(&buffer, &parameter_section_size, NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    if (parameter_section_size > buffer.size()) {
      error_reporter.Run(TPM_RC_INSUFFICIENT);
      return;
    }
    authorization_section_bytes = buffer.substr(parameter_section_size);
    // Keep the parameter section in |buffer|.
    buffer.erase(parameter_section_size);
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(response_code_bytes.data(),
               response_code_bytes.size());
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(buffer.data(),
               buffer.size());
  std::string response_hash(32, 0);
  hash->Finish(string_as_array(&response_hash), response_hash.size());
  if (tag == TPM_ST_SESSIONS) {
    if (!authorization_delegate->CheckResponseAuthorization(
        response_hash,
        authorization_section_bytes)) {
      error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
      return;
    }
  }
  callback.Run(response_code);
}

void Tpm::SequenceUpdate(
      TPMI_DH_OBJECT sequence_handle,
      const std::string& sequence_handle_name,
      TPM2B_MAX_BUFFER buffer,
      AuthorizationDelegate* authorization_delegate,
      const SequenceUpdateResponse& callback) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(SequenceUpdateErrorCallback, callback);
  base::Callback<void(const std::string&)> parser =
      base::Bind(SequenceUpdateResponseParser,
                 callback,
                 authorization_delegate);
  TPMI_ST_COMMAND_TAG tag = TPM_ST_NO_SESSIONS;
  UINT32 command_size = 10;  // Header size.
  std::string handle_section_bytes;
  std::string parameter_section_bytes;
  TPM_CC command_code = TPM_CC_SequenceUpdate;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string sequence_handle_bytes;
  rc = Serialize_TPMI_DH_OBJECT(
      sequence_handle,
      &sequence_handle_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string buffer_bytes;
  rc = Serialize_TPM2B_MAX_BUFFER(
      buffer,
      &buffer_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  // Encrypt just the parameter data, not the size.
  std::string tmp = buffer_bytes.substr(2);
  if (!authorization_delegate->EncryptCommandParameter(&tmp)) {
    error_reporter.Run(TRUNKS_RC_ENCRYPTION_FAILED);
    return;
  }
  buffer_bytes.replace(2, std::string::npos, tmp);
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(sequence_handle_name.data(),
               sequence_handle_name.size());
  handle_section_bytes += sequence_handle_bytes;
  command_size += sequence_handle_bytes.size();
  hash->Update(buffer_bytes.data(),
               buffer_bytes.size());
  parameter_section_bytes += buffer_bytes;
  command_size += buffer_bytes.size();
  std::string command_hash(32, 0);
  hash->Finish(string_as_array(&command_hash), command_hash.size());
  std::string authorization_section_bytes;
  if (!authorization_delegate->GetCommandAuthorization(
      command_hash,
      &authorization_section_bytes)) {
    error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
    return;
  }
  std::string authorization_size_bytes;
  if (!authorization_section_bytes.empty()) {
    tag = TPM_ST_SESSIONS;
    std::string tmp;
    rc = Serialize_UINT32(authorization_section_bytes.size(),
                          &authorization_size_bytes);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    command_size += authorization_size_bytes.size() +
                    authorization_section_bytes.size();
  }
  std::string tag_bytes;
  rc = Serialize_TPMI_ST_COMMAND_TAG(
      tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command_size_bytes;
  rc = Serialize_UINT32(
      command_size,
      &command_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command = tag_bytes +
                        command_size_bytes +
                        command_code_bytes +
                        handle_section_bytes +
                        authorization_size_bytes +
                        authorization_section_bytes +
                        parameter_section_bytes;
  CHECK(command.size() == command_size) << "Command size mismatch!";
  transceiver_->SendCommand(command, parser);
}

void SequenceCompleteErrorCallback(
    const Tpm::SequenceCompleteResponse& callback,
    TPM_RC response_code) {
  callback.Run(response_code,
               TPM2B_DIGEST(),
               TPMT_TK_HASHCHECK());
}

void SequenceCompleteResponseParser(
    const Tpm::SequenceCompleteResponse& callback,
    AuthorizationDelegate* authorization_delegate,
    const std::string& response) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(SequenceCompleteErrorCallback, callback);
  std::string buffer(response);
  TPM_ST tag;
  std::string tag_bytes;
  rc = Parse_TPM_ST(
      &buffer,
      &tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  UINT32 response_size;
  std::string response_size_bytes;
  rc = Parse_UINT32(
      &buffer,
      &response_size,
      &response_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM_RC response_code;
  std::string response_code_bytes;
  rc = Parse_TPM_RC(
      &buffer,
      &response_code,
      &response_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (response_size != response.size()) {
    error_reporter.Run(TPM_RC_SIZE);
    return;
  }
  if (response_code != TPM_RC_SUCCESS) {
    error_reporter.Run(response_code);
    return;
  }
  TPM_CC command_code = TPM_CC_SequenceComplete;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string authorization_section_bytes;
  if (tag == TPM_ST_SESSIONS) {
    UINT32 parameter_section_size = buffer.size();
    rc = Parse_UINT32(&buffer, &parameter_section_size, NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    if (parameter_section_size > buffer.size()) {
      error_reporter.Run(TPM_RC_INSUFFICIENT);
      return;
    }
    authorization_section_bytes = buffer.substr(parameter_section_size);
    // Keep the parameter section in |buffer|.
    buffer.erase(parameter_section_size);
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(response_code_bytes.data(),
               response_code_bytes.size());
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(buffer.data(),
               buffer.size());
  std::string response_hash(32, 0);
  hash->Finish(string_as_array(&response_hash), response_hash.size());
  if (tag == TPM_ST_SESSIONS) {
    if (!authorization_delegate->CheckResponseAuthorization(
        response_hash,
        authorization_section_bytes)) {
      error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
      return;
    }
  }
  TPM2B_DIGEST result;
  std::string result_bytes;
  rc = Parse_TPM2B_DIGEST(
      &buffer,
      &result,
      &result_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPMT_TK_HASHCHECK validation;
  std::string validation_bytes;
  rc = Parse_TPMT_TK_HASHCHECK(
      &buffer,
      &validation,
      &validation_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (tag == TPM_ST_SESSIONS) {
    // Decrypt just the parameter data, not the size.
    std::string tmp = result_bytes.substr(2);
    if (!authorization_delegate->DecryptResponseParameter(&tmp)) {
      error_reporter.Run(TRUNKS_RC_ENCRYPTION_FAILED);
      return;
    }
    result_bytes.replace(2, std::string::npos, tmp);
    rc = Parse_TPM2B_DIGEST(
        &result_bytes,
        &result,
        NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
  }
  callback.Run(response_code,
               result,
               validation);
}

void Tpm::SequenceComplete(
      TPMI_DH_OBJECT sequence_handle,
      const std::string& sequence_handle_name,
      TPMI_RH_HIERARCHY hierarchy,
      const std::string& hierarchy_name,
      TPM2B_MAX_BUFFER buffer,
      AuthorizationDelegate* authorization_delegate,
      const SequenceCompleteResponse& callback) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(SequenceCompleteErrorCallback, callback);
  base::Callback<void(const std::string&)> parser =
      base::Bind(SequenceCompleteResponseParser,
                 callback,
                 authorization_delegate);
  TPMI_ST_COMMAND_TAG tag = TPM_ST_NO_SESSIONS;
  UINT32 command_size = 10;  // Header size.
  std::string handle_section_bytes;
  std::string parameter_section_bytes;
  TPM_CC command_code = TPM_CC_SequenceComplete;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string sequence_handle_bytes;
  rc = Serialize_TPMI_DH_OBJECT(
      sequence_handle,
      &sequence_handle_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string buffer_bytes;
  rc = Serialize_TPM2B_MAX_BUFFER(
      buffer,
      &buffer_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string hierarchy_bytes;
  rc = Serialize_TPMI_RH_HIERARCHY(
      hierarchy,
      &hierarchy_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  // Encrypt just the parameter data, not the size.
  std::string tmp = buffer_bytes.substr(2);
  if (!authorization_delegate->EncryptCommandParameter(&tmp)) {
    error_reporter.Run(TRUNKS_RC_ENCRYPTION_FAILED);
    return;
  }
  buffer_bytes.replace(2, std::string::npos, tmp);
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(sequence_handle_name.data(),
               sequence_handle_name.size());
  handle_section_bytes += sequence_handle_bytes;
  command_size += sequence_handle_bytes.size();
  hash->Update(hierarchy_name.data(),
               hierarchy_name.size());
  handle_section_bytes += hierarchy_bytes;
  command_size += hierarchy_bytes.size();
  hash->Update(buffer_bytes.data(),
               buffer_bytes.size());
  parameter_section_bytes += buffer_bytes;
  command_size += buffer_bytes.size();
  std::string command_hash(32, 0);
  hash->Finish(string_as_array(&command_hash), command_hash.size());
  std::string authorization_section_bytes;
  if (!authorization_delegate->GetCommandAuthorization(
      command_hash,
      &authorization_section_bytes)) {
    error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
    return;
  }
  std::string authorization_size_bytes;
  if (!authorization_section_bytes.empty()) {
    tag = TPM_ST_SESSIONS;
    std::string tmp;
    rc = Serialize_UINT32(authorization_section_bytes.size(),
                          &authorization_size_bytes);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    command_size += authorization_size_bytes.size() +
                    authorization_section_bytes.size();
  }
  std::string tag_bytes;
  rc = Serialize_TPMI_ST_COMMAND_TAG(
      tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command_size_bytes;
  rc = Serialize_UINT32(
      command_size,
      &command_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command = tag_bytes +
                        command_size_bytes +
                        command_code_bytes +
                        handle_section_bytes +
                        authorization_size_bytes +
                        authorization_section_bytes +
                        parameter_section_bytes;
  CHECK(command.size() == command_size) << "Command size mismatch!";
  transceiver_->SendCommand(command, parser);
}

void EventSequenceCompleteErrorCallback(
    const Tpm::EventSequenceCompleteResponse& callback,
    TPM_RC response_code) {
  callback.Run(response_code,
               TPML_DIGEST_VALUES());
}

void EventSequenceCompleteResponseParser(
    const Tpm::EventSequenceCompleteResponse& callback,
    AuthorizationDelegate* authorization_delegate,
    const std::string& response) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(EventSequenceCompleteErrorCallback, callback);
  std::string buffer(response);
  TPM_ST tag;
  std::string tag_bytes;
  rc = Parse_TPM_ST(
      &buffer,
      &tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  UINT32 response_size;
  std::string response_size_bytes;
  rc = Parse_UINT32(
      &buffer,
      &response_size,
      &response_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM_RC response_code;
  std::string response_code_bytes;
  rc = Parse_TPM_RC(
      &buffer,
      &response_code,
      &response_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (response_size != response.size()) {
    error_reporter.Run(TPM_RC_SIZE);
    return;
  }
  if (response_code != TPM_RC_SUCCESS) {
    error_reporter.Run(response_code);
    return;
  }
  TPM_CC command_code = TPM_CC_EventSequenceComplete;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string authorization_section_bytes;
  if (tag == TPM_ST_SESSIONS) {
    UINT32 parameter_section_size = buffer.size();
    rc = Parse_UINT32(&buffer, &parameter_section_size, NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    if (parameter_section_size > buffer.size()) {
      error_reporter.Run(TPM_RC_INSUFFICIENT);
      return;
    }
    authorization_section_bytes = buffer.substr(parameter_section_size);
    // Keep the parameter section in |buffer|.
    buffer.erase(parameter_section_size);
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(response_code_bytes.data(),
               response_code_bytes.size());
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(buffer.data(),
               buffer.size());
  std::string response_hash(32, 0);
  hash->Finish(string_as_array(&response_hash), response_hash.size());
  if (tag == TPM_ST_SESSIONS) {
    if (!authorization_delegate->CheckResponseAuthorization(
        response_hash,
        authorization_section_bytes)) {
      error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
      return;
    }
  }
  TPML_DIGEST_VALUES results;
  std::string results_bytes;
  rc = Parse_TPML_DIGEST_VALUES(
      &buffer,
      &results,
      &results_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (tag == TPM_ST_SESSIONS) {
    // Decrypt just the parameter data, not the size.
    std::string tmp = results_bytes.substr(2);
    if (!authorization_delegate->DecryptResponseParameter(&tmp)) {
      error_reporter.Run(TRUNKS_RC_ENCRYPTION_FAILED);
      return;
    }
    results_bytes.replace(2, std::string::npos, tmp);
    rc = Parse_TPML_DIGEST_VALUES(
        &results_bytes,
        &results,
        NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
  }
  callback.Run(response_code,
               results);
}

void Tpm::EventSequenceComplete(
      TPMI_DH_PCR pcr_handle,
      const std::string& pcr_handle_name,
      TPMI_DH_OBJECT sequence_handle,
      const std::string& sequence_handle_name,
      TPM2B_MAX_BUFFER buffer,
      AuthorizationDelegate* authorization_delegate,
      const EventSequenceCompleteResponse& callback) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(EventSequenceCompleteErrorCallback, callback);
  base::Callback<void(const std::string&)> parser =
      base::Bind(EventSequenceCompleteResponseParser,
                 callback,
                 authorization_delegate);
  TPMI_ST_COMMAND_TAG tag = TPM_ST_NO_SESSIONS;
  UINT32 command_size = 10;  // Header size.
  std::string handle_section_bytes;
  std::string parameter_section_bytes;
  TPM_CC command_code = TPM_CC_EventSequenceComplete;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string pcr_handle_bytes;
  rc = Serialize_TPMI_DH_PCR(
      pcr_handle,
      &pcr_handle_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string sequence_handle_bytes;
  rc = Serialize_TPMI_DH_OBJECT(
      sequence_handle,
      &sequence_handle_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string buffer_bytes;
  rc = Serialize_TPM2B_MAX_BUFFER(
      buffer,
      &buffer_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  // Encrypt just the parameter data, not the size.
  std::string tmp = buffer_bytes.substr(2);
  if (!authorization_delegate->EncryptCommandParameter(&tmp)) {
    error_reporter.Run(TRUNKS_RC_ENCRYPTION_FAILED);
    return;
  }
  buffer_bytes.replace(2, std::string::npos, tmp);
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(pcr_handle_name.data(),
               pcr_handle_name.size());
  handle_section_bytes += pcr_handle_bytes;
  command_size += pcr_handle_bytes.size();
  hash->Update(sequence_handle_name.data(),
               sequence_handle_name.size());
  handle_section_bytes += sequence_handle_bytes;
  command_size += sequence_handle_bytes.size();
  hash->Update(buffer_bytes.data(),
               buffer_bytes.size());
  parameter_section_bytes += buffer_bytes;
  command_size += buffer_bytes.size();
  std::string command_hash(32, 0);
  hash->Finish(string_as_array(&command_hash), command_hash.size());
  std::string authorization_section_bytes;
  if (!authorization_delegate->GetCommandAuthorization(
      command_hash,
      &authorization_section_bytes)) {
    error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
    return;
  }
  std::string authorization_size_bytes;
  if (!authorization_section_bytes.empty()) {
    tag = TPM_ST_SESSIONS;
    std::string tmp;
    rc = Serialize_UINT32(authorization_section_bytes.size(),
                          &authorization_size_bytes);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    command_size += authorization_size_bytes.size() +
                    authorization_section_bytes.size();
  }
  std::string tag_bytes;
  rc = Serialize_TPMI_ST_COMMAND_TAG(
      tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command_size_bytes;
  rc = Serialize_UINT32(
      command_size,
      &command_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command = tag_bytes +
                        command_size_bytes +
                        command_code_bytes +
                        handle_section_bytes +
                        authorization_size_bytes +
                        authorization_section_bytes +
                        parameter_section_bytes;
  CHECK(command.size() == command_size) << "Command size mismatch!";
  transceiver_->SendCommand(command, parser);
}

void CertifyErrorCallback(
    const Tpm::CertifyResponse& callback,
    TPM_RC response_code) {
  callback.Run(response_code,
               TPM2B_ATTEST(),
               TPMT_SIGNATURE());
}

void CertifyResponseParser(
    const Tpm::CertifyResponse& callback,
    AuthorizationDelegate* authorization_delegate,
    const std::string& response) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(CertifyErrorCallback, callback);
  std::string buffer(response);
  TPM_ST tag;
  std::string tag_bytes;
  rc = Parse_TPM_ST(
      &buffer,
      &tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  UINT32 response_size;
  std::string response_size_bytes;
  rc = Parse_UINT32(
      &buffer,
      &response_size,
      &response_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM_RC response_code;
  std::string response_code_bytes;
  rc = Parse_TPM_RC(
      &buffer,
      &response_code,
      &response_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (response_size != response.size()) {
    error_reporter.Run(TPM_RC_SIZE);
    return;
  }
  if (response_code != TPM_RC_SUCCESS) {
    error_reporter.Run(response_code);
    return;
  }
  TPM_CC command_code = TPM_CC_Certify;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string authorization_section_bytes;
  if (tag == TPM_ST_SESSIONS) {
    UINT32 parameter_section_size = buffer.size();
    rc = Parse_UINT32(&buffer, &parameter_section_size, NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    if (parameter_section_size > buffer.size()) {
      error_reporter.Run(TPM_RC_INSUFFICIENT);
      return;
    }
    authorization_section_bytes = buffer.substr(parameter_section_size);
    // Keep the parameter section in |buffer|.
    buffer.erase(parameter_section_size);
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(response_code_bytes.data(),
               response_code_bytes.size());
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(buffer.data(),
               buffer.size());
  std::string response_hash(32, 0);
  hash->Finish(string_as_array(&response_hash), response_hash.size());
  if (tag == TPM_ST_SESSIONS) {
    if (!authorization_delegate->CheckResponseAuthorization(
        response_hash,
        authorization_section_bytes)) {
      error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
      return;
    }
  }
  TPM2B_ATTEST certify_info;
  std::string certify_info_bytes;
  rc = Parse_TPM2B_ATTEST(
      &buffer,
      &certify_info,
      &certify_info_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPMT_SIGNATURE signature;
  std::string signature_bytes;
  rc = Parse_TPMT_SIGNATURE(
      &buffer,
      &signature,
      &signature_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (tag == TPM_ST_SESSIONS) {
    // Decrypt just the parameter data, not the size.
    std::string tmp = certify_info_bytes.substr(2);
    if (!authorization_delegate->DecryptResponseParameter(&tmp)) {
      error_reporter.Run(TRUNKS_RC_ENCRYPTION_FAILED);
      return;
    }
    certify_info_bytes.replace(2, std::string::npos, tmp);
    rc = Parse_TPM2B_ATTEST(
        &certify_info_bytes,
        &certify_info,
        NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
  }
  callback.Run(response_code,
               certify_info,
               signature);
}

void Tpm::Certify(
      TPMI_DH_OBJECT object_handle,
      const std::string& object_handle_name,
      TPMI_DH_OBJECT sign_handle,
      const std::string& sign_handle_name,
      TPM2B_DATA qualifying_data,
      TPMT_SIG_SCHEME in_scheme,
      AuthorizationDelegate* authorization_delegate,
      const CertifyResponse& callback) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(CertifyErrorCallback, callback);
  base::Callback<void(const std::string&)> parser =
      base::Bind(CertifyResponseParser,
                 callback,
                 authorization_delegate);
  TPMI_ST_COMMAND_TAG tag = TPM_ST_NO_SESSIONS;
  UINT32 command_size = 10;  // Header size.
  std::string handle_section_bytes;
  std::string parameter_section_bytes;
  TPM_CC command_code = TPM_CC_Certify;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string object_handle_bytes;
  rc = Serialize_TPMI_DH_OBJECT(
      object_handle,
      &object_handle_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string sign_handle_bytes;
  rc = Serialize_TPMI_DH_OBJECT(
      sign_handle,
      &sign_handle_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string qualifying_data_bytes;
  rc = Serialize_TPM2B_DATA(
      qualifying_data,
      &qualifying_data_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string in_scheme_bytes;
  rc = Serialize_TPMT_SIG_SCHEME(
      in_scheme,
      &in_scheme_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  // Encrypt just the parameter data, not the size.
  std::string tmp = qualifying_data_bytes.substr(2);
  if (!authorization_delegate->EncryptCommandParameter(&tmp)) {
    error_reporter.Run(TRUNKS_RC_ENCRYPTION_FAILED);
    return;
  }
  qualifying_data_bytes.replace(2, std::string::npos, tmp);
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(object_handle_name.data(),
               object_handle_name.size());
  handle_section_bytes += object_handle_bytes;
  command_size += object_handle_bytes.size();
  hash->Update(sign_handle_name.data(),
               sign_handle_name.size());
  handle_section_bytes += sign_handle_bytes;
  command_size += sign_handle_bytes.size();
  hash->Update(qualifying_data_bytes.data(),
               qualifying_data_bytes.size());
  parameter_section_bytes += qualifying_data_bytes;
  command_size += qualifying_data_bytes.size();
  hash->Update(in_scheme_bytes.data(),
               in_scheme_bytes.size());
  parameter_section_bytes += in_scheme_bytes;
  command_size += in_scheme_bytes.size();
  std::string command_hash(32, 0);
  hash->Finish(string_as_array(&command_hash), command_hash.size());
  std::string authorization_section_bytes;
  if (!authorization_delegate->GetCommandAuthorization(
      command_hash,
      &authorization_section_bytes)) {
    error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
    return;
  }
  std::string authorization_size_bytes;
  if (!authorization_section_bytes.empty()) {
    tag = TPM_ST_SESSIONS;
    std::string tmp;
    rc = Serialize_UINT32(authorization_section_bytes.size(),
                          &authorization_size_bytes);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    command_size += authorization_size_bytes.size() +
                    authorization_section_bytes.size();
  }
  std::string tag_bytes;
  rc = Serialize_TPMI_ST_COMMAND_TAG(
      tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command_size_bytes;
  rc = Serialize_UINT32(
      command_size,
      &command_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command = tag_bytes +
                        command_size_bytes +
                        command_code_bytes +
                        handle_section_bytes +
                        authorization_size_bytes +
                        authorization_section_bytes +
                        parameter_section_bytes;
  CHECK(command.size() == command_size) << "Command size mismatch!";
  transceiver_->SendCommand(command, parser);
}

void CertifyCreationErrorCallback(
    const Tpm::CertifyCreationResponse& callback,
    TPM_RC response_code) {
  callback.Run(response_code,
               TPM2B_ATTEST(),
               TPMT_SIGNATURE());
}

void CertifyCreationResponseParser(
    const Tpm::CertifyCreationResponse& callback,
    AuthorizationDelegate* authorization_delegate,
    const std::string& response) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(CertifyCreationErrorCallback, callback);
  std::string buffer(response);
  TPM_ST tag;
  std::string tag_bytes;
  rc = Parse_TPM_ST(
      &buffer,
      &tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  UINT32 response_size;
  std::string response_size_bytes;
  rc = Parse_UINT32(
      &buffer,
      &response_size,
      &response_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM_RC response_code;
  std::string response_code_bytes;
  rc = Parse_TPM_RC(
      &buffer,
      &response_code,
      &response_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (response_size != response.size()) {
    error_reporter.Run(TPM_RC_SIZE);
    return;
  }
  if (response_code != TPM_RC_SUCCESS) {
    error_reporter.Run(response_code);
    return;
  }
  TPM_CC command_code = TPM_CC_CertifyCreation;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string authorization_section_bytes;
  if (tag == TPM_ST_SESSIONS) {
    UINT32 parameter_section_size = buffer.size();
    rc = Parse_UINT32(&buffer, &parameter_section_size, NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    if (parameter_section_size > buffer.size()) {
      error_reporter.Run(TPM_RC_INSUFFICIENT);
      return;
    }
    authorization_section_bytes = buffer.substr(parameter_section_size);
    // Keep the parameter section in |buffer|.
    buffer.erase(parameter_section_size);
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(response_code_bytes.data(),
               response_code_bytes.size());
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(buffer.data(),
               buffer.size());
  std::string response_hash(32, 0);
  hash->Finish(string_as_array(&response_hash), response_hash.size());
  if (tag == TPM_ST_SESSIONS) {
    if (!authorization_delegate->CheckResponseAuthorization(
        response_hash,
        authorization_section_bytes)) {
      error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
      return;
    }
  }
  TPM2B_ATTEST certify_info;
  std::string certify_info_bytes;
  rc = Parse_TPM2B_ATTEST(
      &buffer,
      &certify_info,
      &certify_info_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPMT_SIGNATURE signature;
  std::string signature_bytes;
  rc = Parse_TPMT_SIGNATURE(
      &buffer,
      &signature,
      &signature_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (tag == TPM_ST_SESSIONS) {
    // Decrypt just the parameter data, not the size.
    std::string tmp = certify_info_bytes.substr(2);
    if (!authorization_delegate->DecryptResponseParameter(&tmp)) {
      error_reporter.Run(TRUNKS_RC_ENCRYPTION_FAILED);
      return;
    }
    certify_info_bytes.replace(2, std::string::npos, tmp);
    rc = Parse_TPM2B_ATTEST(
        &certify_info_bytes,
        &certify_info,
        NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
  }
  callback.Run(response_code,
               certify_info,
               signature);
}

void Tpm::CertifyCreation(
      TPMI_DH_OBJECT sign_handle,
      const std::string& sign_handle_name,
      TPMI_DH_OBJECT object_handle,
      const std::string& object_handle_name,
      TPM2B_DATA qualifying_data,
      TPM2B_DIGEST creation_hash,
      TPMT_SIG_SCHEME in_scheme,
      TPMT_TK_CREATION creation_ticket,
      AuthorizationDelegate* authorization_delegate,
      const CertifyCreationResponse& callback) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(CertifyCreationErrorCallback, callback);
  base::Callback<void(const std::string&)> parser =
      base::Bind(CertifyCreationResponseParser,
                 callback,
                 authorization_delegate);
  TPMI_ST_COMMAND_TAG tag = TPM_ST_NO_SESSIONS;
  UINT32 command_size = 10;  // Header size.
  std::string handle_section_bytes;
  std::string parameter_section_bytes;
  TPM_CC command_code = TPM_CC_CertifyCreation;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string sign_handle_bytes;
  rc = Serialize_TPMI_DH_OBJECT(
      sign_handle,
      &sign_handle_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string object_handle_bytes;
  rc = Serialize_TPMI_DH_OBJECT(
      object_handle,
      &object_handle_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string qualifying_data_bytes;
  rc = Serialize_TPM2B_DATA(
      qualifying_data,
      &qualifying_data_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string creation_hash_bytes;
  rc = Serialize_TPM2B_DIGEST(
      creation_hash,
      &creation_hash_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string in_scheme_bytes;
  rc = Serialize_TPMT_SIG_SCHEME(
      in_scheme,
      &in_scheme_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string creation_ticket_bytes;
  rc = Serialize_TPMT_TK_CREATION(
      creation_ticket,
      &creation_ticket_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  // Encrypt just the parameter data, not the size.
  std::string tmp = qualifying_data_bytes.substr(2);
  if (!authorization_delegate->EncryptCommandParameter(&tmp)) {
    error_reporter.Run(TRUNKS_RC_ENCRYPTION_FAILED);
    return;
  }
  qualifying_data_bytes.replace(2, std::string::npos, tmp);
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(sign_handle_name.data(),
               sign_handle_name.size());
  handle_section_bytes += sign_handle_bytes;
  command_size += sign_handle_bytes.size();
  hash->Update(object_handle_name.data(),
               object_handle_name.size());
  handle_section_bytes += object_handle_bytes;
  command_size += object_handle_bytes.size();
  hash->Update(qualifying_data_bytes.data(),
               qualifying_data_bytes.size());
  parameter_section_bytes += qualifying_data_bytes;
  command_size += qualifying_data_bytes.size();
  hash->Update(creation_hash_bytes.data(),
               creation_hash_bytes.size());
  parameter_section_bytes += creation_hash_bytes;
  command_size += creation_hash_bytes.size();
  hash->Update(in_scheme_bytes.data(),
               in_scheme_bytes.size());
  parameter_section_bytes += in_scheme_bytes;
  command_size += in_scheme_bytes.size();
  hash->Update(creation_ticket_bytes.data(),
               creation_ticket_bytes.size());
  parameter_section_bytes += creation_ticket_bytes;
  command_size += creation_ticket_bytes.size();
  std::string command_hash(32, 0);
  hash->Finish(string_as_array(&command_hash), command_hash.size());
  std::string authorization_section_bytes;
  if (!authorization_delegate->GetCommandAuthorization(
      command_hash,
      &authorization_section_bytes)) {
    error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
    return;
  }
  std::string authorization_size_bytes;
  if (!authorization_section_bytes.empty()) {
    tag = TPM_ST_SESSIONS;
    std::string tmp;
    rc = Serialize_UINT32(authorization_section_bytes.size(),
                          &authorization_size_bytes);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    command_size += authorization_size_bytes.size() +
                    authorization_section_bytes.size();
  }
  std::string tag_bytes;
  rc = Serialize_TPMI_ST_COMMAND_TAG(
      tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command_size_bytes;
  rc = Serialize_UINT32(
      command_size,
      &command_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command = tag_bytes +
                        command_size_bytes +
                        command_code_bytes +
                        handle_section_bytes +
                        authorization_size_bytes +
                        authorization_section_bytes +
                        parameter_section_bytes;
  CHECK(command.size() == command_size) << "Command size mismatch!";
  transceiver_->SendCommand(command, parser);
}

void QuoteErrorCallback(
    const Tpm::QuoteResponse& callback,
    TPM_RC response_code) {
  callback.Run(response_code,
               TPM2B_ATTEST(),
               TPMT_SIGNATURE());
}

void QuoteResponseParser(
    const Tpm::QuoteResponse& callback,
    AuthorizationDelegate* authorization_delegate,
    const std::string& response) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(QuoteErrorCallback, callback);
  std::string buffer(response);
  TPM_ST tag;
  std::string tag_bytes;
  rc = Parse_TPM_ST(
      &buffer,
      &tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  UINT32 response_size;
  std::string response_size_bytes;
  rc = Parse_UINT32(
      &buffer,
      &response_size,
      &response_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM_RC response_code;
  std::string response_code_bytes;
  rc = Parse_TPM_RC(
      &buffer,
      &response_code,
      &response_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (response_size != response.size()) {
    error_reporter.Run(TPM_RC_SIZE);
    return;
  }
  if (response_code != TPM_RC_SUCCESS) {
    error_reporter.Run(response_code);
    return;
  }
  TPM_CC command_code = TPM_CC_Quote;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string authorization_section_bytes;
  if (tag == TPM_ST_SESSIONS) {
    UINT32 parameter_section_size = buffer.size();
    rc = Parse_UINT32(&buffer, &parameter_section_size, NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    if (parameter_section_size > buffer.size()) {
      error_reporter.Run(TPM_RC_INSUFFICIENT);
      return;
    }
    authorization_section_bytes = buffer.substr(parameter_section_size);
    // Keep the parameter section in |buffer|.
    buffer.erase(parameter_section_size);
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(response_code_bytes.data(),
               response_code_bytes.size());
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(buffer.data(),
               buffer.size());
  std::string response_hash(32, 0);
  hash->Finish(string_as_array(&response_hash), response_hash.size());
  if (tag == TPM_ST_SESSIONS) {
    if (!authorization_delegate->CheckResponseAuthorization(
        response_hash,
        authorization_section_bytes)) {
      error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
      return;
    }
  }
  TPM2B_ATTEST quoted;
  std::string quoted_bytes;
  rc = Parse_TPM2B_ATTEST(
      &buffer,
      &quoted,
      &quoted_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPMT_SIGNATURE signature;
  std::string signature_bytes;
  rc = Parse_TPMT_SIGNATURE(
      &buffer,
      &signature,
      &signature_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (tag == TPM_ST_SESSIONS) {
    // Decrypt just the parameter data, not the size.
    std::string tmp = quoted_bytes.substr(2);
    if (!authorization_delegate->DecryptResponseParameter(&tmp)) {
      error_reporter.Run(TRUNKS_RC_ENCRYPTION_FAILED);
      return;
    }
    quoted_bytes.replace(2, std::string::npos, tmp);
    rc = Parse_TPM2B_ATTEST(
        &quoted_bytes,
        &quoted,
        NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
  }
  callback.Run(response_code,
               quoted,
               signature);
}

void Tpm::Quote(
      TPMI_DH_OBJECT sign_handle,
      const std::string& sign_handle_name,
      TPM2B_DATA qualifying_data,
      TPMT_SIG_SCHEME in_scheme,
      TPML_PCR_SELECTION pcrselect,
      AuthorizationDelegate* authorization_delegate,
      const QuoteResponse& callback) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(QuoteErrorCallback, callback);
  base::Callback<void(const std::string&)> parser =
      base::Bind(QuoteResponseParser,
                 callback,
                 authorization_delegate);
  TPMI_ST_COMMAND_TAG tag = TPM_ST_NO_SESSIONS;
  UINT32 command_size = 10;  // Header size.
  std::string handle_section_bytes;
  std::string parameter_section_bytes;
  TPM_CC command_code = TPM_CC_Quote;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string sign_handle_bytes;
  rc = Serialize_TPMI_DH_OBJECT(
      sign_handle,
      &sign_handle_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string qualifying_data_bytes;
  rc = Serialize_TPM2B_DATA(
      qualifying_data,
      &qualifying_data_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string in_scheme_bytes;
  rc = Serialize_TPMT_SIG_SCHEME(
      in_scheme,
      &in_scheme_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string pcrselect_bytes;
  rc = Serialize_TPML_PCR_SELECTION(
      pcrselect,
      &pcrselect_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  // Encrypt just the parameter data, not the size.
  std::string tmp = qualifying_data_bytes.substr(2);
  if (!authorization_delegate->EncryptCommandParameter(&tmp)) {
    error_reporter.Run(TRUNKS_RC_ENCRYPTION_FAILED);
    return;
  }
  qualifying_data_bytes.replace(2, std::string::npos, tmp);
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(sign_handle_name.data(),
               sign_handle_name.size());
  handle_section_bytes += sign_handle_bytes;
  command_size += sign_handle_bytes.size();
  hash->Update(qualifying_data_bytes.data(),
               qualifying_data_bytes.size());
  parameter_section_bytes += qualifying_data_bytes;
  command_size += qualifying_data_bytes.size();
  hash->Update(in_scheme_bytes.data(),
               in_scheme_bytes.size());
  parameter_section_bytes += in_scheme_bytes;
  command_size += in_scheme_bytes.size();
  hash->Update(pcrselect_bytes.data(),
               pcrselect_bytes.size());
  parameter_section_bytes += pcrselect_bytes;
  command_size += pcrselect_bytes.size();
  std::string command_hash(32, 0);
  hash->Finish(string_as_array(&command_hash), command_hash.size());
  std::string authorization_section_bytes;
  if (!authorization_delegate->GetCommandAuthorization(
      command_hash,
      &authorization_section_bytes)) {
    error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
    return;
  }
  std::string authorization_size_bytes;
  if (!authorization_section_bytes.empty()) {
    tag = TPM_ST_SESSIONS;
    std::string tmp;
    rc = Serialize_UINT32(authorization_section_bytes.size(),
                          &authorization_size_bytes);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    command_size += authorization_size_bytes.size() +
                    authorization_section_bytes.size();
  }
  std::string tag_bytes;
  rc = Serialize_TPMI_ST_COMMAND_TAG(
      tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command_size_bytes;
  rc = Serialize_UINT32(
      command_size,
      &command_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command = tag_bytes +
                        command_size_bytes +
                        command_code_bytes +
                        handle_section_bytes +
                        authorization_size_bytes +
                        authorization_section_bytes +
                        parameter_section_bytes;
  CHECK(command.size() == command_size) << "Command size mismatch!";
  transceiver_->SendCommand(command, parser);
}

void GetSessionAuditDigestErrorCallback(
    const Tpm::GetSessionAuditDigestResponse& callback,
    TPM_RC response_code) {
  callback.Run(response_code,
               TPM2B_ATTEST(),
               TPMT_SIGNATURE());
}

void GetSessionAuditDigestResponseParser(
    const Tpm::GetSessionAuditDigestResponse& callback,
    AuthorizationDelegate* authorization_delegate,
    const std::string& response) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(GetSessionAuditDigestErrorCallback, callback);
  std::string buffer(response);
  TPM_ST tag;
  std::string tag_bytes;
  rc = Parse_TPM_ST(
      &buffer,
      &tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  UINT32 response_size;
  std::string response_size_bytes;
  rc = Parse_UINT32(
      &buffer,
      &response_size,
      &response_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM_RC response_code;
  std::string response_code_bytes;
  rc = Parse_TPM_RC(
      &buffer,
      &response_code,
      &response_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (response_size != response.size()) {
    error_reporter.Run(TPM_RC_SIZE);
    return;
  }
  if (response_code != TPM_RC_SUCCESS) {
    error_reporter.Run(response_code);
    return;
  }
  TPM_CC command_code = TPM_CC_GetSessionAuditDigest;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string authorization_section_bytes;
  if (tag == TPM_ST_SESSIONS) {
    UINT32 parameter_section_size = buffer.size();
    rc = Parse_UINT32(&buffer, &parameter_section_size, NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    if (parameter_section_size > buffer.size()) {
      error_reporter.Run(TPM_RC_INSUFFICIENT);
      return;
    }
    authorization_section_bytes = buffer.substr(parameter_section_size);
    // Keep the parameter section in |buffer|.
    buffer.erase(parameter_section_size);
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(response_code_bytes.data(),
               response_code_bytes.size());
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(buffer.data(),
               buffer.size());
  std::string response_hash(32, 0);
  hash->Finish(string_as_array(&response_hash), response_hash.size());
  if (tag == TPM_ST_SESSIONS) {
    if (!authorization_delegate->CheckResponseAuthorization(
        response_hash,
        authorization_section_bytes)) {
      error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
      return;
    }
  }
  TPM2B_ATTEST audit_info;
  std::string audit_info_bytes;
  rc = Parse_TPM2B_ATTEST(
      &buffer,
      &audit_info,
      &audit_info_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPMT_SIGNATURE signature;
  std::string signature_bytes;
  rc = Parse_TPMT_SIGNATURE(
      &buffer,
      &signature,
      &signature_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (tag == TPM_ST_SESSIONS) {
    // Decrypt just the parameter data, not the size.
    std::string tmp = audit_info_bytes.substr(2);
    if (!authorization_delegate->DecryptResponseParameter(&tmp)) {
      error_reporter.Run(TRUNKS_RC_ENCRYPTION_FAILED);
      return;
    }
    audit_info_bytes.replace(2, std::string::npos, tmp);
    rc = Parse_TPM2B_ATTEST(
        &audit_info_bytes,
        &audit_info,
        NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
  }
  callback.Run(response_code,
               audit_info,
               signature);
}

void Tpm::GetSessionAuditDigest(
      TPMI_RH_ENDORSEMENT privacy_admin_handle,
      const std::string& privacy_admin_handle_name,
      TPMI_DH_OBJECT sign_handle,
      const std::string& sign_handle_name,
      TPMI_SH_HMAC session_handle,
      const std::string& session_handle_name,
      TPM2B_DATA qualifying_data,
      TPMT_SIG_SCHEME in_scheme,
      AuthorizationDelegate* authorization_delegate,
      const GetSessionAuditDigestResponse& callback) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(GetSessionAuditDigestErrorCallback, callback);
  base::Callback<void(const std::string&)> parser =
      base::Bind(GetSessionAuditDigestResponseParser,
                 callback,
                 authorization_delegate);
  TPMI_ST_COMMAND_TAG tag = TPM_ST_NO_SESSIONS;
  UINT32 command_size = 10;  // Header size.
  std::string handle_section_bytes;
  std::string parameter_section_bytes;
  TPM_CC command_code = TPM_CC_GetSessionAuditDigest;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string privacy_admin_handle_bytes;
  rc = Serialize_TPMI_RH_ENDORSEMENT(
      privacy_admin_handle,
      &privacy_admin_handle_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string sign_handle_bytes;
  rc = Serialize_TPMI_DH_OBJECT(
      sign_handle,
      &sign_handle_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string session_handle_bytes;
  rc = Serialize_TPMI_SH_HMAC(
      session_handle,
      &session_handle_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string qualifying_data_bytes;
  rc = Serialize_TPM2B_DATA(
      qualifying_data,
      &qualifying_data_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string in_scheme_bytes;
  rc = Serialize_TPMT_SIG_SCHEME(
      in_scheme,
      &in_scheme_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  // Encrypt just the parameter data, not the size.
  std::string tmp = qualifying_data_bytes.substr(2);
  if (!authorization_delegate->EncryptCommandParameter(&tmp)) {
    error_reporter.Run(TRUNKS_RC_ENCRYPTION_FAILED);
    return;
  }
  qualifying_data_bytes.replace(2, std::string::npos, tmp);
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(privacy_admin_handle_name.data(),
               privacy_admin_handle_name.size());
  handle_section_bytes += privacy_admin_handle_bytes;
  command_size += privacy_admin_handle_bytes.size();
  hash->Update(sign_handle_name.data(),
               sign_handle_name.size());
  handle_section_bytes += sign_handle_bytes;
  command_size += sign_handle_bytes.size();
  hash->Update(session_handle_name.data(),
               session_handle_name.size());
  handle_section_bytes += session_handle_bytes;
  command_size += session_handle_bytes.size();
  hash->Update(qualifying_data_bytes.data(),
               qualifying_data_bytes.size());
  parameter_section_bytes += qualifying_data_bytes;
  command_size += qualifying_data_bytes.size();
  hash->Update(in_scheme_bytes.data(),
               in_scheme_bytes.size());
  parameter_section_bytes += in_scheme_bytes;
  command_size += in_scheme_bytes.size();
  std::string command_hash(32, 0);
  hash->Finish(string_as_array(&command_hash), command_hash.size());
  std::string authorization_section_bytes;
  if (!authorization_delegate->GetCommandAuthorization(
      command_hash,
      &authorization_section_bytes)) {
    error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
    return;
  }
  std::string authorization_size_bytes;
  if (!authorization_section_bytes.empty()) {
    tag = TPM_ST_SESSIONS;
    std::string tmp;
    rc = Serialize_UINT32(authorization_section_bytes.size(),
                          &authorization_size_bytes);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    command_size += authorization_size_bytes.size() +
                    authorization_section_bytes.size();
  }
  std::string tag_bytes;
  rc = Serialize_TPMI_ST_COMMAND_TAG(
      tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command_size_bytes;
  rc = Serialize_UINT32(
      command_size,
      &command_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command = tag_bytes +
                        command_size_bytes +
                        command_code_bytes +
                        handle_section_bytes +
                        authorization_size_bytes +
                        authorization_section_bytes +
                        parameter_section_bytes;
  CHECK(command.size() == command_size) << "Command size mismatch!";
  transceiver_->SendCommand(command, parser);
}

void GetCommandAuditDigestErrorCallback(
    const Tpm::GetCommandAuditDigestResponse& callback,
    TPM_RC response_code) {
  callback.Run(response_code,
               TPM2B_ATTEST(),
               TPMT_SIGNATURE());
}

void GetCommandAuditDigestResponseParser(
    const Tpm::GetCommandAuditDigestResponse& callback,
    AuthorizationDelegate* authorization_delegate,
    const std::string& response) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(GetCommandAuditDigestErrorCallback, callback);
  std::string buffer(response);
  TPM_ST tag;
  std::string tag_bytes;
  rc = Parse_TPM_ST(
      &buffer,
      &tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  UINT32 response_size;
  std::string response_size_bytes;
  rc = Parse_UINT32(
      &buffer,
      &response_size,
      &response_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM_RC response_code;
  std::string response_code_bytes;
  rc = Parse_TPM_RC(
      &buffer,
      &response_code,
      &response_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (response_size != response.size()) {
    error_reporter.Run(TPM_RC_SIZE);
    return;
  }
  if (response_code != TPM_RC_SUCCESS) {
    error_reporter.Run(response_code);
    return;
  }
  TPM_CC command_code = TPM_CC_GetCommandAuditDigest;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string authorization_section_bytes;
  if (tag == TPM_ST_SESSIONS) {
    UINT32 parameter_section_size = buffer.size();
    rc = Parse_UINT32(&buffer, &parameter_section_size, NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    if (parameter_section_size > buffer.size()) {
      error_reporter.Run(TPM_RC_INSUFFICIENT);
      return;
    }
    authorization_section_bytes = buffer.substr(parameter_section_size);
    // Keep the parameter section in |buffer|.
    buffer.erase(parameter_section_size);
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(response_code_bytes.data(),
               response_code_bytes.size());
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(buffer.data(),
               buffer.size());
  std::string response_hash(32, 0);
  hash->Finish(string_as_array(&response_hash), response_hash.size());
  if (tag == TPM_ST_SESSIONS) {
    if (!authorization_delegate->CheckResponseAuthorization(
        response_hash,
        authorization_section_bytes)) {
      error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
      return;
    }
  }
  TPM2B_ATTEST audit_info;
  std::string audit_info_bytes;
  rc = Parse_TPM2B_ATTEST(
      &buffer,
      &audit_info,
      &audit_info_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPMT_SIGNATURE signature;
  std::string signature_bytes;
  rc = Parse_TPMT_SIGNATURE(
      &buffer,
      &signature,
      &signature_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (tag == TPM_ST_SESSIONS) {
    // Decrypt just the parameter data, not the size.
    std::string tmp = audit_info_bytes.substr(2);
    if (!authorization_delegate->DecryptResponseParameter(&tmp)) {
      error_reporter.Run(TRUNKS_RC_ENCRYPTION_FAILED);
      return;
    }
    audit_info_bytes.replace(2, std::string::npos, tmp);
    rc = Parse_TPM2B_ATTEST(
        &audit_info_bytes,
        &audit_info,
        NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
  }
  callback.Run(response_code,
               audit_info,
               signature);
}

void Tpm::GetCommandAuditDigest(
      TPMI_RH_ENDORSEMENT privacy_handle,
      const std::string& privacy_handle_name,
      TPMI_DH_OBJECT sign_handle,
      const std::string& sign_handle_name,
      TPM2B_DATA qualifying_data,
      TPMT_SIG_SCHEME in_scheme,
      AuthorizationDelegate* authorization_delegate,
      const GetCommandAuditDigestResponse& callback) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(GetCommandAuditDigestErrorCallback, callback);
  base::Callback<void(const std::string&)> parser =
      base::Bind(GetCommandAuditDigestResponseParser,
                 callback,
                 authorization_delegate);
  TPMI_ST_COMMAND_TAG tag = TPM_ST_NO_SESSIONS;
  UINT32 command_size = 10;  // Header size.
  std::string handle_section_bytes;
  std::string parameter_section_bytes;
  TPM_CC command_code = TPM_CC_GetCommandAuditDigest;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string privacy_handle_bytes;
  rc = Serialize_TPMI_RH_ENDORSEMENT(
      privacy_handle,
      &privacy_handle_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string sign_handle_bytes;
  rc = Serialize_TPMI_DH_OBJECT(
      sign_handle,
      &sign_handle_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string qualifying_data_bytes;
  rc = Serialize_TPM2B_DATA(
      qualifying_data,
      &qualifying_data_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string in_scheme_bytes;
  rc = Serialize_TPMT_SIG_SCHEME(
      in_scheme,
      &in_scheme_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  // Encrypt just the parameter data, not the size.
  std::string tmp = qualifying_data_bytes.substr(2);
  if (!authorization_delegate->EncryptCommandParameter(&tmp)) {
    error_reporter.Run(TRUNKS_RC_ENCRYPTION_FAILED);
    return;
  }
  qualifying_data_bytes.replace(2, std::string::npos, tmp);
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(privacy_handle_name.data(),
               privacy_handle_name.size());
  handle_section_bytes += privacy_handle_bytes;
  command_size += privacy_handle_bytes.size();
  hash->Update(sign_handle_name.data(),
               sign_handle_name.size());
  handle_section_bytes += sign_handle_bytes;
  command_size += sign_handle_bytes.size();
  hash->Update(qualifying_data_bytes.data(),
               qualifying_data_bytes.size());
  parameter_section_bytes += qualifying_data_bytes;
  command_size += qualifying_data_bytes.size();
  hash->Update(in_scheme_bytes.data(),
               in_scheme_bytes.size());
  parameter_section_bytes += in_scheme_bytes;
  command_size += in_scheme_bytes.size();
  std::string command_hash(32, 0);
  hash->Finish(string_as_array(&command_hash), command_hash.size());
  std::string authorization_section_bytes;
  if (!authorization_delegate->GetCommandAuthorization(
      command_hash,
      &authorization_section_bytes)) {
    error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
    return;
  }
  std::string authorization_size_bytes;
  if (!authorization_section_bytes.empty()) {
    tag = TPM_ST_SESSIONS;
    std::string tmp;
    rc = Serialize_UINT32(authorization_section_bytes.size(),
                          &authorization_size_bytes);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    command_size += authorization_size_bytes.size() +
                    authorization_section_bytes.size();
  }
  std::string tag_bytes;
  rc = Serialize_TPMI_ST_COMMAND_TAG(
      tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command_size_bytes;
  rc = Serialize_UINT32(
      command_size,
      &command_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command = tag_bytes +
                        command_size_bytes +
                        command_code_bytes +
                        handle_section_bytes +
                        authorization_size_bytes +
                        authorization_section_bytes +
                        parameter_section_bytes;
  CHECK(command.size() == command_size) << "Command size mismatch!";
  transceiver_->SendCommand(command, parser);
}

void GetTimeErrorCallback(
    const Tpm::GetTimeResponse& callback,
    TPM_RC response_code) {
  callback.Run(response_code,
               TPM2B_ATTEST(),
               TPMT_SIGNATURE());
}

void GetTimeResponseParser(
    const Tpm::GetTimeResponse& callback,
    AuthorizationDelegate* authorization_delegate,
    const std::string& response) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(GetTimeErrorCallback, callback);
  std::string buffer(response);
  TPM_ST tag;
  std::string tag_bytes;
  rc = Parse_TPM_ST(
      &buffer,
      &tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  UINT32 response_size;
  std::string response_size_bytes;
  rc = Parse_UINT32(
      &buffer,
      &response_size,
      &response_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM_RC response_code;
  std::string response_code_bytes;
  rc = Parse_TPM_RC(
      &buffer,
      &response_code,
      &response_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (response_size != response.size()) {
    error_reporter.Run(TPM_RC_SIZE);
    return;
  }
  if (response_code != TPM_RC_SUCCESS) {
    error_reporter.Run(response_code);
    return;
  }
  TPM_CC command_code = TPM_CC_GetTime;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string authorization_section_bytes;
  if (tag == TPM_ST_SESSIONS) {
    UINT32 parameter_section_size = buffer.size();
    rc = Parse_UINT32(&buffer, &parameter_section_size, NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    if (parameter_section_size > buffer.size()) {
      error_reporter.Run(TPM_RC_INSUFFICIENT);
      return;
    }
    authorization_section_bytes = buffer.substr(parameter_section_size);
    // Keep the parameter section in |buffer|.
    buffer.erase(parameter_section_size);
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(response_code_bytes.data(),
               response_code_bytes.size());
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(buffer.data(),
               buffer.size());
  std::string response_hash(32, 0);
  hash->Finish(string_as_array(&response_hash), response_hash.size());
  if (tag == TPM_ST_SESSIONS) {
    if (!authorization_delegate->CheckResponseAuthorization(
        response_hash,
        authorization_section_bytes)) {
      error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
      return;
    }
  }
  TPM2B_ATTEST time_info;
  std::string time_info_bytes;
  rc = Parse_TPM2B_ATTEST(
      &buffer,
      &time_info,
      &time_info_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPMT_SIGNATURE signature;
  std::string signature_bytes;
  rc = Parse_TPMT_SIGNATURE(
      &buffer,
      &signature,
      &signature_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (tag == TPM_ST_SESSIONS) {
    // Decrypt just the parameter data, not the size.
    std::string tmp = time_info_bytes.substr(2);
    if (!authorization_delegate->DecryptResponseParameter(&tmp)) {
      error_reporter.Run(TRUNKS_RC_ENCRYPTION_FAILED);
      return;
    }
    time_info_bytes.replace(2, std::string::npos, tmp);
    rc = Parse_TPM2B_ATTEST(
        &time_info_bytes,
        &time_info,
        NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
  }
  callback.Run(response_code,
               time_info,
               signature);
}

void Tpm::GetTime(
      TPMI_RH_ENDORSEMENT privacy_admin_handle,
      const std::string& privacy_admin_handle_name,
      TPMI_DH_OBJECT sign_handle,
      const std::string& sign_handle_name,
      TPM2B_DATA qualifying_data,
      TPMT_SIG_SCHEME in_scheme,
      AuthorizationDelegate* authorization_delegate,
      const GetTimeResponse& callback) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(GetTimeErrorCallback, callback);
  base::Callback<void(const std::string&)> parser =
      base::Bind(GetTimeResponseParser,
                 callback,
                 authorization_delegate);
  TPMI_ST_COMMAND_TAG tag = TPM_ST_NO_SESSIONS;
  UINT32 command_size = 10;  // Header size.
  std::string handle_section_bytes;
  std::string parameter_section_bytes;
  TPM_CC command_code = TPM_CC_GetTime;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string privacy_admin_handle_bytes;
  rc = Serialize_TPMI_RH_ENDORSEMENT(
      privacy_admin_handle,
      &privacy_admin_handle_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string sign_handle_bytes;
  rc = Serialize_TPMI_DH_OBJECT(
      sign_handle,
      &sign_handle_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string qualifying_data_bytes;
  rc = Serialize_TPM2B_DATA(
      qualifying_data,
      &qualifying_data_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string in_scheme_bytes;
  rc = Serialize_TPMT_SIG_SCHEME(
      in_scheme,
      &in_scheme_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  // Encrypt just the parameter data, not the size.
  std::string tmp = qualifying_data_bytes.substr(2);
  if (!authorization_delegate->EncryptCommandParameter(&tmp)) {
    error_reporter.Run(TRUNKS_RC_ENCRYPTION_FAILED);
    return;
  }
  qualifying_data_bytes.replace(2, std::string::npos, tmp);
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(privacy_admin_handle_name.data(),
               privacy_admin_handle_name.size());
  handle_section_bytes += privacy_admin_handle_bytes;
  command_size += privacy_admin_handle_bytes.size();
  hash->Update(sign_handle_name.data(),
               sign_handle_name.size());
  handle_section_bytes += sign_handle_bytes;
  command_size += sign_handle_bytes.size();
  hash->Update(qualifying_data_bytes.data(),
               qualifying_data_bytes.size());
  parameter_section_bytes += qualifying_data_bytes;
  command_size += qualifying_data_bytes.size();
  hash->Update(in_scheme_bytes.data(),
               in_scheme_bytes.size());
  parameter_section_bytes += in_scheme_bytes;
  command_size += in_scheme_bytes.size();
  std::string command_hash(32, 0);
  hash->Finish(string_as_array(&command_hash), command_hash.size());
  std::string authorization_section_bytes;
  if (!authorization_delegate->GetCommandAuthorization(
      command_hash,
      &authorization_section_bytes)) {
    error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
    return;
  }
  std::string authorization_size_bytes;
  if (!authorization_section_bytes.empty()) {
    tag = TPM_ST_SESSIONS;
    std::string tmp;
    rc = Serialize_UINT32(authorization_section_bytes.size(),
                          &authorization_size_bytes);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    command_size += authorization_size_bytes.size() +
                    authorization_section_bytes.size();
  }
  std::string tag_bytes;
  rc = Serialize_TPMI_ST_COMMAND_TAG(
      tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command_size_bytes;
  rc = Serialize_UINT32(
      command_size,
      &command_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command = tag_bytes +
                        command_size_bytes +
                        command_code_bytes +
                        handle_section_bytes +
                        authorization_size_bytes +
                        authorization_section_bytes +
                        parameter_section_bytes;
  CHECK(command.size() == command_size) << "Command size mismatch!";
  transceiver_->SendCommand(command, parser);
}

void CommitErrorCallback(
    const Tpm::CommitResponse& callback,
    TPM_RC response_code) {
  callback.Run(response_code,
               UINT32(),
               TPM2B_ECC_POINT(),
               TPM2B_ECC_POINT(),
               TPM2B_ECC_POINT(),
               UINT16());
}

void CommitResponseParser(
    const Tpm::CommitResponse& callback,
    AuthorizationDelegate* authorization_delegate,
    const std::string& response) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(CommitErrorCallback, callback);
  std::string buffer(response);
  TPM_ST tag;
  std::string tag_bytes;
  rc = Parse_TPM_ST(
      &buffer,
      &tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  UINT32 response_size;
  std::string response_size_bytes;
  rc = Parse_UINT32(
      &buffer,
      &response_size,
      &response_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM_RC response_code;
  std::string response_code_bytes;
  rc = Parse_TPM_RC(
      &buffer,
      &response_code,
      &response_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (response_size != response.size()) {
    error_reporter.Run(TPM_RC_SIZE);
    return;
  }
  if (response_code != TPM_RC_SUCCESS) {
    error_reporter.Run(response_code);
    return;
  }
  TPM_CC command_code = TPM_CC_Commit;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string authorization_section_bytes;
  if (tag == TPM_ST_SESSIONS) {
    UINT32 parameter_section_size = buffer.size();
    rc = Parse_UINT32(&buffer, &parameter_section_size, NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    if (parameter_section_size > buffer.size()) {
      error_reporter.Run(TPM_RC_INSUFFICIENT);
      return;
    }
    authorization_section_bytes = buffer.substr(parameter_section_size);
    // Keep the parameter section in |buffer|.
    buffer.erase(parameter_section_size);
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(response_code_bytes.data(),
               response_code_bytes.size());
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(buffer.data(),
               buffer.size());
  std::string response_hash(32, 0);
  hash->Finish(string_as_array(&response_hash), response_hash.size());
  if (tag == TPM_ST_SESSIONS) {
    if (!authorization_delegate->CheckResponseAuthorization(
        response_hash,
        authorization_section_bytes)) {
      error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
      return;
    }
  }
  UINT32 param_size;
  std::string param_size_bytes;
  rc = Parse_UINT32(
      &buffer,
      &param_size,
      &param_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM2B_ECC_POINT k;
  std::string k_bytes;
  rc = Parse_TPM2B_ECC_POINT(
      &buffer,
      &k,
      &k_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM2B_ECC_POINT l;
  std::string l_bytes;
  rc = Parse_TPM2B_ECC_POINT(
      &buffer,
      &l,
      &l_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM2B_ECC_POINT e;
  std::string e_bytes;
  rc = Parse_TPM2B_ECC_POINT(
      &buffer,
      &e,
      &e_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  UINT16 counter;
  std::string counter_bytes;
  rc = Parse_UINT16(
      &buffer,
      &counter,
      &counter_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (tag == TPM_ST_SESSIONS) {
    // Decrypt just the parameter data, not the size.
    std::string tmp = param_size_bytes.substr(2);
    if (!authorization_delegate->DecryptResponseParameter(&tmp)) {
      error_reporter.Run(TRUNKS_RC_ENCRYPTION_FAILED);
      return;
    }
    param_size_bytes.replace(2, std::string::npos, tmp);
    rc = Parse_UINT32(
        &param_size_bytes,
        &param_size,
        NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
  }
  callback.Run(response_code,
               param_size,
               k,
               l,
               e,
               counter);
}

void Tpm::Commit(
      TPMI_DH_OBJECT sign_handle,
      const std::string& sign_handle_name,
      UINT32 param_size,
      TPM2B_ECC_POINT p1,
      TPM2B_SENSITIVE_DATA s2,
      TPM2B_ECC_PARAMETER y2,
      AuthorizationDelegate* authorization_delegate,
      const CommitResponse& callback) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(CommitErrorCallback, callback);
  base::Callback<void(const std::string&)> parser =
      base::Bind(CommitResponseParser,
                 callback,
                 authorization_delegate);
  TPMI_ST_COMMAND_TAG tag = TPM_ST_NO_SESSIONS;
  UINT32 command_size = 10;  // Header size.
  std::string handle_section_bytes;
  std::string parameter_section_bytes;
  TPM_CC command_code = TPM_CC_Commit;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string param_size_bytes;
  rc = Serialize_UINT32(
      param_size,
      &param_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string sign_handle_bytes;
  rc = Serialize_TPMI_DH_OBJECT(
      sign_handle,
      &sign_handle_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string p1_bytes;
  rc = Serialize_TPM2B_ECC_POINT(
      p1,
      &p1_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string s2_bytes;
  rc = Serialize_TPM2B_SENSITIVE_DATA(
      s2,
      &s2_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string y2_bytes;
  rc = Serialize_TPM2B_ECC_PARAMETER(
      y2,
      &y2_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(sign_handle_name.data(),
               sign_handle_name.size());
  handle_section_bytes += sign_handle_bytes;
  command_size += sign_handle_bytes.size();
  hash->Update(param_size_bytes.data(),
               param_size_bytes.size());
  parameter_section_bytes += param_size_bytes;
  command_size += param_size_bytes.size();
  hash->Update(p1_bytes.data(),
               p1_bytes.size());
  parameter_section_bytes += p1_bytes;
  command_size += p1_bytes.size();
  hash->Update(s2_bytes.data(),
               s2_bytes.size());
  parameter_section_bytes += s2_bytes;
  command_size += s2_bytes.size();
  hash->Update(y2_bytes.data(),
               y2_bytes.size());
  parameter_section_bytes += y2_bytes;
  command_size += y2_bytes.size();
  std::string command_hash(32, 0);
  hash->Finish(string_as_array(&command_hash), command_hash.size());
  std::string authorization_section_bytes;
  if (!authorization_delegate->GetCommandAuthorization(
      command_hash,
      &authorization_section_bytes)) {
    error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
    return;
  }
  std::string authorization_size_bytes;
  if (!authorization_section_bytes.empty()) {
    tag = TPM_ST_SESSIONS;
    std::string tmp;
    rc = Serialize_UINT32(authorization_section_bytes.size(),
                          &authorization_size_bytes);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    command_size += authorization_size_bytes.size() +
                    authorization_section_bytes.size();
  }
  std::string tag_bytes;
  rc = Serialize_TPMI_ST_COMMAND_TAG(
      tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command_size_bytes;
  rc = Serialize_UINT32(
      command_size,
      &command_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command = tag_bytes +
                        command_size_bytes +
                        command_code_bytes +
                        handle_section_bytes +
                        authorization_size_bytes +
                        authorization_section_bytes +
                        parameter_section_bytes;
  CHECK(command.size() == command_size) << "Command size mismatch!";
  transceiver_->SendCommand(command, parser);
}

void EC_EphemeralErrorCallback(
    const Tpm::EC_EphemeralResponse& callback,
    TPM_RC response_code) {
  callback.Run(response_code,
               UINT32(),
               TPM2B_ECC_POINT(),
               UINT16());
}

void EC_EphemeralResponseParser(
    const Tpm::EC_EphemeralResponse& callback,
    AuthorizationDelegate* authorization_delegate,
    const std::string& response) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(EC_EphemeralErrorCallback, callback);
  std::string buffer(response);
  TPM_ST tag;
  std::string tag_bytes;
  rc = Parse_TPM_ST(
      &buffer,
      &tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  UINT32 response_size;
  std::string response_size_bytes;
  rc = Parse_UINT32(
      &buffer,
      &response_size,
      &response_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM_RC response_code;
  std::string response_code_bytes;
  rc = Parse_TPM_RC(
      &buffer,
      &response_code,
      &response_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (response_size != response.size()) {
    error_reporter.Run(TPM_RC_SIZE);
    return;
  }
  if (response_code != TPM_RC_SUCCESS) {
    error_reporter.Run(response_code);
    return;
  }
  TPM_CC command_code = TPM_CC_EC_Ephemeral;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string authorization_section_bytes;
  if (tag == TPM_ST_SESSIONS) {
    UINT32 parameter_section_size = buffer.size();
    rc = Parse_UINT32(&buffer, &parameter_section_size, NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    if (parameter_section_size > buffer.size()) {
      error_reporter.Run(TPM_RC_INSUFFICIENT);
      return;
    }
    authorization_section_bytes = buffer.substr(parameter_section_size);
    // Keep the parameter section in |buffer|.
    buffer.erase(parameter_section_size);
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(response_code_bytes.data(),
               response_code_bytes.size());
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(buffer.data(),
               buffer.size());
  std::string response_hash(32, 0);
  hash->Finish(string_as_array(&response_hash), response_hash.size());
  if (tag == TPM_ST_SESSIONS) {
    if (!authorization_delegate->CheckResponseAuthorization(
        response_hash,
        authorization_section_bytes)) {
      error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
      return;
    }
  }
  UINT32 param_size;
  std::string param_size_bytes;
  rc = Parse_UINT32(
      &buffer,
      &param_size,
      &param_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM2B_ECC_POINT q;
  std::string q_bytes;
  rc = Parse_TPM2B_ECC_POINT(
      &buffer,
      &q,
      &q_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  UINT16 counter;
  std::string counter_bytes;
  rc = Parse_UINT16(
      &buffer,
      &counter,
      &counter_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (tag == TPM_ST_SESSIONS) {
    // Decrypt just the parameter data, not the size.
    std::string tmp = param_size_bytes.substr(2);
    if (!authorization_delegate->DecryptResponseParameter(&tmp)) {
      error_reporter.Run(TRUNKS_RC_ENCRYPTION_FAILED);
      return;
    }
    param_size_bytes.replace(2, std::string::npos, tmp);
    rc = Parse_UINT32(
        &param_size_bytes,
        &param_size,
        NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
  }
  callback.Run(response_code,
               param_size,
               q,
               counter);
}

void Tpm::EC_Ephemeral(
      UINT32 param_size,
      TPMI_ECC_CURVE curve_id,
      AuthorizationDelegate* authorization_delegate,
      const EC_EphemeralResponse& callback) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(EC_EphemeralErrorCallback, callback);
  base::Callback<void(const std::string&)> parser =
      base::Bind(EC_EphemeralResponseParser,
                 callback,
                 authorization_delegate);
  TPMI_ST_COMMAND_TAG tag = TPM_ST_NO_SESSIONS;
  UINT32 command_size = 10;  // Header size.
  std::string handle_section_bytes;
  std::string parameter_section_bytes;
  TPM_CC command_code = TPM_CC_EC_Ephemeral;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string param_size_bytes;
  rc = Serialize_UINT32(
      param_size,
      &param_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string curve_id_bytes;
  rc = Serialize_TPMI_ECC_CURVE(
      curve_id,
      &curve_id_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(param_size_bytes.data(),
               param_size_bytes.size());
  parameter_section_bytes += param_size_bytes;
  command_size += param_size_bytes.size();
  hash->Update(curve_id_bytes.data(),
               curve_id_bytes.size());
  parameter_section_bytes += curve_id_bytes;
  command_size += curve_id_bytes.size();
  std::string command_hash(32, 0);
  hash->Finish(string_as_array(&command_hash), command_hash.size());
  std::string authorization_section_bytes;
  if (!authorization_delegate->GetCommandAuthorization(
      command_hash,
      &authorization_section_bytes)) {
    error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
    return;
  }
  std::string authorization_size_bytes;
  if (!authorization_section_bytes.empty()) {
    tag = TPM_ST_SESSIONS;
    std::string tmp;
    rc = Serialize_UINT32(authorization_section_bytes.size(),
                          &authorization_size_bytes);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    command_size += authorization_size_bytes.size() +
                    authorization_section_bytes.size();
  }
  std::string tag_bytes;
  rc = Serialize_TPMI_ST_COMMAND_TAG(
      tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command_size_bytes;
  rc = Serialize_UINT32(
      command_size,
      &command_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command = tag_bytes +
                        command_size_bytes +
                        command_code_bytes +
                        handle_section_bytes +
                        authorization_size_bytes +
                        authorization_section_bytes +
                        parameter_section_bytes;
  CHECK(command.size() == command_size) << "Command size mismatch!";
  transceiver_->SendCommand(command, parser);
}

void VerifySignatureErrorCallback(
    const Tpm::VerifySignatureResponse& callback,
    TPM_RC response_code) {
  callback.Run(response_code,
               TPMT_TK_VERIFIED());
}

void VerifySignatureResponseParser(
    const Tpm::VerifySignatureResponse& callback,
    AuthorizationDelegate* authorization_delegate,
    const std::string& response) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(VerifySignatureErrorCallback, callback);
  std::string buffer(response);
  TPM_ST tag;
  std::string tag_bytes;
  rc = Parse_TPM_ST(
      &buffer,
      &tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  UINT32 response_size;
  std::string response_size_bytes;
  rc = Parse_UINT32(
      &buffer,
      &response_size,
      &response_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM_RC response_code;
  std::string response_code_bytes;
  rc = Parse_TPM_RC(
      &buffer,
      &response_code,
      &response_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (response_size != response.size()) {
    error_reporter.Run(TPM_RC_SIZE);
    return;
  }
  if (response_code != TPM_RC_SUCCESS) {
    error_reporter.Run(response_code);
    return;
  }
  TPM_CC command_code = TPM_CC_VerifySignature;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string authorization_section_bytes;
  if (tag == TPM_ST_SESSIONS) {
    UINT32 parameter_section_size = buffer.size();
    rc = Parse_UINT32(&buffer, &parameter_section_size, NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    if (parameter_section_size > buffer.size()) {
      error_reporter.Run(TPM_RC_INSUFFICIENT);
      return;
    }
    authorization_section_bytes = buffer.substr(parameter_section_size);
    // Keep the parameter section in |buffer|.
    buffer.erase(parameter_section_size);
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(response_code_bytes.data(),
               response_code_bytes.size());
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(buffer.data(),
               buffer.size());
  std::string response_hash(32, 0);
  hash->Finish(string_as_array(&response_hash), response_hash.size());
  if (tag == TPM_ST_SESSIONS) {
    if (!authorization_delegate->CheckResponseAuthorization(
        response_hash,
        authorization_section_bytes)) {
      error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
      return;
    }
  }
  TPMT_TK_VERIFIED validation;
  std::string validation_bytes;
  rc = Parse_TPMT_TK_VERIFIED(
      &buffer,
      &validation,
      &validation_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (tag == TPM_ST_SESSIONS) {
    // Decrypt just the parameter data, not the size.
    std::string tmp = validation_bytes.substr(2);
    if (!authorization_delegate->DecryptResponseParameter(&tmp)) {
      error_reporter.Run(TRUNKS_RC_ENCRYPTION_FAILED);
      return;
    }
    validation_bytes.replace(2, std::string::npos, tmp);
    rc = Parse_TPMT_TK_VERIFIED(
        &validation_bytes,
        &validation,
        NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
  }
  callback.Run(response_code,
               validation);
}

void Tpm::VerifySignature(
      TPMI_DH_OBJECT key_handle,
      const std::string& key_handle_name,
      TPM2B_DIGEST digest,
      TPMT_SIGNATURE signature,
      AuthorizationDelegate* authorization_delegate,
      const VerifySignatureResponse& callback) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(VerifySignatureErrorCallback, callback);
  base::Callback<void(const std::string&)> parser =
      base::Bind(VerifySignatureResponseParser,
                 callback,
                 authorization_delegate);
  TPMI_ST_COMMAND_TAG tag = TPM_ST_NO_SESSIONS;
  UINT32 command_size = 10;  // Header size.
  std::string handle_section_bytes;
  std::string parameter_section_bytes;
  TPM_CC command_code = TPM_CC_VerifySignature;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string key_handle_bytes;
  rc = Serialize_TPMI_DH_OBJECT(
      key_handle,
      &key_handle_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string digest_bytes;
  rc = Serialize_TPM2B_DIGEST(
      digest,
      &digest_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string signature_bytes;
  rc = Serialize_TPMT_SIGNATURE(
      signature,
      &signature_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  // Encrypt just the parameter data, not the size.
  std::string tmp = digest_bytes.substr(2);
  if (!authorization_delegate->EncryptCommandParameter(&tmp)) {
    error_reporter.Run(TRUNKS_RC_ENCRYPTION_FAILED);
    return;
  }
  digest_bytes.replace(2, std::string::npos, tmp);
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(key_handle_name.data(),
               key_handle_name.size());
  handle_section_bytes += key_handle_bytes;
  command_size += key_handle_bytes.size();
  hash->Update(digest_bytes.data(),
               digest_bytes.size());
  parameter_section_bytes += digest_bytes;
  command_size += digest_bytes.size();
  hash->Update(signature_bytes.data(),
               signature_bytes.size());
  parameter_section_bytes += signature_bytes;
  command_size += signature_bytes.size();
  std::string command_hash(32, 0);
  hash->Finish(string_as_array(&command_hash), command_hash.size());
  std::string authorization_section_bytes;
  if (!authorization_delegate->GetCommandAuthorization(
      command_hash,
      &authorization_section_bytes)) {
    error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
    return;
  }
  std::string authorization_size_bytes;
  if (!authorization_section_bytes.empty()) {
    tag = TPM_ST_SESSIONS;
    std::string tmp;
    rc = Serialize_UINT32(authorization_section_bytes.size(),
                          &authorization_size_bytes);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    command_size += authorization_size_bytes.size() +
                    authorization_section_bytes.size();
  }
  std::string tag_bytes;
  rc = Serialize_TPMI_ST_COMMAND_TAG(
      tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command_size_bytes;
  rc = Serialize_UINT32(
      command_size,
      &command_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command = tag_bytes +
                        command_size_bytes +
                        command_code_bytes +
                        handle_section_bytes +
                        authorization_size_bytes +
                        authorization_section_bytes +
                        parameter_section_bytes;
  CHECK(command.size() == command_size) << "Command size mismatch!";
  transceiver_->SendCommand(command, parser);
}

void SignErrorCallback(
    const Tpm::SignResponse& callback,
    TPM_RC response_code) {
  callback.Run(response_code,
               TPMT_SIGNATURE());
}

void SignResponseParser(
    const Tpm::SignResponse& callback,
    AuthorizationDelegate* authorization_delegate,
    const std::string& response) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(SignErrorCallback, callback);
  std::string buffer(response);
  TPM_ST tag;
  std::string tag_bytes;
  rc = Parse_TPM_ST(
      &buffer,
      &tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  UINT32 response_size;
  std::string response_size_bytes;
  rc = Parse_UINT32(
      &buffer,
      &response_size,
      &response_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM_RC response_code;
  std::string response_code_bytes;
  rc = Parse_TPM_RC(
      &buffer,
      &response_code,
      &response_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (response_size != response.size()) {
    error_reporter.Run(TPM_RC_SIZE);
    return;
  }
  if (response_code != TPM_RC_SUCCESS) {
    error_reporter.Run(response_code);
    return;
  }
  TPM_CC command_code = TPM_CC_Sign;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string authorization_section_bytes;
  if (tag == TPM_ST_SESSIONS) {
    UINT32 parameter_section_size = buffer.size();
    rc = Parse_UINT32(&buffer, &parameter_section_size, NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    if (parameter_section_size > buffer.size()) {
      error_reporter.Run(TPM_RC_INSUFFICIENT);
      return;
    }
    authorization_section_bytes = buffer.substr(parameter_section_size);
    // Keep the parameter section in |buffer|.
    buffer.erase(parameter_section_size);
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(response_code_bytes.data(),
               response_code_bytes.size());
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(buffer.data(),
               buffer.size());
  std::string response_hash(32, 0);
  hash->Finish(string_as_array(&response_hash), response_hash.size());
  if (tag == TPM_ST_SESSIONS) {
    if (!authorization_delegate->CheckResponseAuthorization(
        response_hash,
        authorization_section_bytes)) {
      error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
      return;
    }
  }
  TPMT_SIGNATURE signature;
  std::string signature_bytes;
  rc = Parse_TPMT_SIGNATURE(
      &buffer,
      &signature,
      &signature_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (tag == TPM_ST_SESSIONS) {
    // Decrypt just the parameter data, not the size.
    std::string tmp = signature_bytes.substr(2);
    if (!authorization_delegate->DecryptResponseParameter(&tmp)) {
      error_reporter.Run(TRUNKS_RC_ENCRYPTION_FAILED);
      return;
    }
    signature_bytes.replace(2, std::string::npos, tmp);
    rc = Parse_TPMT_SIGNATURE(
        &signature_bytes,
        &signature,
        NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
  }
  callback.Run(response_code,
               signature);
}

void Tpm::Sign(
      TPMI_DH_OBJECT key_handle,
      const std::string& key_handle_name,
      TPM2B_DIGEST digest,
      TPMT_SIG_SCHEME in_scheme,
      TPMT_TK_HASHCHECK validation,
      AuthorizationDelegate* authorization_delegate,
      const SignResponse& callback) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(SignErrorCallback, callback);
  base::Callback<void(const std::string&)> parser =
      base::Bind(SignResponseParser,
                 callback,
                 authorization_delegate);
  TPMI_ST_COMMAND_TAG tag = TPM_ST_NO_SESSIONS;
  UINT32 command_size = 10;  // Header size.
  std::string handle_section_bytes;
  std::string parameter_section_bytes;
  TPM_CC command_code = TPM_CC_Sign;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string key_handle_bytes;
  rc = Serialize_TPMI_DH_OBJECT(
      key_handle,
      &key_handle_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string digest_bytes;
  rc = Serialize_TPM2B_DIGEST(
      digest,
      &digest_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string in_scheme_bytes;
  rc = Serialize_TPMT_SIG_SCHEME(
      in_scheme,
      &in_scheme_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string validation_bytes;
  rc = Serialize_TPMT_TK_HASHCHECK(
      validation,
      &validation_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  // Encrypt just the parameter data, not the size.
  std::string tmp = digest_bytes.substr(2);
  if (!authorization_delegate->EncryptCommandParameter(&tmp)) {
    error_reporter.Run(TRUNKS_RC_ENCRYPTION_FAILED);
    return;
  }
  digest_bytes.replace(2, std::string::npos, tmp);
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(key_handle_name.data(),
               key_handle_name.size());
  handle_section_bytes += key_handle_bytes;
  command_size += key_handle_bytes.size();
  hash->Update(digest_bytes.data(),
               digest_bytes.size());
  parameter_section_bytes += digest_bytes;
  command_size += digest_bytes.size();
  hash->Update(in_scheme_bytes.data(),
               in_scheme_bytes.size());
  parameter_section_bytes += in_scheme_bytes;
  command_size += in_scheme_bytes.size();
  hash->Update(validation_bytes.data(),
               validation_bytes.size());
  parameter_section_bytes += validation_bytes;
  command_size += validation_bytes.size();
  std::string command_hash(32, 0);
  hash->Finish(string_as_array(&command_hash), command_hash.size());
  std::string authorization_section_bytes;
  if (!authorization_delegate->GetCommandAuthorization(
      command_hash,
      &authorization_section_bytes)) {
    error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
    return;
  }
  std::string authorization_size_bytes;
  if (!authorization_section_bytes.empty()) {
    tag = TPM_ST_SESSIONS;
    std::string tmp;
    rc = Serialize_UINT32(authorization_section_bytes.size(),
                          &authorization_size_bytes);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    command_size += authorization_size_bytes.size() +
                    authorization_section_bytes.size();
  }
  std::string tag_bytes;
  rc = Serialize_TPMI_ST_COMMAND_TAG(
      tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command_size_bytes;
  rc = Serialize_UINT32(
      command_size,
      &command_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command = tag_bytes +
                        command_size_bytes +
                        command_code_bytes +
                        handle_section_bytes +
                        authorization_size_bytes +
                        authorization_section_bytes +
                        parameter_section_bytes;
  CHECK(command.size() == command_size) << "Command size mismatch!";
  transceiver_->SendCommand(command, parser);
}

void SetCommandCodeAuditStatusErrorCallback(
    const Tpm::SetCommandCodeAuditStatusResponse& callback,
    TPM_RC response_code) {
  callback.Run(response_code);
}

void SetCommandCodeAuditStatusResponseParser(
    const Tpm::SetCommandCodeAuditStatusResponse& callback,
    AuthorizationDelegate* authorization_delegate,
    const std::string& response) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(SetCommandCodeAuditStatusErrorCallback, callback);
  std::string buffer(response);
  TPM_ST tag;
  std::string tag_bytes;
  rc = Parse_TPM_ST(
      &buffer,
      &tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  UINT32 response_size;
  std::string response_size_bytes;
  rc = Parse_UINT32(
      &buffer,
      &response_size,
      &response_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM_RC response_code;
  std::string response_code_bytes;
  rc = Parse_TPM_RC(
      &buffer,
      &response_code,
      &response_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (response_size != response.size()) {
    error_reporter.Run(TPM_RC_SIZE);
    return;
  }
  if (response_code != TPM_RC_SUCCESS) {
    error_reporter.Run(response_code);
    return;
  }
  TPM_CC command_code = TPM_CC_SetCommandCodeAuditStatus;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string authorization_section_bytes;
  if (tag == TPM_ST_SESSIONS) {
    UINT32 parameter_section_size = buffer.size();
    rc = Parse_UINT32(&buffer, &parameter_section_size, NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    if (parameter_section_size > buffer.size()) {
      error_reporter.Run(TPM_RC_INSUFFICIENT);
      return;
    }
    authorization_section_bytes = buffer.substr(parameter_section_size);
    // Keep the parameter section in |buffer|.
    buffer.erase(parameter_section_size);
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(response_code_bytes.data(),
               response_code_bytes.size());
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(buffer.data(),
               buffer.size());
  std::string response_hash(32, 0);
  hash->Finish(string_as_array(&response_hash), response_hash.size());
  if (tag == TPM_ST_SESSIONS) {
    if (!authorization_delegate->CheckResponseAuthorization(
        response_hash,
        authorization_section_bytes)) {
      error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
      return;
    }
  }
  callback.Run(response_code);
}

void Tpm::SetCommandCodeAuditStatus(
      TPMI_RH_PROVISION auth,
      const std::string& auth_name,
      TPMI_ALG_HASH audit_alg,
      TPML_CC set_list,
      TPML_CC clear_list,
      AuthorizationDelegate* authorization_delegate,
      const SetCommandCodeAuditStatusResponse& callback) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(SetCommandCodeAuditStatusErrorCallback, callback);
  base::Callback<void(const std::string&)> parser =
      base::Bind(SetCommandCodeAuditStatusResponseParser,
                 callback,
                 authorization_delegate);
  TPMI_ST_COMMAND_TAG tag = TPM_ST_NO_SESSIONS;
  UINT32 command_size = 10;  // Header size.
  std::string handle_section_bytes;
  std::string parameter_section_bytes;
  TPM_CC command_code = TPM_CC_SetCommandCodeAuditStatus;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string auth_bytes;
  rc = Serialize_TPMI_RH_PROVISION(
      auth,
      &auth_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string audit_alg_bytes;
  rc = Serialize_TPMI_ALG_HASH(
      audit_alg,
      &audit_alg_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string set_list_bytes;
  rc = Serialize_TPML_CC(
      set_list,
      &set_list_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string clear_list_bytes;
  rc = Serialize_TPML_CC(
      clear_list,
      &clear_list_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(auth_name.data(),
               auth_name.size());
  handle_section_bytes += auth_bytes;
  command_size += auth_bytes.size();
  hash->Update(audit_alg_bytes.data(),
               audit_alg_bytes.size());
  parameter_section_bytes += audit_alg_bytes;
  command_size += audit_alg_bytes.size();
  hash->Update(set_list_bytes.data(),
               set_list_bytes.size());
  parameter_section_bytes += set_list_bytes;
  command_size += set_list_bytes.size();
  hash->Update(clear_list_bytes.data(),
               clear_list_bytes.size());
  parameter_section_bytes += clear_list_bytes;
  command_size += clear_list_bytes.size();
  std::string command_hash(32, 0);
  hash->Finish(string_as_array(&command_hash), command_hash.size());
  std::string authorization_section_bytes;
  if (!authorization_delegate->GetCommandAuthorization(
      command_hash,
      &authorization_section_bytes)) {
    error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
    return;
  }
  std::string authorization_size_bytes;
  if (!authorization_section_bytes.empty()) {
    tag = TPM_ST_SESSIONS;
    std::string tmp;
    rc = Serialize_UINT32(authorization_section_bytes.size(),
                          &authorization_size_bytes);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    command_size += authorization_size_bytes.size() +
                    authorization_section_bytes.size();
  }
  std::string tag_bytes;
  rc = Serialize_TPMI_ST_COMMAND_TAG(
      tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command_size_bytes;
  rc = Serialize_UINT32(
      command_size,
      &command_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command = tag_bytes +
                        command_size_bytes +
                        command_code_bytes +
                        handle_section_bytes +
                        authorization_size_bytes +
                        authorization_section_bytes +
                        parameter_section_bytes;
  CHECK(command.size() == command_size) << "Command size mismatch!";
  transceiver_->SendCommand(command, parser);
}

void PCR_ExtendErrorCallback(
    const Tpm::PCR_ExtendResponse& callback,
    TPM_RC response_code) {
  callback.Run(response_code);
}

void PCR_ExtendResponseParser(
    const Tpm::PCR_ExtendResponse& callback,
    AuthorizationDelegate* authorization_delegate,
    const std::string& response) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(PCR_ExtendErrorCallback, callback);
  std::string buffer(response);
  TPM_ST tag;
  std::string tag_bytes;
  rc = Parse_TPM_ST(
      &buffer,
      &tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  UINT32 response_size;
  std::string response_size_bytes;
  rc = Parse_UINT32(
      &buffer,
      &response_size,
      &response_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM_RC response_code;
  std::string response_code_bytes;
  rc = Parse_TPM_RC(
      &buffer,
      &response_code,
      &response_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (response_size != response.size()) {
    error_reporter.Run(TPM_RC_SIZE);
    return;
  }
  if (response_code != TPM_RC_SUCCESS) {
    error_reporter.Run(response_code);
    return;
  }
  TPM_CC command_code = TPM_CC_PCR_Extend;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string authorization_section_bytes;
  if (tag == TPM_ST_SESSIONS) {
    UINT32 parameter_section_size = buffer.size();
    rc = Parse_UINT32(&buffer, &parameter_section_size, NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    if (parameter_section_size > buffer.size()) {
      error_reporter.Run(TPM_RC_INSUFFICIENT);
      return;
    }
    authorization_section_bytes = buffer.substr(parameter_section_size);
    // Keep the parameter section in |buffer|.
    buffer.erase(parameter_section_size);
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(response_code_bytes.data(),
               response_code_bytes.size());
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(buffer.data(),
               buffer.size());
  std::string response_hash(32, 0);
  hash->Finish(string_as_array(&response_hash), response_hash.size());
  if (tag == TPM_ST_SESSIONS) {
    if (!authorization_delegate->CheckResponseAuthorization(
        response_hash,
        authorization_section_bytes)) {
      error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
      return;
    }
  }
  callback.Run(response_code);
}

void Tpm::PCR_Extend(
      TPMI_DH_PCR pcr_handle,
      const std::string& pcr_handle_name,
      TPML_DIGEST_VALUES digests,
      AuthorizationDelegate* authorization_delegate,
      const PCR_ExtendResponse& callback) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(PCR_ExtendErrorCallback, callback);
  base::Callback<void(const std::string&)> parser =
      base::Bind(PCR_ExtendResponseParser,
                 callback,
                 authorization_delegate);
  TPMI_ST_COMMAND_TAG tag = TPM_ST_NO_SESSIONS;
  UINT32 command_size = 10;  // Header size.
  std::string handle_section_bytes;
  std::string parameter_section_bytes;
  TPM_CC command_code = TPM_CC_PCR_Extend;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string pcr_handle_bytes;
  rc = Serialize_TPMI_DH_PCR(
      pcr_handle,
      &pcr_handle_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string digests_bytes;
  rc = Serialize_TPML_DIGEST_VALUES(
      digests,
      &digests_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(pcr_handle_name.data(),
               pcr_handle_name.size());
  handle_section_bytes += pcr_handle_bytes;
  command_size += pcr_handle_bytes.size();
  hash->Update(digests_bytes.data(),
               digests_bytes.size());
  parameter_section_bytes += digests_bytes;
  command_size += digests_bytes.size();
  std::string command_hash(32, 0);
  hash->Finish(string_as_array(&command_hash), command_hash.size());
  std::string authorization_section_bytes;
  if (!authorization_delegate->GetCommandAuthorization(
      command_hash,
      &authorization_section_bytes)) {
    error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
    return;
  }
  std::string authorization_size_bytes;
  if (!authorization_section_bytes.empty()) {
    tag = TPM_ST_SESSIONS;
    std::string tmp;
    rc = Serialize_UINT32(authorization_section_bytes.size(),
                          &authorization_size_bytes);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    command_size += authorization_size_bytes.size() +
                    authorization_section_bytes.size();
  }
  std::string tag_bytes;
  rc = Serialize_TPMI_ST_COMMAND_TAG(
      tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command_size_bytes;
  rc = Serialize_UINT32(
      command_size,
      &command_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command = tag_bytes +
                        command_size_bytes +
                        command_code_bytes +
                        handle_section_bytes +
                        authorization_size_bytes +
                        authorization_section_bytes +
                        parameter_section_bytes;
  CHECK(command.size() == command_size) << "Command size mismatch!";
  transceiver_->SendCommand(command, parser);
}

void PCR_EventErrorCallback(
    const Tpm::PCR_EventResponse& callback,
    TPM_RC response_code) {
  callback.Run(response_code,
               TPML_DIGEST_VALUES());
}

void PCR_EventResponseParser(
    const Tpm::PCR_EventResponse& callback,
    AuthorizationDelegate* authorization_delegate,
    const std::string& response) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(PCR_EventErrorCallback, callback);
  std::string buffer(response);
  TPM_ST tag;
  std::string tag_bytes;
  rc = Parse_TPM_ST(
      &buffer,
      &tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  UINT32 response_size;
  std::string response_size_bytes;
  rc = Parse_UINT32(
      &buffer,
      &response_size,
      &response_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM_RC response_code;
  std::string response_code_bytes;
  rc = Parse_TPM_RC(
      &buffer,
      &response_code,
      &response_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (response_size != response.size()) {
    error_reporter.Run(TPM_RC_SIZE);
    return;
  }
  if (response_code != TPM_RC_SUCCESS) {
    error_reporter.Run(response_code);
    return;
  }
  TPM_CC command_code = TPM_CC_PCR_Event;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string authorization_section_bytes;
  if (tag == TPM_ST_SESSIONS) {
    UINT32 parameter_section_size = buffer.size();
    rc = Parse_UINT32(&buffer, &parameter_section_size, NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    if (parameter_section_size > buffer.size()) {
      error_reporter.Run(TPM_RC_INSUFFICIENT);
      return;
    }
    authorization_section_bytes = buffer.substr(parameter_section_size);
    // Keep the parameter section in |buffer|.
    buffer.erase(parameter_section_size);
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(response_code_bytes.data(),
               response_code_bytes.size());
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(buffer.data(),
               buffer.size());
  std::string response_hash(32, 0);
  hash->Finish(string_as_array(&response_hash), response_hash.size());
  if (tag == TPM_ST_SESSIONS) {
    if (!authorization_delegate->CheckResponseAuthorization(
        response_hash,
        authorization_section_bytes)) {
      error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
      return;
    }
  }
  TPML_DIGEST_VALUES digests;
  std::string digests_bytes;
  rc = Parse_TPML_DIGEST_VALUES(
      &buffer,
      &digests,
      &digests_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (tag == TPM_ST_SESSIONS) {
    // Decrypt just the parameter data, not the size.
    std::string tmp = digests_bytes.substr(2);
    if (!authorization_delegate->DecryptResponseParameter(&tmp)) {
      error_reporter.Run(TRUNKS_RC_ENCRYPTION_FAILED);
      return;
    }
    digests_bytes.replace(2, std::string::npos, tmp);
    rc = Parse_TPML_DIGEST_VALUES(
        &digests_bytes,
        &digests,
        NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
  }
  callback.Run(response_code,
               digests);
}

void Tpm::PCR_Event(
      TPMI_DH_PCR pcr_handle,
      const std::string& pcr_handle_name,
      TPM2B_EVENT event_data,
      AuthorizationDelegate* authorization_delegate,
      const PCR_EventResponse& callback) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(PCR_EventErrorCallback, callback);
  base::Callback<void(const std::string&)> parser =
      base::Bind(PCR_EventResponseParser,
                 callback,
                 authorization_delegate);
  TPMI_ST_COMMAND_TAG tag = TPM_ST_NO_SESSIONS;
  UINT32 command_size = 10;  // Header size.
  std::string handle_section_bytes;
  std::string parameter_section_bytes;
  TPM_CC command_code = TPM_CC_PCR_Event;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string pcr_handle_bytes;
  rc = Serialize_TPMI_DH_PCR(
      pcr_handle,
      &pcr_handle_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string event_data_bytes;
  rc = Serialize_TPM2B_EVENT(
      event_data,
      &event_data_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  // Encrypt just the parameter data, not the size.
  std::string tmp = event_data_bytes.substr(2);
  if (!authorization_delegate->EncryptCommandParameter(&tmp)) {
    error_reporter.Run(TRUNKS_RC_ENCRYPTION_FAILED);
    return;
  }
  event_data_bytes.replace(2, std::string::npos, tmp);
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(pcr_handle_name.data(),
               pcr_handle_name.size());
  handle_section_bytes += pcr_handle_bytes;
  command_size += pcr_handle_bytes.size();
  hash->Update(event_data_bytes.data(),
               event_data_bytes.size());
  parameter_section_bytes += event_data_bytes;
  command_size += event_data_bytes.size();
  std::string command_hash(32, 0);
  hash->Finish(string_as_array(&command_hash), command_hash.size());
  std::string authorization_section_bytes;
  if (!authorization_delegate->GetCommandAuthorization(
      command_hash,
      &authorization_section_bytes)) {
    error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
    return;
  }
  std::string authorization_size_bytes;
  if (!authorization_section_bytes.empty()) {
    tag = TPM_ST_SESSIONS;
    std::string tmp;
    rc = Serialize_UINT32(authorization_section_bytes.size(),
                          &authorization_size_bytes);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    command_size += authorization_size_bytes.size() +
                    authorization_section_bytes.size();
  }
  std::string tag_bytes;
  rc = Serialize_TPMI_ST_COMMAND_TAG(
      tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command_size_bytes;
  rc = Serialize_UINT32(
      command_size,
      &command_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command = tag_bytes +
                        command_size_bytes +
                        command_code_bytes +
                        handle_section_bytes +
                        authorization_size_bytes +
                        authorization_section_bytes +
                        parameter_section_bytes;
  CHECK(command.size() == command_size) << "Command size mismatch!";
  transceiver_->SendCommand(command, parser);
}

void PCR_ReadErrorCallback(
    const Tpm::PCR_ReadResponse& callback,
    TPM_RC response_code) {
  callback.Run(response_code,
               UINT32(),
               TPML_PCR_SELECTION(),
               TPML_DIGEST());
}

void PCR_ReadResponseParser(
    const Tpm::PCR_ReadResponse& callback,
    AuthorizationDelegate* authorization_delegate,
    const std::string& response) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(PCR_ReadErrorCallback, callback);
  std::string buffer(response);
  TPM_ST tag;
  std::string tag_bytes;
  rc = Parse_TPM_ST(
      &buffer,
      &tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  UINT32 response_size;
  std::string response_size_bytes;
  rc = Parse_UINT32(
      &buffer,
      &response_size,
      &response_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM_RC response_code;
  std::string response_code_bytes;
  rc = Parse_TPM_RC(
      &buffer,
      &response_code,
      &response_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (response_size != response.size()) {
    error_reporter.Run(TPM_RC_SIZE);
    return;
  }
  if (response_code != TPM_RC_SUCCESS) {
    error_reporter.Run(response_code);
    return;
  }
  TPM_CC command_code = TPM_CC_PCR_Read;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string authorization_section_bytes;
  if (tag == TPM_ST_SESSIONS) {
    UINT32 parameter_section_size = buffer.size();
    rc = Parse_UINT32(&buffer, &parameter_section_size, NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    if (parameter_section_size > buffer.size()) {
      error_reporter.Run(TPM_RC_INSUFFICIENT);
      return;
    }
    authorization_section_bytes = buffer.substr(parameter_section_size);
    // Keep the parameter section in |buffer|.
    buffer.erase(parameter_section_size);
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(response_code_bytes.data(),
               response_code_bytes.size());
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(buffer.data(),
               buffer.size());
  std::string response_hash(32, 0);
  hash->Finish(string_as_array(&response_hash), response_hash.size());
  if (tag == TPM_ST_SESSIONS) {
    if (!authorization_delegate->CheckResponseAuthorization(
        response_hash,
        authorization_section_bytes)) {
      error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
      return;
    }
  }
  UINT32 pcr_update_counter;
  std::string pcr_update_counter_bytes;
  rc = Parse_UINT32(
      &buffer,
      &pcr_update_counter,
      &pcr_update_counter_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPML_PCR_SELECTION pcr_selection_out;
  std::string pcr_selection_out_bytes;
  rc = Parse_TPML_PCR_SELECTION(
      &buffer,
      &pcr_selection_out,
      &pcr_selection_out_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPML_DIGEST pcr_values;
  std::string pcr_values_bytes;
  rc = Parse_TPML_DIGEST(
      &buffer,
      &pcr_values,
      &pcr_values_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (tag == TPM_ST_SESSIONS) {
    // Decrypt just the parameter data, not the size.
    std::string tmp = pcr_update_counter_bytes.substr(2);
    if (!authorization_delegate->DecryptResponseParameter(&tmp)) {
      error_reporter.Run(TRUNKS_RC_ENCRYPTION_FAILED);
      return;
    }
    pcr_update_counter_bytes.replace(2, std::string::npos, tmp);
    rc = Parse_UINT32(
        &pcr_update_counter_bytes,
        &pcr_update_counter,
        NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
  }
  callback.Run(response_code,
               pcr_update_counter,
               pcr_selection_out,
               pcr_values);
}

void Tpm::PCR_Read(
      TPML_PCR_SELECTION pcr_selection_in,
      AuthorizationDelegate* authorization_delegate,
      const PCR_ReadResponse& callback) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(PCR_ReadErrorCallback, callback);
  base::Callback<void(const std::string&)> parser =
      base::Bind(PCR_ReadResponseParser,
                 callback,
                 authorization_delegate);
  TPMI_ST_COMMAND_TAG tag = TPM_ST_NO_SESSIONS;
  UINT32 command_size = 10;  // Header size.
  std::string handle_section_bytes;
  std::string parameter_section_bytes;
  TPM_CC command_code = TPM_CC_PCR_Read;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string pcr_selection_in_bytes;
  rc = Serialize_TPML_PCR_SELECTION(
      pcr_selection_in,
      &pcr_selection_in_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(pcr_selection_in_bytes.data(),
               pcr_selection_in_bytes.size());
  parameter_section_bytes += pcr_selection_in_bytes;
  command_size += pcr_selection_in_bytes.size();
  std::string command_hash(32, 0);
  hash->Finish(string_as_array(&command_hash), command_hash.size());
  std::string authorization_section_bytes;
  if (!authorization_delegate->GetCommandAuthorization(
      command_hash,
      &authorization_section_bytes)) {
    error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
    return;
  }
  std::string authorization_size_bytes;
  if (!authorization_section_bytes.empty()) {
    tag = TPM_ST_SESSIONS;
    std::string tmp;
    rc = Serialize_UINT32(authorization_section_bytes.size(),
                          &authorization_size_bytes);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    command_size += authorization_size_bytes.size() +
                    authorization_section_bytes.size();
  }
  std::string tag_bytes;
  rc = Serialize_TPMI_ST_COMMAND_TAG(
      tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command_size_bytes;
  rc = Serialize_UINT32(
      command_size,
      &command_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command = tag_bytes +
                        command_size_bytes +
                        command_code_bytes +
                        handle_section_bytes +
                        authorization_size_bytes +
                        authorization_section_bytes +
                        parameter_section_bytes;
  CHECK(command.size() == command_size) << "Command size mismatch!";
  transceiver_->SendCommand(command, parser);
}

void PCR_AllocateErrorCallback(
    const Tpm::PCR_AllocateResponse& callback,
    TPM_RC response_code) {
  callback.Run(response_code,
               TPMI_YES_NO(),
               UINT32(),
               UINT32(),
               UINT32());
}

void PCR_AllocateResponseParser(
    const Tpm::PCR_AllocateResponse& callback,
    AuthorizationDelegate* authorization_delegate,
    const std::string& response) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(PCR_AllocateErrorCallback, callback);
  std::string buffer(response);
  TPM_ST tag;
  std::string tag_bytes;
  rc = Parse_TPM_ST(
      &buffer,
      &tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  UINT32 response_size;
  std::string response_size_bytes;
  rc = Parse_UINT32(
      &buffer,
      &response_size,
      &response_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM_RC response_code;
  std::string response_code_bytes;
  rc = Parse_TPM_RC(
      &buffer,
      &response_code,
      &response_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (response_size != response.size()) {
    error_reporter.Run(TPM_RC_SIZE);
    return;
  }
  if (response_code != TPM_RC_SUCCESS) {
    error_reporter.Run(response_code);
    return;
  }
  TPM_CC command_code = TPM_CC_PCR_Allocate;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string authorization_section_bytes;
  if (tag == TPM_ST_SESSIONS) {
    UINT32 parameter_section_size = buffer.size();
    rc = Parse_UINT32(&buffer, &parameter_section_size, NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    if (parameter_section_size > buffer.size()) {
      error_reporter.Run(TPM_RC_INSUFFICIENT);
      return;
    }
    authorization_section_bytes = buffer.substr(parameter_section_size);
    // Keep the parameter section in |buffer|.
    buffer.erase(parameter_section_size);
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(response_code_bytes.data(),
               response_code_bytes.size());
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(buffer.data(),
               buffer.size());
  std::string response_hash(32, 0);
  hash->Finish(string_as_array(&response_hash), response_hash.size());
  if (tag == TPM_ST_SESSIONS) {
    if (!authorization_delegate->CheckResponseAuthorization(
        response_hash,
        authorization_section_bytes)) {
      error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
      return;
    }
  }
  TPMI_YES_NO allocation_success;
  std::string allocation_success_bytes;
  rc = Parse_TPMI_YES_NO(
      &buffer,
      &allocation_success,
      &allocation_success_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  UINT32 max_pcr;
  std::string max_pcr_bytes;
  rc = Parse_UINT32(
      &buffer,
      &max_pcr,
      &max_pcr_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  UINT32 size_needed;
  std::string size_needed_bytes;
  rc = Parse_UINT32(
      &buffer,
      &size_needed,
      &size_needed_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  UINT32 size_available;
  std::string size_available_bytes;
  rc = Parse_UINT32(
      &buffer,
      &size_available,
      &size_available_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (tag == TPM_ST_SESSIONS) {
    // Decrypt just the parameter data, not the size.
    std::string tmp = allocation_success_bytes.substr(2);
    if (!authorization_delegate->DecryptResponseParameter(&tmp)) {
      error_reporter.Run(TRUNKS_RC_ENCRYPTION_FAILED);
      return;
    }
    allocation_success_bytes.replace(2, std::string::npos, tmp);
    rc = Parse_TPMI_YES_NO(
        &allocation_success_bytes,
        &allocation_success,
        NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
  }
  callback.Run(response_code,
               allocation_success,
               max_pcr,
               size_needed,
               size_available);
}

void Tpm::PCR_Allocate(
      TPMI_RH_PLATFORM auth_handle,
      const std::string& auth_handle_name,
      TPML_PCR_SELECTION pcr_allocation,
      AuthorizationDelegate* authorization_delegate,
      const PCR_AllocateResponse& callback) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(PCR_AllocateErrorCallback, callback);
  base::Callback<void(const std::string&)> parser =
      base::Bind(PCR_AllocateResponseParser,
                 callback,
                 authorization_delegate);
  TPMI_ST_COMMAND_TAG tag = TPM_ST_NO_SESSIONS;
  UINT32 command_size = 10;  // Header size.
  std::string handle_section_bytes;
  std::string parameter_section_bytes;
  TPM_CC command_code = TPM_CC_PCR_Allocate;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string auth_handle_bytes;
  rc = Serialize_TPMI_RH_PLATFORM(
      auth_handle,
      &auth_handle_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string pcr_allocation_bytes;
  rc = Serialize_TPML_PCR_SELECTION(
      pcr_allocation,
      &pcr_allocation_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(auth_handle_name.data(),
               auth_handle_name.size());
  handle_section_bytes += auth_handle_bytes;
  command_size += auth_handle_bytes.size();
  hash->Update(pcr_allocation_bytes.data(),
               pcr_allocation_bytes.size());
  parameter_section_bytes += pcr_allocation_bytes;
  command_size += pcr_allocation_bytes.size();
  std::string command_hash(32, 0);
  hash->Finish(string_as_array(&command_hash), command_hash.size());
  std::string authorization_section_bytes;
  if (!authorization_delegate->GetCommandAuthorization(
      command_hash,
      &authorization_section_bytes)) {
    error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
    return;
  }
  std::string authorization_size_bytes;
  if (!authorization_section_bytes.empty()) {
    tag = TPM_ST_SESSIONS;
    std::string tmp;
    rc = Serialize_UINT32(authorization_section_bytes.size(),
                          &authorization_size_bytes);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    command_size += authorization_size_bytes.size() +
                    authorization_section_bytes.size();
  }
  std::string tag_bytes;
  rc = Serialize_TPMI_ST_COMMAND_TAG(
      tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command_size_bytes;
  rc = Serialize_UINT32(
      command_size,
      &command_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command = tag_bytes +
                        command_size_bytes +
                        command_code_bytes +
                        handle_section_bytes +
                        authorization_size_bytes +
                        authorization_section_bytes +
                        parameter_section_bytes;
  CHECK(command.size() == command_size) << "Command size mismatch!";
  transceiver_->SendCommand(command, parser);
}

void PCR_SetAuthPolicyErrorCallback(
    const Tpm::PCR_SetAuthPolicyResponse& callback,
    TPM_RC response_code) {
  callback.Run(response_code);
}

void PCR_SetAuthPolicyResponseParser(
    const Tpm::PCR_SetAuthPolicyResponse& callback,
    AuthorizationDelegate* authorization_delegate,
    const std::string& response) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(PCR_SetAuthPolicyErrorCallback, callback);
  std::string buffer(response);
  TPM_ST tag;
  std::string tag_bytes;
  rc = Parse_TPM_ST(
      &buffer,
      &tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  UINT32 response_size;
  std::string response_size_bytes;
  rc = Parse_UINT32(
      &buffer,
      &response_size,
      &response_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM_RC response_code;
  std::string response_code_bytes;
  rc = Parse_TPM_RC(
      &buffer,
      &response_code,
      &response_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (response_size != response.size()) {
    error_reporter.Run(TPM_RC_SIZE);
    return;
  }
  if (response_code != TPM_RC_SUCCESS) {
    error_reporter.Run(response_code);
    return;
  }
  TPM_CC command_code = TPM_CC_PCR_SetAuthPolicy;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string authorization_section_bytes;
  if (tag == TPM_ST_SESSIONS) {
    UINT32 parameter_section_size = buffer.size();
    rc = Parse_UINT32(&buffer, &parameter_section_size, NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    if (parameter_section_size > buffer.size()) {
      error_reporter.Run(TPM_RC_INSUFFICIENT);
      return;
    }
    authorization_section_bytes = buffer.substr(parameter_section_size);
    // Keep the parameter section in |buffer|.
    buffer.erase(parameter_section_size);
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(response_code_bytes.data(),
               response_code_bytes.size());
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(buffer.data(),
               buffer.size());
  std::string response_hash(32, 0);
  hash->Finish(string_as_array(&response_hash), response_hash.size());
  if (tag == TPM_ST_SESSIONS) {
    if (!authorization_delegate->CheckResponseAuthorization(
        response_hash,
        authorization_section_bytes)) {
      error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
      return;
    }
  }
  callback.Run(response_code);
}

void Tpm::PCR_SetAuthPolicy(
      TPMI_RH_PLATFORM auth_handle,
      const std::string& auth_handle_name,
      TPMI_DH_PCR pcr_num,
      const std::string& pcr_num_name,
      TPM2B_DIGEST auth_policy,
      TPMI_ALG_HASH policy_digest,
      AuthorizationDelegate* authorization_delegate,
      const PCR_SetAuthPolicyResponse& callback) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(PCR_SetAuthPolicyErrorCallback, callback);
  base::Callback<void(const std::string&)> parser =
      base::Bind(PCR_SetAuthPolicyResponseParser,
                 callback,
                 authorization_delegate);
  TPMI_ST_COMMAND_TAG tag = TPM_ST_NO_SESSIONS;
  UINT32 command_size = 10;  // Header size.
  std::string handle_section_bytes;
  std::string parameter_section_bytes;
  TPM_CC command_code = TPM_CC_PCR_SetAuthPolicy;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string auth_handle_bytes;
  rc = Serialize_TPMI_RH_PLATFORM(
      auth_handle,
      &auth_handle_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string auth_policy_bytes;
  rc = Serialize_TPM2B_DIGEST(
      auth_policy,
      &auth_policy_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string policy_digest_bytes;
  rc = Serialize_TPMI_ALG_HASH(
      policy_digest,
      &policy_digest_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string pcr_num_bytes;
  rc = Serialize_TPMI_DH_PCR(
      pcr_num,
      &pcr_num_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  // Encrypt just the parameter data, not the size.
  std::string tmp = auth_policy_bytes.substr(2);
  if (!authorization_delegate->EncryptCommandParameter(&tmp)) {
    error_reporter.Run(TRUNKS_RC_ENCRYPTION_FAILED);
    return;
  }
  auth_policy_bytes.replace(2, std::string::npos, tmp);
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(auth_handle_name.data(),
               auth_handle_name.size());
  handle_section_bytes += auth_handle_bytes;
  command_size += auth_handle_bytes.size();
  hash->Update(pcr_num_name.data(),
               pcr_num_name.size());
  handle_section_bytes += pcr_num_bytes;
  command_size += pcr_num_bytes.size();
  hash->Update(auth_policy_bytes.data(),
               auth_policy_bytes.size());
  parameter_section_bytes += auth_policy_bytes;
  command_size += auth_policy_bytes.size();
  hash->Update(policy_digest_bytes.data(),
               policy_digest_bytes.size());
  parameter_section_bytes += policy_digest_bytes;
  command_size += policy_digest_bytes.size();
  std::string command_hash(32, 0);
  hash->Finish(string_as_array(&command_hash), command_hash.size());
  std::string authorization_section_bytes;
  if (!authorization_delegate->GetCommandAuthorization(
      command_hash,
      &authorization_section_bytes)) {
    error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
    return;
  }
  std::string authorization_size_bytes;
  if (!authorization_section_bytes.empty()) {
    tag = TPM_ST_SESSIONS;
    std::string tmp;
    rc = Serialize_UINT32(authorization_section_bytes.size(),
                          &authorization_size_bytes);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    command_size += authorization_size_bytes.size() +
                    authorization_section_bytes.size();
  }
  std::string tag_bytes;
  rc = Serialize_TPMI_ST_COMMAND_TAG(
      tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command_size_bytes;
  rc = Serialize_UINT32(
      command_size,
      &command_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command = tag_bytes +
                        command_size_bytes +
                        command_code_bytes +
                        handle_section_bytes +
                        authorization_size_bytes +
                        authorization_section_bytes +
                        parameter_section_bytes;
  CHECK(command.size() == command_size) << "Command size mismatch!";
  transceiver_->SendCommand(command, parser);
}

void PCR_SetAuthValueErrorCallback(
    const Tpm::PCR_SetAuthValueResponse& callback,
    TPM_RC response_code) {
  callback.Run(response_code);
}

void PCR_SetAuthValueResponseParser(
    const Tpm::PCR_SetAuthValueResponse& callback,
    AuthorizationDelegate* authorization_delegate,
    const std::string& response) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(PCR_SetAuthValueErrorCallback, callback);
  std::string buffer(response);
  TPM_ST tag;
  std::string tag_bytes;
  rc = Parse_TPM_ST(
      &buffer,
      &tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  UINT32 response_size;
  std::string response_size_bytes;
  rc = Parse_UINT32(
      &buffer,
      &response_size,
      &response_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM_RC response_code;
  std::string response_code_bytes;
  rc = Parse_TPM_RC(
      &buffer,
      &response_code,
      &response_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (response_size != response.size()) {
    error_reporter.Run(TPM_RC_SIZE);
    return;
  }
  if (response_code != TPM_RC_SUCCESS) {
    error_reporter.Run(response_code);
    return;
  }
  TPM_CC command_code = TPM_CC_PCR_SetAuthValue;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string authorization_section_bytes;
  if (tag == TPM_ST_SESSIONS) {
    UINT32 parameter_section_size = buffer.size();
    rc = Parse_UINT32(&buffer, &parameter_section_size, NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    if (parameter_section_size > buffer.size()) {
      error_reporter.Run(TPM_RC_INSUFFICIENT);
      return;
    }
    authorization_section_bytes = buffer.substr(parameter_section_size);
    // Keep the parameter section in |buffer|.
    buffer.erase(parameter_section_size);
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(response_code_bytes.data(),
               response_code_bytes.size());
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(buffer.data(),
               buffer.size());
  std::string response_hash(32, 0);
  hash->Finish(string_as_array(&response_hash), response_hash.size());
  if (tag == TPM_ST_SESSIONS) {
    if (!authorization_delegate->CheckResponseAuthorization(
        response_hash,
        authorization_section_bytes)) {
      error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
      return;
    }
  }
  callback.Run(response_code);
}

void Tpm::PCR_SetAuthValue(
      TPMI_DH_PCR pcr_handle,
      const std::string& pcr_handle_name,
      TPM2B_DIGEST auth,
      AuthorizationDelegate* authorization_delegate,
      const PCR_SetAuthValueResponse& callback) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(PCR_SetAuthValueErrorCallback, callback);
  base::Callback<void(const std::string&)> parser =
      base::Bind(PCR_SetAuthValueResponseParser,
                 callback,
                 authorization_delegate);
  TPMI_ST_COMMAND_TAG tag = TPM_ST_NO_SESSIONS;
  UINT32 command_size = 10;  // Header size.
  std::string handle_section_bytes;
  std::string parameter_section_bytes;
  TPM_CC command_code = TPM_CC_PCR_SetAuthValue;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string pcr_handle_bytes;
  rc = Serialize_TPMI_DH_PCR(
      pcr_handle,
      &pcr_handle_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string auth_bytes;
  rc = Serialize_TPM2B_DIGEST(
      auth,
      &auth_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  // Encrypt just the parameter data, not the size.
  std::string tmp = auth_bytes.substr(2);
  if (!authorization_delegate->EncryptCommandParameter(&tmp)) {
    error_reporter.Run(TRUNKS_RC_ENCRYPTION_FAILED);
    return;
  }
  auth_bytes.replace(2, std::string::npos, tmp);
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(pcr_handle_name.data(),
               pcr_handle_name.size());
  handle_section_bytes += pcr_handle_bytes;
  command_size += pcr_handle_bytes.size();
  hash->Update(auth_bytes.data(),
               auth_bytes.size());
  parameter_section_bytes += auth_bytes;
  command_size += auth_bytes.size();
  std::string command_hash(32, 0);
  hash->Finish(string_as_array(&command_hash), command_hash.size());
  std::string authorization_section_bytes;
  if (!authorization_delegate->GetCommandAuthorization(
      command_hash,
      &authorization_section_bytes)) {
    error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
    return;
  }
  std::string authorization_size_bytes;
  if (!authorization_section_bytes.empty()) {
    tag = TPM_ST_SESSIONS;
    std::string tmp;
    rc = Serialize_UINT32(authorization_section_bytes.size(),
                          &authorization_size_bytes);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    command_size += authorization_size_bytes.size() +
                    authorization_section_bytes.size();
  }
  std::string tag_bytes;
  rc = Serialize_TPMI_ST_COMMAND_TAG(
      tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command_size_bytes;
  rc = Serialize_UINT32(
      command_size,
      &command_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command = tag_bytes +
                        command_size_bytes +
                        command_code_bytes +
                        handle_section_bytes +
                        authorization_size_bytes +
                        authorization_section_bytes +
                        parameter_section_bytes;
  CHECK(command.size() == command_size) << "Command size mismatch!";
  transceiver_->SendCommand(command, parser);
}

void PCR_ResetErrorCallback(
    const Tpm::PCR_ResetResponse& callback,
    TPM_RC response_code) {
  callback.Run(response_code);
}

void PCR_ResetResponseParser(
    const Tpm::PCR_ResetResponse& callback,
    AuthorizationDelegate* authorization_delegate,
    const std::string& response) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(PCR_ResetErrorCallback, callback);
  std::string buffer(response);
  TPM_ST tag;
  std::string tag_bytes;
  rc = Parse_TPM_ST(
      &buffer,
      &tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  UINT32 response_size;
  std::string response_size_bytes;
  rc = Parse_UINT32(
      &buffer,
      &response_size,
      &response_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM_RC response_code;
  std::string response_code_bytes;
  rc = Parse_TPM_RC(
      &buffer,
      &response_code,
      &response_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (response_size != response.size()) {
    error_reporter.Run(TPM_RC_SIZE);
    return;
  }
  if (response_code != TPM_RC_SUCCESS) {
    error_reporter.Run(response_code);
    return;
  }
  TPM_CC command_code = TPM_CC_PCR_Reset;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string authorization_section_bytes;
  if (tag == TPM_ST_SESSIONS) {
    UINT32 parameter_section_size = buffer.size();
    rc = Parse_UINT32(&buffer, &parameter_section_size, NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    if (parameter_section_size > buffer.size()) {
      error_reporter.Run(TPM_RC_INSUFFICIENT);
      return;
    }
    authorization_section_bytes = buffer.substr(parameter_section_size);
    // Keep the parameter section in |buffer|.
    buffer.erase(parameter_section_size);
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(response_code_bytes.data(),
               response_code_bytes.size());
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(buffer.data(),
               buffer.size());
  std::string response_hash(32, 0);
  hash->Finish(string_as_array(&response_hash), response_hash.size());
  if (tag == TPM_ST_SESSIONS) {
    if (!authorization_delegate->CheckResponseAuthorization(
        response_hash,
        authorization_section_bytes)) {
      error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
      return;
    }
  }
  callback.Run(response_code);
}

void Tpm::PCR_Reset(
      TPMI_DH_PCR pcr_handle,
      const std::string& pcr_handle_name,
      AuthorizationDelegate* authorization_delegate,
      const PCR_ResetResponse& callback) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(PCR_ResetErrorCallback, callback);
  base::Callback<void(const std::string&)> parser =
      base::Bind(PCR_ResetResponseParser,
                 callback,
                 authorization_delegate);
  TPMI_ST_COMMAND_TAG tag = TPM_ST_NO_SESSIONS;
  UINT32 command_size = 10;  // Header size.
  std::string handle_section_bytes;
  std::string parameter_section_bytes;
  TPM_CC command_code = TPM_CC_PCR_Reset;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string pcr_handle_bytes;
  rc = Serialize_TPMI_DH_PCR(
      pcr_handle,
      &pcr_handle_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(pcr_handle_name.data(),
               pcr_handle_name.size());
  handle_section_bytes += pcr_handle_bytes;
  command_size += pcr_handle_bytes.size();
  std::string command_hash(32, 0);
  hash->Finish(string_as_array(&command_hash), command_hash.size());
  std::string authorization_section_bytes;
  if (!authorization_delegate->GetCommandAuthorization(
      command_hash,
      &authorization_section_bytes)) {
    error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
    return;
  }
  std::string authorization_size_bytes;
  if (!authorization_section_bytes.empty()) {
    tag = TPM_ST_SESSIONS;
    std::string tmp;
    rc = Serialize_UINT32(authorization_section_bytes.size(),
                          &authorization_size_bytes);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    command_size += authorization_size_bytes.size() +
                    authorization_section_bytes.size();
  }
  std::string tag_bytes;
  rc = Serialize_TPMI_ST_COMMAND_TAG(
      tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command_size_bytes;
  rc = Serialize_UINT32(
      command_size,
      &command_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command = tag_bytes +
                        command_size_bytes +
                        command_code_bytes +
                        handle_section_bytes +
                        authorization_size_bytes +
                        authorization_section_bytes +
                        parameter_section_bytes;
  CHECK(command.size() == command_size) << "Command size mismatch!";
  transceiver_->SendCommand(command, parser);
}

void PolicySignedErrorCallback(
    const Tpm::PolicySignedResponse& callback,
    TPM_RC response_code) {
  callback.Run(response_code,
               TPM2B_TIMEOUT(),
               TPMT_TK_AUTH());
}

void PolicySignedResponseParser(
    const Tpm::PolicySignedResponse& callback,
    AuthorizationDelegate* authorization_delegate,
    const std::string& response) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(PolicySignedErrorCallback, callback);
  std::string buffer(response);
  TPM_ST tag;
  std::string tag_bytes;
  rc = Parse_TPM_ST(
      &buffer,
      &tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  UINT32 response_size;
  std::string response_size_bytes;
  rc = Parse_UINT32(
      &buffer,
      &response_size,
      &response_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM_RC response_code;
  std::string response_code_bytes;
  rc = Parse_TPM_RC(
      &buffer,
      &response_code,
      &response_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (response_size != response.size()) {
    error_reporter.Run(TPM_RC_SIZE);
    return;
  }
  if (response_code != TPM_RC_SUCCESS) {
    error_reporter.Run(response_code);
    return;
  }
  TPM_CC command_code = TPM_CC_PolicySigned;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string authorization_section_bytes;
  if (tag == TPM_ST_SESSIONS) {
    UINT32 parameter_section_size = buffer.size();
    rc = Parse_UINT32(&buffer, &parameter_section_size, NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    if (parameter_section_size > buffer.size()) {
      error_reporter.Run(TPM_RC_INSUFFICIENT);
      return;
    }
    authorization_section_bytes = buffer.substr(parameter_section_size);
    // Keep the parameter section in |buffer|.
    buffer.erase(parameter_section_size);
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(response_code_bytes.data(),
               response_code_bytes.size());
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(buffer.data(),
               buffer.size());
  std::string response_hash(32, 0);
  hash->Finish(string_as_array(&response_hash), response_hash.size());
  if (tag == TPM_ST_SESSIONS) {
    if (!authorization_delegate->CheckResponseAuthorization(
        response_hash,
        authorization_section_bytes)) {
      error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
      return;
    }
  }
  TPM2B_TIMEOUT timeout;
  std::string timeout_bytes;
  rc = Parse_TPM2B_TIMEOUT(
      &buffer,
      &timeout,
      &timeout_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPMT_TK_AUTH policy_ticket;
  std::string policy_ticket_bytes;
  rc = Parse_TPMT_TK_AUTH(
      &buffer,
      &policy_ticket,
      &policy_ticket_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (tag == TPM_ST_SESSIONS) {
    // Decrypt just the parameter data, not the size.
    std::string tmp = timeout_bytes.substr(2);
    if (!authorization_delegate->DecryptResponseParameter(&tmp)) {
      error_reporter.Run(TRUNKS_RC_ENCRYPTION_FAILED);
      return;
    }
    timeout_bytes.replace(2, std::string::npos, tmp);
    rc = Parse_TPM2B_TIMEOUT(
        &timeout_bytes,
        &timeout,
        NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
  }
  callback.Run(response_code,
               timeout,
               policy_ticket);
}

void Tpm::PolicySigned(
      TPMI_DH_OBJECT auth_object,
      const std::string& auth_object_name,
      TPMI_SH_POLICY policy_session,
      const std::string& policy_session_name,
      TPM2B_NONCE nonce_tpm,
      TPM2B_DIGEST cp_hash_a,
      TPM2B_NONCE policy_ref,
      TPMT_SIGNATURE auth,
      AuthorizationDelegate* authorization_delegate,
      const PolicySignedResponse& callback) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(PolicySignedErrorCallback, callback);
  base::Callback<void(const std::string&)> parser =
      base::Bind(PolicySignedResponseParser,
                 callback,
                 authorization_delegate);
  TPMI_ST_COMMAND_TAG tag = TPM_ST_NO_SESSIONS;
  UINT32 command_size = 10;  // Header size.
  std::string handle_section_bytes;
  std::string parameter_section_bytes;
  TPM_CC command_code = TPM_CC_PolicySigned;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string auth_object_bytes;
  rc = Serialize_TPMI_DH_OBJECT(
      auth_object,
      &auth_object_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string policy_session_bytes;
  rc = Serialize_TPMI_SH_POLICY(
      policy_session,
      &policy_session_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string nonce_tpm_bytes;
  rc = Serialize_TPM2B_NONCE(
      nonce_tpm,
      &nonce_tpm_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string cp_hash_a_bytes;
  rc = Serialize_TPM2B_DIGEST(
      cp_hash_a,
      &cp_hash_a_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string policy_ref_bytes;
  rc = Serialize_TPM2B_NONCE(
      policy_ref,
      &policy_ref_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string auth_bytes;
  rc = Serialize_TPMT_SIGNATURE(
      auth,
      &auth_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  // Encrypt just the parameter data, not the size.
  std::string tmp = nonce_tpm_bytes.substr(2);
  if (!authorization_delegate->EncryptCommandParameter(&tmp)) {
    error_reporter.Run(TRUNKS_RC_ENCRYPTION_FAILED);
    return;
  }
  nonce_tpm_bytes.replace(2, std::string::npos, tmp);
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(auth_object_name.data(),
               auth_object_name.size());
  handle_section_bytes += auth_object_bytes;
  command_size += auth_object_bytes.size();
  hash->Update(policy_session_name.data(),
               policy_session_name.size());
  handle_section_bytes += policy_session_bytes;
  command_size += policy_session_bytes.size();
  hash->Update(nonce_tpm_bytes.data(),
               nonce_tpm_bytes.size());
  parameter_section_bytes += nonce_tpm_bytes;
  command_size += nonce_tpm_bytes.size();
  hash->Update(cp_hash_a_bytes.data(),
               cp_hash_a_bytes.size());
  parameter_section_bytes += cp_hash_a_bytes;
  command_size += cp_hash_a_bytes.size();
  hash->Update(policy_ref_bytes.data(),
               policy_ref_bytes.size());
  parameter_section_bytes += policy_ref_bytes;
  command_size += policy_ref_bytes.size();
  hash->Update(auth_bytes.data(),
               auth_bytes.size());
  parameter_section_bytes += auth_bytes;
  command_size += auth_bytes.size();
  std::string command_hash(32, 0);
  hash->Finish(string_as_array(&command_hash), command_hash.size());
  std::string authorization_section_bytes;
  if (!authorization_delegate->GetCommandAuthorization(
      command_hash,
      &authorization_section_bytes)) {
    error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
    return;
  }
  std::string authorization_size_bytes;
  if (!authorization_section_bytes.empty()) {
    tag = TPM_ST_SESSIONS;
    std::string tmp;
    rc = Serialize_UINT32(authorization_section_bytes.size(),
                          &authorization_size_bytes);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    command_size += authorization_size_bytes.size() +
                    authorization_section_bytes.size();
  }
  std::string tag_bytes;
  rc = Serialize_TPMI_ST_COMMAND_TAG(
      tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command_size_bytes;
  rc = Serialize_UINT32(
      command_size,
      &command_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command = tag_bytes +
                        command_size_bytes +
                        command_code_bytes +
                        handle_section_bytes +
                        authorization_size_bytes +
                        authorization_section_bytes +
                        parameter_section_bytes;
  CHECK(command.size() == command_size) << "Command size mismatch!";
  transceiver_->SendCommand(command, parser);
}

void PolicySecretErrorCallback(
    const Tpm::PolicySecretResponse& callback,
    TPM_RC response_code) {
  callback.Run(response_code,
               TPM2B_TIMEOUT(),
               TPMT_TK_AUTH());
}

void PolicySecretResponseParser(
    const Tpm::PolicySecretResponse& callback,
    AuthorizationDelegate* authorization_delegate,
    const std::string& response) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(PolicySecretErrorCallback, callback);
  std::string buffer(response);
  TPM_ST tag;
  std::string tag_bytes;
  rc = Parse_TPM_ST(
      &buffer,
      &tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  UINT32 response_size;
  std::string response_size_bytes;
  rc = Parse_UINT32(
      &buffer,
      &response_size,
      &response_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM_RC response_code;
  std::string response_code_bytes;
  rc = Parse_TPM_RC(
      &buffer,
      &response_code,
      &response_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (response_size != response.size()) {
    error_reporter.Run(TPM_RC_SIZE);
    return;
  }
  if (response_code != TPM_RC_SUCCESS) {
    error_reporter.Run(response_code);
    return;
  }
  TPM_CC command_code = TPM_CC_PolicySecret;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string authorization_section_bytes;
  if (tag == TPM_ST_SESSIONS) {
    UINT32 parameter_section_size = buffer.size();
    rc = Parse_UINT32(&buffer, &parameter_section_size, NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    if (parameter_section_size > buffer.size()) {
      error_reporter.Run(TPM_RC_INSUFFICIENT);
      return;
    }
    authorization_section_bytes = buffer.substr(parameter_section_size);
    // Keep the parameter section in |buffer|.
    buffer.erase(parameter_section_size);
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(response_code_bytes.data(),
               response_code_bytes.size());
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(buffer.data(),
               buffer.size());
  std::string response_hash(32, 0);
  hash->Finish(string_as_array(&response_hash), response_hash.size());
  if (tag == TPM_ST_SESSIONS) {
    if (!authorization_delegate->CheckResponseAuthorization(
        response_hash,
        authorization_section_bytes)) {
      error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
      return;
    }
  }
  TPM2B_TIMEOUT timeout;
  std::string timeout_bytes;
  rc = Parse_TPM2B_TIMEOUT(
      &buffer,
      &timeout,
      &timeout_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPMT_TK_AUTH policy_ticket;
  std::string policy_ticket_bytes;
  rc = Parse_TPMT_TK_AUTH(
      &buffer,
      &policy_ticket,
      &policy_ticket_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (tag == TPM_ST_SESSIONS) {
    // Decrypt just the parameter data, not the size.
    std::string tmp = timeout_bytes.substr(2);
    if (!authorization_delegate->DecryptResponseParameter(&tmp)) {
      error_reporter.Run(TRUNKS_RC_ENCRYPTION_FAILED);
      return;
    }
    timeout_bytes.replace(2, std::string::npos, tmp);
    rc = Parse_TPM2B_TIMEOUT(
        &timeout_bytes,
        &timeout,
        NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
  }
  callback.Run(response_code,
               timeout,
               policy_ticket);
}

void Tpm::PolicySecret(
      TPMI_DH_ENTITY auth_handle,
      const std::string& auth_handle_name,
      TPMI_SH_POLICY policy_session,
      const std::string& policy_session_name,
      TPM2B_NONCE nonce_tpm,
      TPM2B_DIGEST cp_hash_a,
      TPM2B_NONCE policy_ref,
      AuthorizationDelegate* authorization_delegate,
      const PolicySecretResponse& callback) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(PolicySecretErrorCallback, callback);
  base::Callback<void(const std::string&)> parser =
      base::Bind(PolicySecretResponseParser,
                 callback,
                 authorization_delegate);
  TPMI_ST_COMMAND_TAG tag = TPM_ST_NO_SESSIONS;
  UINT32 command_size = 10;  // Header size.
  std::string handle_section_bytes;
  std::string parameter_section_bytes;
  TPM_CC command_code = TPM_CC_PolicySecret;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string auth_handle_bytes;
  rc = Serialize_TPMI_DH_ENTITY(
      auth_handle,
      &auth_handle_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string policy_session_bytes;
  rc = Serialize_TPMI_SH_POLICY(
      policy_session,
      &policy_session_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string nonce_tpm_bytes;
  rc = Serialize_TPM2B_NONCE(
      nonce_tpm,
      &nonce_tpm_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string cp_hash_a_bytes;
  rc = Serialize_TPM2B_DIGEST(
      cp_hash_a,
      &cp_hash_a_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string policy_ref_bytes;
  rc = Serialize_TPM2B_NONCE(
      policy_ref,
      &policy_ref_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  // Encrypt just the parameter data, not the size.
  std::string tmp = nonce_tpm_bytes.substr(2);
  if (!authorization_delegate->EncryptCommandParameter(&tmp)) {
    error_reporter.Run(TRUNKS_RC_ENCRYPTION_FAILED);
    return;
  }
  nonce_tpm_bytes.replace(2, std::string::npos, tmp);
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(auth_handle_name.data(),
               auth_handle_name.size());
  handle_section_bytes += auth_handle_bytes;
  command_size += auth_handle_bytes.size();
  hash->Update(policy_session_name.data(),
               policy_session_name.size());
  handle_section_bytes += policy_session_bytes;
  command_size += policy_session_bytes.size();
  hash->Update(nonce_tpm_bytes.data(),
               nonce_tpm_bytes.size());
  parameter_section_bytes += nonce_tpm_bytes;
  command_size += nonce_tpm_bytes.size();
  hash->Update(cp_hash_a_bytes.data(),
               cp_hash_a_bytes.size());
  parameter_section_bytes += cp_hash_a_bytes;
  command_size += cp_hash_a_bytes.size();
  hash->Update(policy_ref_bytes.data(),
               policy_ref_bytes.size());
  parameter_section_bytes += policy_ref_bytes;
  command_size += policy_ref_bytes.size();
  std::string command_hash(32, 0);
  hash->Finish(string_as_array(&command_hash), command_hash.size());
  std::string authorization_section_bytes;
  if (!authorization_delegate->GetCommandAuthorization(
      command_hash,
      &authorization_section_bytes)) {
    error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
    return;
  }
  std::string authorization_size_bytes;
  if (!authorization_section_bytes.empty()) {
    tag = TPM_ST_SESSIONS;
    std::string tmp;
    rc = Serialize_UINT32(authorization_section_bytes.size(),
                          &authorization_size_bytes);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    command_size += authorization_size_bytes.size() +
                    authorization_section_bytes.size();
  }
  std::string tag_bytes;
  rc = Serialize_TPMI_ST_COMMAND_TAG(
      tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command_size_bytes;
  rc = Serialize_UINT32(
      command_size,
      &command_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command = tag_bytes +
                        command_size_bytes +
                        command_code_bytes +
                        handle_section_bytes +
                        authorization_size_bytes +
                        authorization_section_bytes +
                        parameter_section_bytes;
  CHECK(command.size() == command_size) << "Command size mismatch!";
  transceiver_->SendCommand(command, parser);
}

void PolicyTicketErrorCallback(
    const Tpm::PolicyTicketResponse& callback,
    TPM_RC response_code) {
  callback.Run(response_code);
}

void PolicyTicketResponseParser(
    const Tpm::PolicyTicketResponse& callback,
    AuthorizationDelegate* authorization_delegate,
    const std::string& response) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(PolicyTicketErrorCallback, callback);
  std::string buffer(response);
  TPM_ST tag;
  std::string tag_bytes;
  rc = Parse_TPM_ST(
      &buffer,
      &tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  UINT32 response_size;
  std::string response_size_bytes;
  rc = Parse_UINT32(
      &buffer,
      &response_size,
      &response_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM_RC response_code;
  std::string response_code_bytes;
  rc = Parse_TPM_RC(
      &buffer,
      &response_code,
      &response_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (response_size != response.size()) {
    error_reporter.Run(TPM_RC_SIZE);
    return;
  }
  if (response_code != TPM_RC_SUCCESS) {
    error_reporter.Run(response_code);
    return;
  }
  TPM_CC command_code = TPM_CC_PolicyTicket;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string authorization_section_bytes;
  if (tag == TPM_ST_SESSIONS) {
    UINT32 parameter_section_size = buffer.size();
    rc = Parse_UINT32(&buffer, &parameter_section_size, NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    if (parameter_section_size > buffer.size()) {
      error_reporter.Run(TPM_RC_INSUFFICIENT);
      return;
    }
    authorization_section_bytes = buffer.substr(parameter_section_size);
    // Keep the parameter section in |buffer|.
    buffer.erase(parameter_section_size);
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(response_code_bytes.data(),
               response_code_bytes.size());
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(buffer.data(),
               buffer.size());
  std::string response_hash(32, 0);
  hash->Finish(string_as_array(&response_hash), response_hash.size());
  if (tag == TPM_ST_SESSIONS) {
    if (!authorization_delegate->CheckResponseAuthorization(
        response_hash,
        authorization_section_bytes)) {
      error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
      return;
    }
  }
  callback.Run(response_code);
}

void Tpm::PolicyTicket(
      TPMI_SH_POLICY policy_session,
      const std::string& policy_session_name,
      TPM2B_TIMEOUT timeout,
      TPM2B_DIGEST cp_hash_a,
      TPM2B_NONCE policy_ref,
      TPM2B_NAME auth_name,
      TPMT_TK_AUTH ticket,
      AuthorizationDelegate* authorization_delegate,
      const PolicyTicketResponse& callback) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(PolicyTicketErrorCallback, callback);
  base::Callback<void(const std::string&)> parser =
      base::Bind(PolicyTicketResponseParser,
                 callback,
                 authorization_delegate);
  TPMI_ST_COMMAND_TAG tag = TPM_ST_NO_SESSIONS;
  UINT32 command_size = 10;  // Header size.
  std::string handle_section_bytes;
  std::string parameter_section_bytes;
  TPM_CC command_code = TPM_CC_PolicyTicket;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string policy_session_bytes;
  rc = Serialize_TPMI_SH_POLICY(
      policy_session,
      &policy_session_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string timeout_bytes;
  rc = Serialize_TPM2B_TIMEOUT(
      timeout,
      &timeout_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string cp_hash_a_bytes;
  rc = Serialize_TPM2B_DIGEST(
      cp_hash_a,
      &cp_hash_a_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string policy_ref_bytes;
  rc = Serialize_TPM2B_NONCE(
      policy_ref,
      &policy_ref_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string auth_name_bytes;
  rc = Serialize_TPM2B_NAME(
      auth_name,
      &auth_name_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string ticket_bytes;
  rc = Serialize_TPMT_TK_AUTH(
      ticket,
      &ticket_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  // Encrypt just the parameter data, not the size.
  std::string tmp = timeout_bytes.substr(2);
  if (!authorization_delegate->EncryptCommandParameter(&tmp)) {
    error_reporter.Run(TRUNKS_RC_ENCRYPTION_FAILED);
    return;
  }
  timeout_bytes.replace(2, std::string::npos, tmp);
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(policy_session_name.data(),
               policy_session_name.size());
  handle_section_bytes += policy_session_bytes;
  command_size += policy_session_bytes.size();
  hash->Update(timeout_bytes.data(),
               timeout_bytes.size());
  parameter_section_bytes += timeout_bytes;
  command_size += timeout_bytes.size();
  hash->Update(cp_hash_a_bytes.data(),
               cp_hash_a_bytes.size());
  parameter_section_bytes += cp_hash_a_bytes;
  command_size += cp_hash_a_bytes.size();
  hash->Update(policy_ref_bytes.data(),
               policy_ref_bytes.size());
  parameter_section_bytes += policy_ref_bytes;
  command_size += policy_ref_bytes.size();
  hash->Update(auth_name_bytes.data(),
               auth_name_bytes.size());
  parameter_section_bytes += auth_name_bytes;
  command_size += auth_name_bytes.size();
  hash->Update(ticket_bytes.data(),
               ticket_bytes.size());
  parameter_section_bytes += ticket_bytes;
  command_size += ticket_bytes.size();
  std::string command_hash(32, 0);
  hash->Finish(string_as_array(&command_hash), command_hash.size());
  std::string authorization_section_bytes;
  if (!authorization_delegate->GetCommandAuthorization(
      command_hash,
      &authorization_section_bytes)) {
    error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
    return;
  }
  std::string authorization_size_bytes;
  if (!authorization_section_bytes.empty()) {
    tag = TPM_ST_SESSIONS;
    std::string tmp;
    rc = Serialize_UINT32(authorization_section_bytes.size(),
                          &authorization_size_bytes);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    command_size += authorization_size_bytes.size() +
                    authorization_section_bytes.size();
  }
  std::string tag_bytes;
  rc = Serialize_TPMI_ST_COMMAND_TAG(
      tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command_size_bytes;
  rc = Serialize_UINT32(
      command_size,
      &command_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command = tag_bytes +
                        command_size_bytes +
                        command_code_bytes +
                        handle_section_bytes +
                        authorization_size_bytes +
                        authorization_section_bytes +
                        parameter_section_bytes;
  CHECK(command.size() == command_size) << "Command size mismatch!";
  transceiver_->SendCommand(command, parser);
}

void PolicyORErrorCallback(
    const Tpm::PolicyORResponse& callback,
    TPM_RC response_code) {
  callback.Run(response_code);
}

void PolicyORResponseParser(
    const Tpm::PolicyORResponse& callback,
    AuthorizationDelegate* authorization_delegate,
    const std::string& response) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(PolicyORErrorCallback, callback);
  std::string buffer(response);
  TPM_ST tag;
  std::string tag_bytes;
  rc = Parse_TPM_ST(
      &buffer,
      &tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  UINT32 response_size;
  std::string response_size_bytes;
  rc = Parse_UINT32(
      &buffer,
      &response_size,
      &response_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM_RC response_code;
  std::string response_code_bytes;
  rc = Parse_TPM_RC(
      &buffer,
      &response_code,
      &response_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (response_size != response.size()) {
    error_reporter.Run(TPM_RC_SIZE);
    return;
  }
  if (response_code != TPM_RC_SUCCESS) {
    error_reporter.Run(response_code);
    return;
  }
  TPM_CC command_code = TPM_CC_PolicyOR;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string authorization_section_bytes;
  if (tag == TPM_ST_SESSIONS) {
    UINT32 parameter_section_size = buffer.size();
    rc = Parse_UINT32(&buffer, &parameter_section_size, NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    if (parameter_section_size > buffer.size()) {
      error_reporter.Run(TPM_RC_INSUFFICIENT);
      return;
    }
    authorization_section_bytes = buffer.substr(parameter_section_size);
    // Keep the parameter section in |buffer|.
    buffer.erase(parameter_section_size);
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(response_code_bytes.data(),
               response_code_bytes.size());
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(buffer.data(),
               buffer.size());
  std::string response_hash(32, 0);
  hash->Finish(string_as_array(&response_hash), response_hash.size());
  if (tag == TPM_ST_SESSIONS) {
    if (!authorization_delegate->CheckResponseAuthorization(
        response_hash,
        authorization_section_bytes)) {
      error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
      return;
    }
  }
  callback.Run(response_code);
}

void Tpm::PolicyOR(
      TPMI_SH_POLICY policy_session,
      const std::string& policy_session_name,
      TPML_DIGEST p_hash_list,
      AuthorizationDelegate* authorization_delegate,
      const PolicyORResponse& callback) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(PolicyORErrorCallback, callback);
  base::Callback<void(const std::string&)> parser =
      base::Bind(PolicyORResponseParser,
                 callback,
                 authorization_delegate);
  TPMI_ST_COMMAND_TAG tag = TPM_ST_NO_SESSIONS;
  UINT32 command_size = 10;  // Header size.
  std::string handle_section_bytes;
  std::string parameter_section_bytes;
  TPM_CC command_code = TPM_CC_PolicyOR;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string policy_session_bytes;
  rc = Serialize_TPMI_SH_POLICY(
      policy_session,
      &policy_session_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string p_hash_list_bytes;
  rc = Serialize_TPML_DIGEST(
      p_hash_list,
      &p_hash_list_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(policy_session_name.data(),
               policy_session_name.size());
  handle_section_bytes += policy_session_bytes;
  command_size += policy_session_bytes.size();
  hash->Update(p_hash_list_bytes.data(),
               p_hash_list_bytes.size());
  parameter_section_bytes += p_hash_list_bytes;
  command_size += p_hash_list_bytes.size();
  std::string command_hash(32, 0);
  hash->Finish(string_as_array(&command_hash), command_hash.size());
  std::string authorization_section_bytes;
  if (!authorization_delegate->GetCommandAuthorization(
      command_hash,
      &authorization_section_bytes)) {
    error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
    return;
  }
  std::string authorization_size_bytes;
  if (!authorization_section_bytes.empty()) {
    tag = TPM_ST_SESSIONS;
    std::string tmp;
    rc = Serialize_UINT32(authorization_section_bytes.size(),
                          &authorization_size_bytes);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    command_size += authorization_size_bytes.size() +
                    authorization_section_bytes.size();
  }
  std::string tag_bytes;
  rc = Serialize_TPMI_ST_COMMAND_TAG(
      tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command_size_bytes;
  rc = Serialize_UINT32(
      command_size,
      &command_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command = tag_bytes +
                        command_size_bytes +
                        command_code_bytes +
                        handle_section_bytes +
                        authorization_size_bytes +
                        authorization_section_bytes +
                        parameter_section_bytes;
  CHECK(command.size() == command_size) << "Command size mismatch!";
  transceiver_->SendCommand(command, parser);
}

void PolicyPCRErrorCallback(
    const Tpm::PolicyPCRResponse& callback,
    TPM_RC response_code) {
  callback.Run(response_code);
}

void PolicyPCRResponseParser(
    const Tpm::PolicyPCRResponse& callback,
    AuthorizationDelegate* authorization_delegate,
    const std::string& response) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(PolicyPCRErrorCallback, callback);
  std::string buffer(response);
  TPM_ST tag;
  std::string tag_bytes;
  rc = Parse_TPM_ST(
      &buffer,
      &tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  UINT32 response_size;
  std::string response_size_bytes;
  rc = Parse_UINT32(
      &buffer,
      &response_size,
      &response_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM_RC response_code;
  std::string response_code_bytes;
  rc = Parse_TPM_RC(
      &buffer,
      &response_code,
      &response_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (response_size != response.size()) {
    error_reporter.Run(TPM_RC_SIZE);
    return;
  }
  if (response_code != TPM_RC_SUCCESS) {
    error_reporter.Run(response_code);
    return;
  }
  TPM_CC command_code = TPM_CC_PolicyPCR;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string authorization_section_bytes;
  if (tag == TPM_ST_SESSIONS) {
    UINT32 parameter_section_size = buffer.size();
    rc = Parse_UINT32(&buffer, &parameter_section_size, NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    if (parameter_section_size > buffer.size()) {
      error_reporter.Run(TPM_RC_INSUFFICIENT);
      return;
    }
    authorization_section_bytes = buffer.substr(parameter_section_size);
    // Keep the parameter section in |buffer|.
    buffer.erase(parameter_section_size);
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(response_code_bytes.data(),
               response_code_bytes.size());
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(buffer.data(),
               buffer.size());
  std::string response_hash(32, 0);
  hash->Finish(string_as_array(&response_hash), response_hash.size());
  if (tag == TPM_ST_SESSIONS) {
    if (!authorization_delegate->CheckResponseAuthorization(
        response_hash,
        authorization_section_bytes)) {
      error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
      return;
    }
  }
  callback.Run(response_code);
}

void Tpm::PolicyPCR(
      TPMI_SH_POLICY policy_session,
      const std::string& policy_session_name,
      TPM2B_DIGEST pcr_digest,
      TPML_PCR_SELECTION pcrs,
      AuthorizationDelegate* authorization_delegate,
      const PolicyPCRResponse& callback) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(PolicyPCRErrorCallback, callback);
  base::Callback<void(const std::string&)> parser =
      base::Bind(PolicyPCRResponseParser,
                 callback,
                 authorization_delegate);
  TPMI_ST_COMMAND_TAG tag = TPM_ST_NO_SESSIONS;
  UINT32 command_size = 10;  // Header size.
  std::string handle_section_bytes;
  std::string parameter_section_bytes;
  TPM_CC command_code = TPM_CC_PolicyPCR;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string policy_session_bytes;
  rc = Serialize_TPMI_SH_POLICY(
      policy_session,
      &policy_session_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string pcr_digest_bytes;
  rc = Serialize_TPM2B_DIGEST(
      pcr_digest,
      &pcr_digest_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string pcrs_bytes;
  rc = Serialize_TPML_PCR_SELECTION(
      pcrs,
      &pcrs_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  // Encrypt just the parameter data, not the size.
  std::string tmp = pcr_digest_bytes.substr(2);
  if (!authorization_delegate->EncryptCommandParameter(&tmp)) {
    error_reporter.Run(TRUNKS_RC_ENCRYPTION_FAILED);
    return;
  }
  pcr_digest_bytes.replace(2, std::string::npos, tmp);
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(policy_session_name.data(),
               policy_session_name.size());
  handle_section_bytes += policy_session_bytes;
  command_size += policy_session_bytes.size();
  hash->Update(pcr_digest_bytes.data(),
               pcr_digest_bytes.size());
  parameter_section_bytes += pcr_digest_bytes;
  command_size += pcr_digest_bytes.size();
  hash->Update(pcrs_bytes.data(),
               pcrs_bytes.size());
  parameter_section_bytes += pcrs_bytes;
  command_size += pcrs_bytes.size();
  std::string command_hash(32, 0);
  hash->Finish(string_as_array(&command_hash), command_hash.size());
  std::string authorization_section_bytes;
  if (!authorization_delegate->GetCommandAuthorization(
      command_hash,
      &authorization_section_bytes)) {
    error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
    return;
  }
  std::string authorization_size_bytes;
  if (!authorization_section_bytes.empty()) {
    tag = TPM_ST_SESSIONS;
    std::string tmp;
    rc = Serialize_UINT32(authorization_section_bytes.size(),
                          &authorization_size_bytes);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    command_size += authorization_size_bytes.size() +
                    authorization_section_bytes.size();
  }
  std::string tag_bytes;
  rc = Serialize_TPMI_ST_COMMAND_TAG(
      tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command_size_bytes;
  rc = Serialize_UINT32(
      command_size,
      &command_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command = tag_bytes +
                        command_size_bytes +
                        command_code_bytes +
                        handle_section_bytes +
                        authorization_size_bytes +
                        authorization_section_bytes +
                        parameter_section_bytes;
  CHECK(command.size() == command_size) << "Command size mismatch!";
  transceiver_->SendCommand(command, parser);
}

void PolicyLocalityErrorCallback(
    const Tpm::PolicyLocalityResponse& callback,
    TPM_RC response_code) {
  callback.Run(response_code);
}

void PolicyLocalityResponseParser(
    const Tpm::PolicyLocalityResponse& callback,
    AuthorizationDelegate* authorization_delegate,
    const std::string& response) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(PolicyLocalityErrorCallback, callback);
  std::string buffer(response);
  TPM_ST tag;
  std::string tag_bytes;
  rc = Parse_TPM_ST(
      &buffer,
      &tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  UINT32 response_size;
  std::string response_size_bytes;
  rc = Parse_UINT32(
      &buffer,
      &response_size,
      &response_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM_RC response_code;
  std::string response_code_bytes;
  rc = Parse_TPM_RC(
      &buffer,
      &response_code,
      &response_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (response_size != response.size()) {
    error_reporter.Run(TPM_RC_SIZE);
    return;
  }
  if (response_code != TPM_RC_SUCCESS) {
    error_reporter.Run(response_code);
    return;
  }
  TPM_CC command_code = TPM_CC_PolicyLocality;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string authorization_section_bytes;
  if (tag == TPM_ST_SESSIONS) {
    UINT32 parameter_section_size = buffer.size();
    rc = Parse_UINT32(&buffer, &parameter_section_size, NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    if (parameter_section_size > buffer.size()) {
      error_reporter.Run(TPM_RC_INSUFFICIENT);
      return;
    }
    authorization_section_bytes = buffer.substr(parameter_section_size);
    // Keep the parameter section in |buffer|.
    buffer.erase(parameter_section_size);
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(response_code_bytes.data(),
               response_code_bytes.size());
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(buffer.data(),
               buffer.size());
  std::string response_hash(32, 0);
  hash->Finish(string_as_array(&response_hash), response_hash.size());
  if (tag == TPM_ST_SESSIONS) {
    if (!authorization_delegate->CheckResponseAuthorization(
        response_hash,
        authorization_section_bytes)) {
      error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
      return;
    }
  }
  callback.Run(response_code);
}

void Tpm::PolicyLocality(
      TPMI_SH_POLICY policy_session,
      const std::string& policy_session_name,
      TPMA_LOCALITY locality,
      AuthorizationDelegate* authorization_delegate,
      const PolicyLocalityResponse& callback) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(PolicyLocalityErrorCallback, callback);
  base::Callback<void(const std::string&)> parser =
      base::Bind(PolicyLocalityResponseParser,
                 callback,
                 authorization_delegate);
  TPMI_ST_COMMAND_TAG tag = TPM_ST_NO_SESSIONS;
  UINT32 command_size = 10;  // Header size.
  std::string handle_section_bytes;
  std::string parameter_section_bytes;
  TPM_CC command_code = TPM_CC_PolicyLocality;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string policy_session_bytes;
  rc = Serialize_TPMI_SH_POLICY(
      policy_session,
      &policy_session_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string locality_bytes;
  rc = Serialize_TPMA_LOCALITY(
      locality,
      &locality_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(policy_session_name.data(),
               policy_session_name.size());
  handle_section_bytes += policy_session_bytes;
  command_size += policy_session_bytes.size();
  hash->Update(locality_bytes.data(),
               locality_bytes.size());
  parameter_section_bytes += locality_bytes;
  command_size += locality_bytes.size();
  std::string command_hash(32, 0);
  hash->Finish(string_as_array(&command_hash), command_hash.size());
  std::string authorization_section_bytes;
  if (!authorization_delegate->GetCommandAuthorization(
      command_hash,
      &authorization_section_bytes)) {
    error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
    return;
  }
  std::string authorization_size_bytes;
  if (!authorization_section_bytes.empty()) {
    tag = TPM_ST_SESSIONS;
    std::string tmp;
    rc = Serialize_UINT32(authorization_section_bytes.size(),
                          &authorization_size_bytes);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    command_size += authorization_size_bytes.size() +
                    authorization_section_bytes.size();
  }
  std::string tag_bytes;
  rc = Serialize_TPMI_ST_COMMAND_TAG(
      tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command_size_bytes;
  rc = Serialize_UINT32(
      command_size,
      &command_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command = tag_bytes +
                        command_size_bytes +
                        command_code_bytes +
                        handle_section_bytes +
                        authorization_size_bytes +
                        authorization_section_bytes +
                        parameter_section_bytes;
  CHECK(command.size() == command_size) << "Command size mismatch!";
  transceiver_->SendCommand(command, parser);
}

void PolicyNVErrorCallback(
    const Tpm::PolicyNVResponse& callback,
    TPM_RC response_code) {
  callback.Run(response_code);
}

void PolicyNVResponseParser(
    const Tpm::PolicyNVResponse& callback,
    AuthorizationDelegate* authorization_delegate,
    const std::string& response) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(PolicyNVErrorCallback, callback);
  std::string buffer(response);
  TPM_ST tag;
  std::string tag_bytes;
  rc = Parse_TPM_ST(
      &buffer,
      &tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  UINT32 response_size;
  std::string response_size_bytes;
  rc = Parse_UINT32(
      &buffer,
      &response_size,
      &response_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM_RC response_code;
  std::string response_code_bytes;
  rc = Parse_TPM_RC(
      &buffer,
      &response_code,
      &response_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (response_size != response.size()) {
    error_reporter.Run(TPM_RC_SIZE);
    return;
  }
  if (response_code != TPM_RC_SUCCESS) {
    error_reporter.Run(response_code);
    return;
  }
  TPM_CC command_code = TPM_CC_PolicyNV;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string authorization_section_bytes;
  if (tag == TPM_ST_SESSIONS) {
    UINT32 parameter_section_size = buffer.size();
    rc = Parse_UINT32(&buffer, &parameter_section_size, NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    if (parameter_section_size > buffer.size()) {
      error_reporter.Run(TPM_RC_INSUFFICIENT);
      return;
    }
    authorization_section_bytes = buffer.substr(parameter_section_size);
    // Keep the parameter section in |buffer|.
    buffer.erase(parameter_section_size);
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(response_code_bytes.data(),
               response_code_bytes.size());
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(buffer.data(),
               buffer.size());
  std::string response_hash(32, 0);
  hash->Finish(string_as_array(&response_hash), response_hash.size());
  if (tag == TPM_ST_SESSIONS) {
    if (!authorization_delegate->CheckResponseAuthorization(
        response_hash,
        authorization_section_bytes)) {
      error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
      return;
    }
  }
  callback.Run(response_code);
}

void Tpm::PolicyNV(
      TPMI_RH_NV_AUTH auth_handle,
      const std::string& auth_handle_name,
      TPMI_RH_NV_INDEX nv_index,
      const std::string& nv_index_name,
      TPMI_SH_POLICY policy_session,
      const std::string& policy_session_name,
      TPM2B_OPERAND operand_b,
      UINT16 offset,
      TPM_EO operation,
      AuthorizationDelegate* authorization_delegate,
      const PolicyNVResponse& callback) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(PolicyNVErrorCallback, callback);
  base::Callback<void(const std::string&)> parser =
      base::Bind(PolicyNVResponseParser,
                 callback,
                 authorization_delegate);
  TPMI_ST_COMMAND_TAG tag = TPM_ST_NO_SESSIONS;
  UINT32 command_size = 10;  // Header size.
  std::string handle_section_bytes;
  std::string parameter_section_bytes;
  TPM_CC command_code = TPM_CC_PolicyNV;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string auth_handle_bytes;
  rc = Serialize_TPMI_RH_NV_AUTH(
      auth_handle,
      &auth_handle_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string nv_index_bytes;
  rc = Serialize_TPMI_RH_NV_INDEX(
      nv_index,
      &nv_index_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string policy_session_bytes;
  rc = Serialize_TPMI_SH_POLICY(
      policy_session,
      &policy_session_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string operand_b_bytes;
  rc = Serialize_TPM2B_OPERAND(
      operand_b,
      &operand_b_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string offset_bytes;
  rc = Serialize_UINT16(
      offset,
      &offset_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string operation_bytes;
  rc = Serialize_TPM_EO(
      operation,
      &operation_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  // Encrypt just the parameter data, not the size.
  std::string tmp = operand_b_bytes.substr(2);
  if (!authorization_delegate->EncryptCommandParameter(&tmp)) {
    error_reporter.Run(TRUNKS_RC_ENCRYPTION_FAILED);
    return;
  }
  operand_b_bytes.replace(2, std::string::npos, tmp);
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(auth_handle_name.data(),
               auth_handle_name.size());
  handle_section_bytes += auth_handle_bytes;
  command_size += auth_handle_bytes.size();
  hash->Update(nv_index_name.data(),
               nv_index_name.size());
  handle_section_bytes += nv_index_bytes;
  command_size += nv_index_bytes.size();
  hash->Update(policy_session_name.data(),
               policy_session_name.size());
  handle_section_bytes += policy_session_bytes;
  command_size += policy_session_bytes.size();
  hash->Update(operand_b_bytes.data(),
               operand_b_bytes.size());
  parameter_section_bytes += operand_b_bytes;
  command_size += operand_b_bytes.size();
  hash->Update(offset_bytes.data(),
               offset_bytes.size());
  parameter_section_bytes += offset_bytes;
  command_size += offset_bytes.size();
  hash->Update(operation_bytes.data(),
               operation_bytes.size());
  parameter_section_bytes += operation_bytes;
  command_size += operation_bytes.size();
  std::string command_hash(32, 0);
  hash->Finish(string_as_array(&command_hash), command_hash.size());
  std::string authorization_section_bytes;
  if (!authorization_delegate->GetCommandAuthorization(
      command_hash,
      &authorization_section_bytes)) {
    error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
    return;
  }
  std::string authorization_size_bytes;
  if (!authorization_section_bytes.empty()) {
    tag = TPM_ST_SESSIONS;
    std::string tmp;
    rc = Serialize_UINT32(authorization_section_bytes.size(),
                          &authorization_size_bytes);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    command_size += authorization_size_bytes.size() +
                    authorization_section_bytes.size();
  }
  std::string tag_bytes;
  rc = Serialize_TPMI_ST_COMMAND_TAG(
      tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command_size_bytes;
  rc = Serialize_UINT32(
      command_size,
      &command_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command = tag_bytes +
                        command_size_bytes +
                        command_code_bytes +
                        handle_section_bytes +
                        authorization_size_bytes +
                        authorization_section_bytes +
                        parameter_section_bytes;
  CHECK(command.size() == command_size) << "Command size mismatch!";
  transceiver_->SendCommand(command, parser);
}

void PolicyCounterTimerErrorCallback(
    const Tpm::PolicyCounterTimerResponse& callback,
    TPM_RC response_code) {
  callback.Run(response_code);
}

void PolicyCounterTimerResponseParser(
    const Tpm::PolicyCounterTimerResponse& callback,
    AuthorizationDelegate* authorization_delegate,
    const std::string& response) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(PolicyCounterTimerErrorCallback, callback);
  std::string buffer(response);
  TPM_ST tag;
  std::string tag_bytes;
  rc = Parse_TPM_ST(
      &buffer,
      &tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  UINT32 response_size;
  std::string response_size_bytes;
  rc = Parse_UINT32(
      &buffer,
      &response_size,
      &response_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM_RC response_code;
  std::string response_code_bytes;
  rc = Parse_TPM_RC(
      &buffer,
      &response_code,
      &response_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (response_size != response.size()) {
    error_reporter.Run(TPM_RC_SIZE);
    return;
  }
  if (response_code != TPM_RC_SUCCESS) {
    error_reporter.Run(response_code);
    return;
  }
  TPM_CC command_code = TPM_CC_PolicyCounterTimer;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string authorization_section_bytes;
  if (tag == TPM_ST_SESSIONS) {
    UINT32 parameter_section_size = buffer.size();
    rc = Parse_UINT32(&buffer, &parameter_section_size, NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    if (parameter_section_size > buffer.size()) {
      error_reporter.Run(TPM_RC_INSUFFICIENT);
      return;
    }
    authorization_section_bytes = buffer.substr(parameter_section_size);
    // Keep the parameter section in |buffer|.
    buffer.erase(parameter_section_size);
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(response_code_bytes.data(),
               response_code_bytes.size());
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(buffer.data(),
               buffer.size());
  std::string response_hash(32, 0);
  hash->Finish(string_as_array(&response_hash), response_hash.size());
  if (tag == TPM_ST_SESSIONS) {
    if (!authorization_delegate->CheckResponseAuthorization(
        response_hash,
        authorization_section_bytes)) {
      error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
      return;
    }
  }
  callback.Run(response_code);
}

void Tpm::PolicyCounterTimer(
      TPMI_SH_POLICY policy_session,
      const std::string& policy_session_name,
      TPM2B_OPERAND operand_b,
      UINT16 offset,
      TPM_EO operation,
      AuthorizationDelegate* authorization_delegate,
      const PolicyCounterTimerResponse& callback) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(PolicyCounterTimerErrorCallback, callback);
  base::Callback<void(const std::string&)> parser =
      base::Bind(PolicyCounterTimerResponseParser,
                 callback,
                 authorization_delegate);
  TPMI_ST_COMMAND_TAG tag = TPM_ST_NO_SESSIONS;
  UINT32 command_size = 10;  // Header size.
  std::string handle_section_bytes;
  std::string parameter_section_bytes;
  TPM_CC command_code = TPM_CC_PolicyCounterTimer;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string policy_session_bytes;
  rc = Serialize_TPMI_SH_POLICY(
      policy_session,
      &policy_session_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string operand_b_bytes;
  rc = Serialize_TPM2B_OPERAND(
      operand_b,
      &operand_b_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string offset_bytes;
  rc = Serialize_UINT16(
      offset,
      &offset_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string operation_bytes;
  rc = Serialize_TPM_EO(
      operation,
      &operation_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  // Encrypt just the parameter data, not the size.
  std::string tmp = operand_b_bytes.substr(2);
  if (!authorization_delegate->EncryptCommandParameter(&tmp)) {
    error_reporter.Run(TRUNKS_RC_ENCRYPTION_FAILED);
    return;
  }
  operand_b_bytes.replace(2, std::string::npos, tmp);
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(policy_session_name.data(),
               policy_session_name.size());
  handle_section_bytes += policy_session_bytes;
  command_size += policy_session_bytes.size();
  hash->Update(operand_b_bytes.data(),
               operand_b_bytes.size());
  parameter_section_bytes += operand_b_bytes;
  command_size += operand_b_bytes.size();
  hash->Update(offset_bytes.data(),
               offset_bytes.size());
  parameter_section_bytes += offset_bytes;
  command_size += offset_bytes.size();
  hash->Update(operation_bytes.data(),
               operation_bytes.size());
  parameter_section_bytes += operation_bytes;
  command_size += operation_bytes.size();
  std::string command_hash(32, 0);
  hash->Finish(string_as_array(&command_hash), command_hash.size());
  std::string authorization_section_bytes;
  if (!authorization_delegate->GetCommandAuthorization(
      command_hash,
      &authorization_section_bytes)) {
    error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
    return;
  }
  std::string authorization_size_bytes;
  if (!authorization_section_bytes.empty()) {
    tag = TPM_ST_SESSIONS;
    std::string tmp;
    rc = Serialize_UINT32(authorization_section_bytes.size(),
                          &authorization_size_bytes);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    command_size += authorization_size_bytes.size() +
                    authorization_section_bytes.size();
  }
  std::string tag_bytes;
  rc = Serialize_TPMI_ST_COMMAND_TAG(
      tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command_size_bytes;
  rc = Serialize_UINT32(
      command_size,
      &command_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command = tag_bytes +
                        command_size_bytes +
                        command_code_bytes +
                        handle_section_bytes +
                        authorization_size_bytes +
                        authorization_section_bytes +
                        parameter_section_bytes;
  CHECK(command.size() == command_size) << "Command size mismatch!";
  transceiver_->SendCommand(command, parser);
}

void PolicyCommandCodeErrorCallback(
    const Tpm::PolicyCommandCodeResponse& callback,
    TPM_RC response_code) {
  callback.Run(response_code);
}

void PolicyCommandCodeResponseParser(
    const Tpm::PolicyCommandCodeResponse& callback,
    AuthorizationDelegate* authorization_delegate,
    const std::string& response) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(PolicyCommandCodeErrorCallback, callback);
  std::string buffer(response);
  TPM_ST tag;
  std::string tag_bytes;
  rc = Parse_TPM_ST(
      &buffer,
      &tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  UINT32 response_size;
  std::string response_size_bytes;
  rc = Parse_UINT32(
      &buffer,
      &response_size,
      &response_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM_RC response_code;
  std::string response_code_bytes;
  rc = Parse_TPM_RC(
      &buffer,
      &response_code,
      &response_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (response_size != response.size()) {
    error_reporter.Run(TPM_RC_SIZE);
    return;
  }
  if (response_code != TPM_RC_SUCCESS) {
    error_reporter.Run(response_code);
    return;
  }
  TPM_CC command_code = TPM_CC_PolicyCommandCode;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string authorization_section_bytes;
  if (tag == TPM_ST_SESSIONS) {
    UINT32 parameter_section_size = buffer.size();
    rc = Parse_UINT32(&buffer, &parameter_section_size, NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    if (parameter_section_size > buffer.size()) {
      error_reporter.Run(TPM_RC_INSUFFICIENT);
      return;
    }
    authorization_section_bytes = buffer.substr(parameter_section_size);
    // Keep the parameter section in |buffer|.
    buffer.erase(parameter_section_size);
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(response_code_bytes.data(),
               response_code_bytes.size());
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(buffer.data(),
               buffer.size());
  std::string response_hash(32, 0);
  hash->Finish(string_as_array(&response_hash), response_hash.size());
  if (tag == TPM_ST_SESSIONS) {
    if (!authorization_delegate->CheckResponseAuthorization(
        response_hash,
        authorization_section_bytes)) {
      error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
      return;
    }
  }
  callback.Run(response_code);
}

void Tpm::PolicyCommandCode(
      TPMI_SH_POLICY policy_session,
      const std::string& policy_session_name,
      TPM_CC code,
      AuthorizationDelegate* authorization_delegate,
      const PolicyCommandCodeResponse& callback) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(PolicyCommandCodeErrorCallback, callback);
  base::Callback<void(const std::string&)> parser =
      base::Bind(PolicyCommandCodeResponseParser,
                 callback,
                 authorization_delegate);
  TPMI_ST_COMMAND_TAG tag = TPM_ST_NO_SESSIONS;
  UINT32 command_size = 10;  // Header size.
  std::string handle_section_bytes;
  std::string parameter_section_bytes;
  TPM_CC command_code = TPM_CC_PolicyCommandCode;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string policy_session_bytes;
  rc = Serialize_TPMI_SH_POLICY(
      policy_session,
      &policy_session_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string code_bytes;
  rc = Serialize_TPM_CC(
      code,
      &code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(policy_session_name.data(),
               policy_session_name.size());
  handle_section_bytes += policy_session_bytes;
  command_size += policy_session_bytes.size();
  hash->Update(code_bytes.data(),
               code_bytes.size());
  parameter_section_bytes += code_bytes;
  command_size += code_bytes.size();
  std::string command_hash(32, 0);
  hash->Finish(string_as_array(&command_hash), command_hash.size());
  std::string authorization_section_bytes;
  if (!authorization_delegate->GetCommandAuthorization(
      command_hash,
      &authorization_section_bytes)) {
    error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
    return;
  }
  std::string authorization_size_bytes;
  if (!authorization_section_bytes.empty()) {
    tag = TPM_ST_SESSIONS;
    std::string tmp;
    rc = Serialize_UINT32(authorization_section_bytes.size(),
                          &authorization_size_bytes);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    command_size += authorization_size_bytes.size() +
                    authorization_section_bytes.size();
  }
  std::string tag_bytes;
  rc = Serialize_TPMI_ST_COMMAND_TAG(
      tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command_size_bytes;
  rc = Serialize_UINT32(
      command_size,
      &command_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command = tag_bytes +
                        command_size_bytes +
                        command_code_bytes +
                        handle_section_bytes +
                        authorization_size_bytes +
                        authorization_section_bytes +
                        parameter_section_bytes;
  CHECK(command.size() == command_size) << "Command size mismatch!";
  transceiver_->SendCommand(command, parser);
}

void PolicyPhysicalPresenceErrorCallback(
    const Tpm::PolicyPhysicalPresenceResponse& callback,
    TPM_RC response_code) {
  callback.Run(response_code);
}

void PolicyPhysicalPresenceResponseParser(
    const Tpm::PolicyPhysicalPresenceResponse& callback,
    AuthorizationDelegate* authorization_delegate,
    const std::string& response) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(PolicyPhysicalPresenceErrorCallback, callback);
  std::string buffer(response);
  TPM_ST tag;
  std::string tag_bytes;
  rc = Parse_TPM_ST(
      &buffer,
      &tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  UINT32 response_size;
  std::string response_size_bytes;
  rc = Parse_UINT32(
      &buffer,
      &response_size,
      &response_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM_RC response_code;
  std::string response_code_bytes;
  rc = Parse_TPM_RC(
      &buffer,
      &response_code,
      &response_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (response_size != response.size()) {
    error_reporter.Run(TPM_RC_SIZE);
    return;
  }
  if (response_code != TPM_RC_SUCCESS) {
    error_reporter.Run(response_code);
    return;
  }
  TPM_CC command_code = TPM_CC_PolicyPhysicalPresence;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string authorization_section_bytes;
  if (tag == TPM_ST_SESSIONS) {
    UINT32 parameter_section_size = buffer.size();
    rc = Parse_UINT32(&buffer, &parameter_section_size, NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    if (parameter_section_size > buffer.size()) {
      error_reporter.Run(TPM_RC_INSUFFICIENT);
      return;
    }
    authorization_section_bytes = buffer.substr(parameter_section_size);
    // Keep the parameter section in |buffer|.
    buffer.erase(parameter_section_size);
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(response_code_bytes.data(),
               response_code_bytes.size());
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(buffer.data(),
               buffer.size());
  std::string response_hash(32, 0);
  hash->Finish(string_as_array(&response_hash), response_hash.size());
  if (tag == TPM_ST_SESSIONS) {
    if (!authorization_delegate->CheckResponseAuthorization(
        response_hash,
        authorization_section_bytes)) {
      error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
      return;
    }
  }
  callback.Run(response_code);
}

void Tpm::PolicyPhysicalPresence(
      TPMI_SH_POLICY policy_session,
      const std::string& policy_session_name,
      AuthorizationDelegate* authorization_delegate,
      const PolicyPhysicalPresenceResponse& callback) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(PolicyPhysicalPresenceErrorCallback, callback);
  base::Callback<void(const std::string&)> parser =
      base::Bind(PolicyPhysicalPresenceResponseParser,
                 callback,
                 authorization_delegate);
  TPMI_ST_COMMAND_TAG tag = TPM_ST_NO_SESSIONS;
  UINT32 command_size = 10;  // Header size.
  std::string handle_section_bytes;
  std::string parameter_section_bytes;
  TPM_CC command_code = TPM_CC_PolicyPhysicalPresence;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string policy_session_bytes;
  rc = Serialize_TPMI_SH_POLICY(
      policy_session,
      &policy_session_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(policy_session_name.data(),
               policy_session_name.size());
  handle_section_bytes += policy_session_bytes;
  command_size += policy_session_bytes.size();
  std::string command_hash(32, 0);
  hash->Finish(string_as_array(&command_hash), command_hash.size());
  std::string authorization_section_bytes;
  if (!authorization_delegate->GetCommandAuthorization(
      command_hash,
      &authorization_section_bytes)) {
    error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
    return;
  }
  std::string authorization_size_bytes;
  if (!authorization_section_bytes.empty()) {
    tag = TPM_ST_SESSIONS;
    std::string tmp;
    rc = Serialize_UINT32(authorization_section_bytes.size(),
                          &authorization_size_bytes);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    command_size += authorization_size_bytes.size() +
                    authorization_section_bytes.size();
  }
  std::string tag_bytes;
  rc = Serialize_TPMI_ST_COMMAND_TAG(
      tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command_size_bytes;
  rc = Serialize_UINT32(
      command_size,
      &command_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command = tag_bytes +
                        command_size_bytes +
                        command_code_bytes +
                        handle_section_bytes +
                        authorization_size_bytes +
                        authorization_section_bytes +
                        parameter_section_bytes;
  CHECK(command.size() == command_size) << "Command size mismatch!";
  transceiver_->SendCommand(command, parser);
}

void PolicyCpHashErrorCallback(
    const Tpm::PolicyCpHashResponse& callback,
    TPM_RC response_code) {
  callback.Run(response_code);
}

void PolicyCpHashResponseParser(
    const Tpm::PolicyCpHashResponse& callback,
    AuthorizationDelegate* authorization_delegate,
    const std::string& response) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(PolicyCpHashErrorCallback, callback);
  std::string buffer(response);
  TPM_ST tag;
  std::string tag_bytes;
  rc = Parse_TPM_ST(
      &buffer,
      &tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  UINT32 response_size;
  std::string response_size_bytes;
  rc = Parse_UINT32(
      &buffer,
      &response_size,
      &response_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM_RC response_code;
  std::string response_code_bytes;
  rc = Parse_TPM_RC(
      &buffer,
      &response_code,
      &response_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (response_size != response.size()) {
    error_reporter.Run(TPM_RC_SIZE);
    return;
  }
  if (response_code != TPM_RC_SUCCESS) {
    error_reporter.Run(response_code);
    return;
  }
  TPM_CC command_code = TPM_CC_PolicyCpHash;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string authorization_section_bytes;
  if (tag == TPM_ST_SESSIONS) {
    UINT32 parameter_section_size = buffer.size();
    rc = Parse_UINT32(&buffer, &parameter_section_size, NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    if (parameter_section_size > buffer.size()) {
      error_reporter.Run(TPM_RC_INSUFFICIENT);
      return;
    }
    authorization_section_bytes = buffer.substr(parameter_section_size);
    // Keep the parameter section in |buffer|.
    buffer.erase(parameter_section_size);
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(response_code_bytes.data(),
               response_code_bytes.size());
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(buffer.data(),
               buffer.size());
  std::string response_hash(32, 0);
  hash->Finish(string_as_array(&response_hash), response_hash.size());
  if (tag == TPM_ST_SESSIONS) {
    if (!authorization_delegate->CheckResponseAuthorization(
        response_hash,
        authorization_section_bytes)) {
      error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
      return;
    }
  }
  callback.Run(response_code);
}

void Tpm::PolicyCpHash(
      TPMI_SH_POLICY policy_session,
      const std::string& policy_session_name,
      TPM2B_DIGEST cp_hash_a,
      AuthorizationDelegate* authorization_delegate,
      const PolicyCpHashResponse& callback) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(PolicyCpHashErrorCallback, callback);
  base::Callback<void(const std::string&)> parser =
      base::Bind(PolicyCpHashResponseParser,
                 callback,
                 authorization_delegate);
  TPMI_ST_COMMAND_TAG tag = TPM_ST_NO_SESSIONS;
  UINT32 command_size = 10;  // Header size.
  std::string handle_section_bytes;
  std::string parameter_section_bytes;
  TPM_CC command_code = TPM_CC_PolicyCpHash;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string policy_session_bytes;
  rc = Serialize_TPMI_SH_POLICY(
      policy_session,
      &policy_session_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string cp_hash_a_bytes;
  rc = Serialize_TPM2B_DIGEST(
      cp_hash_a,
      &cp_hash_a_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  // Encrypt just the parameter data, not the size.
  std::string tmp = cp_hash_a_bytes.substr(2);
  if (!authorization_delegate->EncryptCommandParameter(&tmp)) {
    error_reporter.Run(TRUNKS_RC_ENCRYPTION_FAILED);
    return;
  }
  cp_hash_a_bytes.replace(2, std::string::npos, tmp);
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(policy_session_name.data(),
               policy_session_name.size());
  handle_section_bytes += policy_session_bytes;
  command_size += policy_session_bytes.size();
  hash->Update(cp_hash_a_bytes.data(),
               cp_hash_a_bytes.size());
  parameter_section_bytes += cp_hash_a_bytes;
  command_size += cp_hash_a_bytes.size();
  std::string command_hash(32, 0);
  hash->Finish(string_as_array(&command_hash), command_hash.size());
  std::string authorization_section_bytes;
  if (!authorization_delegate->GetCommandAuthorization(
      command_hash,
      &authorization_section_bytes)) {
    error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
    return;
  }
  std::string authorization_size_bytes;
  if (!authorization_section_bytes.empty()) {
    tag = TPM_ST_SESSIONS;
    std::string tmp;
    rc = Serialize_UINT32(authorization_section_bytes.size(),
                          &authorization_size_bytes);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    command_size += authorization_size_bytes.size() +
                    authorization_section_bytes.size();
  }
  std::string tag_bytes;
  rc = Serialize_TPMI_ST_COMMAND_TAG(
      tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command_size_bytes;
  rc = Serialize_UINT32(
      command_size,
      &command_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command = tag_bytes +
                        command_size_bytes +
                        command_code_bytes +
                        handle_section_bytes +
                        authorization_size_bytes +
                        authorization_section_bytes +
                        parameter_section_bytes;
  CHECK(command.size() == command_size) << "Command size mismatch!";
  transceiver_->SendCommand(command, parser);
}

void PolicyNameHashErrorCallback(
    const Tpm::PolicyNameHashResponse& callback,
    TPM_RC response_code) {
  callback.Run(response_code);
}

void PolicyNameHashResponseParser(
    const Tpm::PolicyNameHashResponse& callback,
    AuthorizationDelegate* authorization_delegate,
    const std::string& response) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(PolicyNameHashErrorCallback, callback);
  std::string buffer(response);
  TPM_ST tag;
  std::string tag_bytes;
  rc = Parse_TPM_ST(
      &buffer,
      &tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  UINT32 response_size;
  std::string response_size_bytes;
  rc = Parse_UINT32(
      &buffer,
      &response_size,
      &response_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM_RC response_code;
  std::string response_code_bytes;
  rc = Parse_TPM_RC(
      &buffer,
      &response_code,
      &response_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (response_size != response.size()) {
    error_reporter.Run(TPM_RC_SIZE);
    return;
  }
  if (response_code != TPM_RC_SUCCESS) {
    error_reporter.Run(response_code);
    return;
  }
  TPM_CC command_code = TPM_CC_PolicyNameHash;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string authorization_section_bytes;
  if (tag == TPM_ST_SESSIONS) {
    UINT32 parameter_section_size = buffer.size();
    rc = Parse_UINT32(&buffer, &parameter_section_size, NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    if (parameter_section_size > buffer.size()) {
      error_reporter.Run(TPM_RC_INSUFFICIENT);
      return;
    }
    authorization_section_bytes = buffer.substr(parameter_section_size);
    // Keep the parameter section in |buffer|.
    buffer.erase(parameter_section_size);
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(response_code_bytes.data(),
               response_code_bytes.size());
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(buffer.data(),
               buffer.size());
  std::string response_hash(32, 0);
  hash->Finish(string_as_array(&response_hash), response_hash.size());
  if (tag == TPM_ST_SESSIONS) {
    if (!authorization_delegate->CheckResponseAuthorization(
        response_hash,
        authorization_section_bytes)) {
      error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
      return;
    }
  }
  callback.Run(response_code);
}

void Tpm::PolicyNameHash(
      TPMI_SH_POLICY policy_session,
      const std::string& policy_session_name,
      TPM2B_DIGEST name_hash,
      AuthorizationDelegate* authorization_delegate,
      const PolicyNameHashResponse& callback) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(PolicyNameHashErrorCallback, callback);
  base::Callback<void(const std::string&)> parser =
      base::Bind(PolicyNameHashResponseParser,
                 callback,
                 authorization_delegate);
  TPMI_ST_COMMAND_TAG tag = TPM_ST_NO_SESSIONS;
  UINT32 command_size = 10;  // Header size.
  std::string handle_section_bytes;
  std::string parameter_section_bytes;
  TPM_CC command_code = TPM_CC_PolicyNameHash;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string policy_session_bytes;
  rc = Serialize_TPMI_SH_POLICY(
      policy_session,
      &policy_session_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string name_hash_bytes;
  rc = Serialize_TPM2B_DIGEST(
      name_hash,
      &name_hash_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  // Encrypt just the parameter data, not the size.
  std::string tmp = name_hash_bytes.substr(2);
  if (!authorization_delegate->EncryptCommandParameter(&tmp)) {
    error_reporter.Run(TRUNKS_RC_ENCRYPTION_FAILED);
    return;
  }
  name_hash_bytes.replace(2, std::string::npos, tmp);
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(policy_session_name.data(),
               policy_session_name.size());
  handle_section_bytes += policy_session_bytes;
  command_size += policy_session_bytes.size();
  hash->Update(name_hash_bytes.data(),
               name_hash_bytes.size());
  parameter_section_bytes += name_hash_bytes;
  command_size += name_hash_bytes.size();
  std::string command_hash(32, 0);
  hash->Finish(string_as_array(&command_hash), command_hash.size());
  std::string authorization_section_bytes;
  if (!authorization_delegate->GetCommandAuthorization(
      command_hash,
      &authorization_section_bytes)) {
    error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
    return;
  }
  std::string authorization_size_bytes;
  if (!authorization_section_bytes.empty()) {
    tag = TPM_ST_SESSIONS;
    std::string tmp;
    rc = Serialize_UINT32(authorization_section_bytes.size(),
                          &authorization_size_bytes);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    command_size += authorization_size_bytes.size() +
                    authorization_section_bytes.size();
  }
  std::string tag_bytes;
  rc = Serialize_TPMI_ST_COMMAND_TAG(
      tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command_size_bytes;
  rc = Serialize_UINT32(
      command_size,
      &command_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command = tag_bytes +
                        command_size_bytes +
                        command_code_bytes +
                        handle_section_bytes +
                        authorization_size_bytes +
                        authorization_section_bytes +
                        parameter_section_bytes;
  CHECK(command.size() == command_size) << "Command size mismatch!";
  transceiver_->SendCommand(command, parser);
}

void PolicyDuplicationSelectErrorCallback(
    const Tpm::PolicyDuplicationSelectResponse& callback,
    TPM_RC response_code) {
  callback.Run(response_code);
}

void PolicyDuplicationSelectResponseParser(
    const Tpm::PolicyDuplicationSelectResponse& callback,
    AuthorizationDelegate* authorization_delegate,
    const std::string& response) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(PolicyDuplicationSelectErrorCallback, callback);
  std::string buffer(response);
  TPM_ST tag;
  std::string tag_bytes;
  rc = Parse_TPM_ST(
      &buffer,
      &tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  UINT32 response_size;
  std::string response_size_bytes;
  rc = Parse_UINT32(
      &buffer,
      &response_size,
      &response_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM_RC response_code;
  std::string response_code_bytes;
  rc = Parse_TPM_RC(
      &buffer,
      &response_code,
      &response_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (response_size != response.size()) {
    error_reporter.Run(TPM_RC_SIZE);
    return;
  }
  if (response_code != TPM_RC_SUCCESS) {
    error_reporter.Run(response_code);
    return;
  }
  TPM_CC command_code = TPM_CC_PolicyDuplicationSelect;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string authorization_section_bytes;
  if (tag == TPM_ST_SESSIONS) {
    UINT32 parameter_section_size = buffer.size();
    rc = Parse_UINT32(&buffer, &parameter_section_size, NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    if (parameter_section_size > buffer.size()) {
      error_reporter.Run(TPM_RC_INSUFFICIENT);
      return;
    }
    authorization_section_bytes = buffer.substr(parameter_section_size);
    // Keep the parameter section in |buffer|.
    buffer.erase(parameter_section_size);
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(response_code_bytes.data(),
               response_code_bytes.size());
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(buffer.data(),
               buffer.size());
  std::string response_hash(32, 0);
  hash->Finish(string_as_array(&response_hash), response_hash.size());
  if (tag == TPM_ST_SESSIONS) {
    if (!authorization_delegate->CheckResponseAuthorization(
        response_hash,
        authorization_section_bytes)) {
      error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
      return;
    }
  }
  callback.Run(response_code);
}

void Tpm::PolicyDuplicationSelect(
      TPMI_SH_POLICY policy_session,
      const std::string& policy_session_name,
      TPM2B_NAME object_name,
      TPM2B_NAME new_parent_name,
      TPMI_YES_NO include_object,
      AuthorizationDelegate* authorization_delegate,
      const PolicyDuplicationSelectResponse& callback) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(PolicyDuplicationSelectErrorCallback, callback);
  base::Callback<void(const std::string&)> parser =
      base::Bind(PolicyDuplicationSelectResponseParser,
                 callback,
                 authorization_delegate);
  TPMI_ST_COMMAND_TAG tag = TPM_ST_NO_SESSIONS;
  UINT32 command_size = 10;  // Header size.
  std::string handle_section_bytes;
  std::string parameter_section_bytes;
  TPM_CC command_code = TPM_CC_PolicyDuplicationSelect;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string policy_session_bytes;
  rc = Serialize_TPMI_SH_POLICY(
      policy_session,
      &policy_session_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string object_name_bytes;
  rc = Serialize_TPM2B_NAME(
      object_name,
      &object_name_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string new_parent_name_bytes;
  rc = Serialize_TPM2B_NAME(
      new_parent_name,
      &new_parent_name_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string include_object_bytes;
  rc = Serialize_TPMI_YES_NO(
      include_object,
      &include_object_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  // Encrypt just the parameter data, not the size.
  std::string tmp = object_name_bytes.substr(2);
  if (!authorization_delegate->EncryptCommandParameter(&tmp)) {
    error_reporter.Run(TRUNKS_RC_ENCRYPTION_FAILED);
    return;
  }
  object_name_bytes.replace(2, std::string::npos, tmp);
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(policy_session_name.data(),
               policy_session_name.size());
  handle_section_bytes += policy_session_bytes;
  command_size += policy_session_bytes.size();
  hash->Update(object_name_bytes.data(),
               object_name_bytes.size());
  parameter_section_bytes += object_name_bytes;
  command_size += object_name_bytes.size();
  hash->Update(new_parent_name_bytes.data(),
               new_parent_name_bytes.size());
  parameter_section_bytes += new_parent_name_bytes;
  command_size += new_parent_name_bytes.size();
  hash->Update(include_object_bytes.data(),
               include_object_bytes.size());
  parameter_section_bytes += include_object_bytes;
  command_size += include_object_bytes.size();
  std::string command_hash(32, 0);
  hash->Finish(string_as_array(&command_hash), command_hash.size());
  std::string authorization_section_bytes;
  if (!authorization_delegate->GetCommandAuthorization(
      command_hash,
      &authorization_section_bytes)) {
    error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
    return;
  }
  std::string authorization_size_bytes;
  if (!authorization_section_bytes.empty()) {
    tag = TPM_ST_SESSIONS;
    std::string tmp;
    rc = Serialize_UINT32(authorization_section_bytes.size(),
                          &authorization_size_bytes);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    command_size += authorization_size_bytes.size() +
                    authorization_section_bytes.size();
  }
  std::string tag_bytes;
  rc = Serialize_TPMI_ST_COMMAND_TAG(
      tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command_size_bytes;
  rc = Serialize_UINT32(
      command_size,
      &command_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command = tag_bytes +
                        command_size_bytes +
                        command_code_bytes +
                        handle_section_bytes +
                        authorization_size_bytes +
                        authorization_section_bytes +
                        parameter_section_bytes;
  CHECK(command.size() == command_size) << "Command size mismatch!";
  transceiver_->SendCommand(command, parser);
}

void PolicyAuthorizeErrorCallback(
    const Tpm::PolicyAuthorizeResponse& callback,
    TPM_RC response_code) {
  callback.Run(response_code);
}

void PolicyAuthorizeResponseParser(
    const Tpm::PolicyAuthorizeResponse& callback,
    AuthorizationDelegate* authorization_delegate,
    const std::string& response) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(PolicyAuthorizeErrorCallback, callback);
  std::string buffer(response);
  TPM_ST tag;
  std::string tag_bytes;
  rc = Parse_TPM_ST(
      &buffer,
      &tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  UINT32 response_size;
  std::string response_size_bytes;
  rc = Parse_UINT32(
      &buffer,
      &response_size,
      &response_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM_RC response_code;
  std::string response_code_bytes;
  rc = Parse_TPM_RC(
      &buffer,
      &response_code,
      &response_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (response_size != response.size()) {
    error_reporter.Run(TPM_RC_SIZE);
    return;
  }
  if (response_code != TPM_RC_SUCCESS) {
    error_reporter.Run(response_code);
    return;
  }
  TPM_CC command_code = TPM_CC_PolicyAuthorize;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string authorization_section_bytes;
  if (tag == TPM_ST_SESSIONS) {
    UINT32 parameter_section_size = buffer.size();
    rc = Parse_UINT32(&buffer, &parameter_section_size, NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    if (parameter_section_size > buffer.size()) {
      error_reporter.Run(TPM_RC_INSUFFICIENT);
      return;
    }
    authorization_section_bytes = buffer.substr(parameter_section_size);
    // Keep the parameter section in |buffer|.
    buffer.erase(parameter_section_size);
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(response_code_bytes.data(),
               response_code_bytes.size());
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(buffer.data(),
               buffer.size());
  std::string response_hash(32, 0);
  hash->Finish(string_as_array(&response_hash), response_hash.size());
  if (tag == TPM_ST_SESSIONS) {
    if (!authorization_delegate->CheckResponseAuthorization(
        response_hash,
        authorization_section_bytes)) {
      error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
      return;
    }
  }
  callback.Run(response_code);
}

void Tpm::PolicyAuthorize(
      TPMI_SH_POLICY policy_session,
      const std::string& policy_session_name,
      TPM2B_DIGEST approved_policy,
      TPM2B_NONCE policy_ref,
      TPM2B_NAME key_sign,
      TPMT_TK_VERIFIED check_ticket,
      AuthorizationDelegate* authorization_delegate,
      const PolicyAuthorizeResponse& callback) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(PolicyAuthorizeErrorCallback, callback);
  base::Callback<void(const std::string&)> parser =
      base::Bind(PolicyAuthorizeResponseParser,
                 callback,
                 authorization_delegate);
  TPMI_ST_COMMAND_TAG tag = TPM_ST_NO_SESSIONS;
  UINT32 command_size = 10;  // Header size.
  std::string handle_section_bytes;
  std::string parameter_section_bytes;
  TPM_CC command_code = TPM_CC_PolicyAuthorize;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string policy_session_bytes;
  rc = Serialize_TPMI_SH_POLICY(
      policy_session,
      &policy_session_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string approved_policy_bytes;
  rc = Serialize_TPM2B_DIGEST(
      approved_policy,
      &approved_policy_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string policy_ref_bytes;
  rc = Serialize_TPM2B_NONCE(
      policy_ref,
      &policy_ref_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string key_sign_bytes;
  rc = Serialize_TPM2B_NAME(
      key_sign,
      &key_sign_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string check_ticket_bytes;
  rc = Serialize_TPMT_TK_VERIFIED(
      check_ticket,
      &check_ticket_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  // Encrypt just the parameter data, not the size.
  std::string tmp = approved_policy_bytes.substr(2);
  if (!authorization_delegate->EncryptCommandParameter(&tmp)) {
    error_reporter.Run(TRUNKS_RC_ENCRYPTION_FAILED);
    return;
  }
  approved_policy_bytes.replace(2, std::string::npos, tmp);
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(policy_session_name.data(),
               policy_session_name.size());
  handle_section_bytes += policy_session_bytes;
  command_size += policy_session_bytes.size();
  hash->Update(approved_policy_bytes.data(),
               approved_policy_bytes.size());
  parameter_section_bytes += approved_policy_bytes;
  command_size += approved_policy_bytes.size();
  hash->Update(policy_ref_bytes.data(),
               policy_ref_bytes.size());
  parameter_section_bytes += policy_ref_bytes;
  command_size += policy_ref_bytes.size();
  hash->Update(key_sign_bytes.data(),
               key_sign_bytes.size());
  parameter_section_bytes += key_sign_bytes;
  command_size += key_sign_bytes.size();
  hash->Update(check_ticket_bytes.data(),
               check_ticket_bytes.size());
  parameter_section_bytes += check_ticket_bytes;
  command_size += check_ticket_bytes.size();
  std::string command_hash(32, 0);
  hash->Finish(string_as_array(&command_hash), command_hash.size());
  std::string authorization_section_bytes;
  if (!authorization_delegate->GetCommandAuthorization(
      command_hash,
      &authorization_section_bytes)) {
    error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
    return;
  }
  std::string authorization_size_bytes;
  if (!authorization_section_bytes.empty()) {
    tag = TPM_ST_SESSIONS;
    std::string tmp;
    rc = Serialize_UINT32(authorization_section_bytes.size(),
                          &authorization_size_bytes);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    command_size += authorization_size_bytes.size() +
                    authorization_section_bytes.size();
  }
  std::string tag_bytes;
  rc = Serialize_TPMI_ST_COMMAND_TAG(
      tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command_size_bytes;
  rc = Serialize_UINT32(
      command_size,
      &command_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command = tag_bytes +
                        command_size_bytes +
                        command_code_bytes +
                        handle_section_bytes +
                        authorization_size_bytes +
                        authorization_section_bytes +
                        parameter_section_bytes;
  CHECK(command.size() == command_size) << "Command size mismatch!";
  transceiver_->SendCommand(command, parser);
}

void PolicyAuthValueErrorCallback(
    const Tpm::PolicyAuthValueResponse& callback,
    TPM_RC response_code) {
  callback.Run(response_code);
}

void PolicyAuthValueResponseParser(
    const Tpm::PolicyAuthValueResponse& callback,
    AuthorizationDelegate* authorization_delegate,
    const std::string& response) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(PolicyAuthValueErrorCallback, callback);
  std::string buffer(response);
  TPM_ST tag;
  std::string tag_bytes;
  rc = Parse_TPM_ST(
      &buffer,
      &tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  UINT32 response_size;
  std::string response_size_bytes;
  rc = Parse_UINT32(
      &buffer,
      &response_size,
      &response_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM_RC response_code;
  std::string response_code_bytes;
  rc = Parse_TPM_RC(
      &buffer,
      &response_code,
      &response_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (response_size != response.size()) {
    error_reporter.Run(TPM_RC_SIZE);
    return;
  }
  if (response_code != TPM_RC_SUCCESS) {
    error_reporter.Run(response_code);
    return;
  }
  TPM_CC command_code = TPM_CC_PolicyAuthValue;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string authorization_section_bytes;
  if (tag == TPM_ST_SESSIONS) {
    UINT32 parameter_section_size = buffer.size();
    rc = Parse_UINT32(&buffer, &parameter_section_size, NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    if (parameter_section_size > buffer.size()) {
      error_reporter.Run(TPM_RC_INSUFFICIENT);
      return;
    }
    authorization_section_bytes = buffer.substr(parameter_section_size);
    // Keep the parameter section in |buffer|.
    buffer.erase(parameter_section_size);
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(response_code_bytes.data(),
               response_code_bytes.size());
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(buffer.data(),
               buffer.size());
  std::string response_hash(32, 0);
  hash->Finish(string_as_array(&response_hash), response_hash.size());
  if (tag == TPM_ST_SESSIONS) {
    if (!authorization_delegate->CheckResponseAuthorization(
        response_hash,
        authorization_section_bytes)) {
      error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
      return;
    }
  }
  callback.Run(response_code);
}

void Tpm::PolicyAuthValue(
      TPMI_SH_POLICY policy_session,
      const std::string& policy_session_name,
      AuthorizationDelegate* authorization_delegate,
      const PolicyAuthValueResponse& callback) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(PolicyAuthValueErrorCallback, callback);
  base::Callback<void(const std::string&)> parser =
      base::Bind(PolicyAuthValueResponseParser,
                 callback,
                 authorization_delegate);
  TPMI_ST_COMMAND_TAG tag = TPM_ST_NO_SESSIONS;
  UINT32 command_size = 10;  // Header size.
  std::string handle_section_bytes;
  std::string parameter_section_bytes;
  TPM_CC command_code = TPM_CC_PolicyAuthValue;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string policy_session_bytes;
  rc = Serialize_TPMI_SH_POLICY(
      policy_session,
      &policy_session_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(policy_session_name.data(),
               policy_session_name.size());
  handle_section_bytes += policy_session_bytes;
  command_size += policy_session_bytes.size();
  std::string command_hash(32, 0);
  hash->Finish(string_as_array(&command_hash), command_hash.size());
  std::string authorization_section_bytes;
  if (!authorization_delegate->GetCommandAuthorization(
      command_hash,
      &authorization_section_bytes)) {
    error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
    return;
  }
  std::string authorization_size_bytes;
  if (!authorization_section_bytes.empty()) {
    tag = TPM_ST_SESSIONS;
    std::string tmp;
    rc = Serialize_UINT32(authorization_section_bytes.size(),
                          &authorization_size_bytes);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    command_size += authorization_size_bytes.size() +
                    authorization_section_bytes.size();
  }
  std::string tag_bytes;
  rc = Serialize_TPMI_ST_COMMAND_TAG(
      tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command_size_bytes;
  rc = Serialize_UINT32(
      command_size,
      &command_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command = tag_bytes +
                        command_size_bytes +
                        command_code_bytes +
                        handle_section_bytes +
                        authorization_size_bytes +
                        authorization_section_bytes +
                        parameter_section_bytes;
  CHECK(command.size() == command_size) << "Command size mismatch!";
  transceiver_->SendCommand(command, parser);
}

void PolicyPasswordErrorCallback(
    const Tpm::PolicyPasswordResponse& callback,
    TPM_RC response_code) {
  callback.Run(response_code);
}

void PolicyPasswordResponseParser(
    const Tpm::PolicyPasswordResponse& callback,
    AuthorizationDelegate* authorization_delegate,
    const std::string& response) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(PolicyPasswordErrorCallback, callback);
  std::string buffer(response);
  TPM_ST tag;
  std::string tag_bytes;
  rc = Parse_TPM_ST(
      &buffer,
      &tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  UINT32 response_size;
  std::string response_size_bytes;
  rc = Parse_UINT32(
      &buffer,
      &response_size,
      &response_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM_RC response_code;
  std::string response_code_bytes;
  rc = Parse_TPM_RC(
      &buffer,
      &response_code,
      &response_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (response_size != response.size()) {
    error_reporter.Run(TPM_RC_SIZE);
    return;
  }
  if (response_code != TPM_RC_SUCCESS) {
    error_reporter.Run(response_code);
    return;
  }
  TPM_CC command_code = TPM_CC_PolicyPassword;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string authorization_section_bytes;
  if (tag == TPM_ST_SESSIONS) {
    UINT32 parameter_section_size = buffer.size();
    rc = Parse_UINT32(&buffer, &parameter_section_size, NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    if (parameter_section_size > buffer.size()) {
      error_reporter.Run(TPM_RC_INSUFFICIENT);
      return;
    }
    authorization_section_bytes = buffer.substr(parameter_section_size);
    // Keep the parameter section in |buffer|.
    buffer.erase(parameter_section_size);
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(response_code_bytes.data(),
               response_code_bytes.size());
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(buffer.data(),
               buffer.size());
  std::string response_hash(32, 0);
  hash->Finish(string_as_array(&response_hash), response_hash.size());
  if (tag == TPM_ST_SESSIONS) {
    if (!authorization_delegate->CheckResponseAuthorization(
        response_hash,
        authorization_section_bytes)) {
      error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
      return;
    }
  }
  callback.Run(response_code);
}

void Tpm::PolicyPassword(
      TPMI_SH_POLICY policy_session,
      const std::string& policy_session_name,
      AuthorizationDelegate* authorization_delegate,
      const PolicyPasswordResponse& callback) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(PolicyPasswordErrorCallback, callback);
  base::Callback<void(const std::string&)> parser =
      base::Bind(PolicyPasswordResponseParser,
                 callback,
                 authorization_delegate);
  TPMI_ST_COMMAND_TAG tag = TPM_ST_NO_SESSIONS;
  UINT32 command_size = 10;  // Header size.
  std::string handle_section_bytes;
  std::string parameter_section_bytes;
  TPM_CC command_code = TPM_CC_PolicyPassword;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string policy_session_bytes;
  rc = Serialize_TPMI_SH_POLICY(
      policy_session,
      &policy_session_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(policy_session_name.data(),
               policy_session_name.size());
  handle_section_bytes += policy_session_bytes;
  command_size += policy_session_bytes.size();
  std::string command_hash(32, 0);
  hash->Finish(string_as_array(&command_hash), command_hash.size());
  std::string authorization_section_bytes;
  if (!authorization_delegate->GetCommandAuthorization(
      command_hash,
      &authorization_section_bytes)) {
    error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
    return;
  }
  std::string authorization_size_bytes;
  if (!authorization_section_bytes.empty()) {
    tag = TPM_ST_SESSIONS;
    std::string tmp;
    rc = Serialize_UINT32(authorization_section_bytes.size(),
                          &authorization_size_bytes);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    command_size += authorization_size_bytes.size() +
                    authorization_section_bytes.size();
  }
  std::string tag_bytes;
  rc = Serialize_TPMI_ST_COMMAND_TAG(
      tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command_size_bytes;
  rc = Serialize_UINT32(
      command_size,
      &command_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command = tag_bytes +
                        command_size_bytes +
                        command_code_bytes +
                        handle_section_bytes +
                        authorization_size_bytes +
                        authorization_section_bytes +
                        parameter_section_bytes;
  CHECK(command.size() == command_size) << "Command size mismatch!";
  transceiver_->SendCommand(command, parser);
}

void PolicyGetDigestErrorCallback(
    const Tpm::PolicyGetDigestResponse& callback,
    TPM_RC response_code) {
  callback.Run(response_code,
               TPM2B_DIGEST());
}

void PolicyGetDigestResponseParser(
    const Tpm::PolicyGetDigestResponse& callback,
    AuthorizationDelegate* authorization_delegate,
    const std::string& response) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(PolicyGetDigestErrorCallback, callback);
  std::string buffer(response);
  TPM_ST tag;
  std::string tag_bytes;
  rc = Parse_TPM_ST(
      &buffer,
      &tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  UINT32 response_size;
  std::string response_size_bytes;
  rc = Parse_UINT32(
      &buffer,
      &response_size,
      &response_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM_RC response_code;
  std::string response_code_bytes;
  rc = Parse_TPM_RC(
      &buffer,
      &response_code,
      &response_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (response_size != response.size()) {
    error_reporter.Run(TPM_RC_SIZE);
    return;
  }
  if (response_code != TPM_RC_SUCCESS) {
    error_reporter.Run(response_code);
    return;
  }
  TPM_CC command_code = TPM_CC_PolicyGetDigest;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string authorization_section_bytes;
  if (tag == TPM_ST_SESSIONS) {
    UINT32 parameter_section_size = buffer.size();
    rc = Parse_UINT32(&buffer, &parameter_section_size, NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    if (parameter_section_size > buffer.size()) {
      error_reporter.Run(TPM_RC_INSUFFICIENT);
      return;
    }
    authorization_section_bytes = buffer.substr(parameter_section_size);
    // Keep the parameter section in |buffer|.
    buffer.erase(parameter_section_size);
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(response_code_bytes.data(),
               response_code_bytes.size());
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(buffer.data(),
               buffer.size());
  std::string response_hash(32, 0);
  hash->Finish(string_as_array(&response_hash), response_hash.size());
  if (tag == TPM_ST_SESSIONS) {
    if (!authorization_delegate->CheckResponseAuthorization(
        response_hash,
        authorization_section_bytes)) {
      error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
      return;
    }
  }
  TPM2B_DIGEST policy_digest;
  std::string policy_digest_bytes;
  rc = Parse_TPM2B_DIGEST(
      &buffer,
      &policy_digest,
      &policy_digest_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (tag == TPM_ST_SESSIONS) {
    // Decrypt just the parameter data, not the size.
    std::string tmp = policy_digest_bytes.substr(2);
    if (!authorization_delegate->DecryptResponseParameter(&tmp)) {
      error_reporter.Run(TRUNKS_RC_ENCRYPTION_FAILED);
      return;
    }
    policy_digest_bytes.replace(2, std::string::npos, tmp);
    rc = Parse_TPM2B_DIGEST(
        &policy_digest_bytes,
        &policy_digest,
        NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
  }
  callback.Run(response_code,
               policy_digest);
}

void Tpm::PolicyGetDigest(
      TPMI_SH_POLICY policy_session,
      const std::string& policy_session_name,
      AuthorizationDelegate* authorization_delegate,
      const PolicyGetDigestResponse& callback) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(PolicyGetDigestErrorCallback, callback);
  base::Callback<void(const std::string&)> parser =
      base::Bind(PolicyGetDigestResponseParser,
                 callback,
                 authorization_delegate);
  TPMI_ST_COMMAND_TAG tag = TPM_ST_NO_SESSIONS;
  UINT32 command_size = 10;  // Header size.
  std::string handle_section_bytes;
  std::string parameter_section_bytes;
  TPM_CC command_code = TPM_CC_PolicyGetDigest;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string policy_session_bytes;
  rc = Serialize_TPMI_SH_POLICY(
      policy_session,
      &policy_session_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(policy_session_name.data(),
               policy_session_name.size());
  handle_section_bytes += policy_session_bytes;
  command_size += policy_session_bytes.size();
  std::string command_hash(32, 0);
  hash->Finish(string_as_array(&command_hash), command_hash.size());
  std::string authorization_section_bytes;
  if (!authorization_delegate->GetCommandAuthorization(
      command_hash,
      &authorization_section_bytes)) {
    error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
    return;
  }
  std::string authorization_size_bytes;
  if (!authorization_section_bytes.empty()) {
    tag = TPM_ST_SESSIONS;
    std::string tmp;
    rc = Serialize_UINT32(authorization_section_bytes.size(),
                          &authorization_size_bytes);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    command_size += authorization_size_bytes.size() +
                    authorization_section_bytes.size();
  }
  std::string tag_bytes;
  rc = Serialize_TPMI_ST_COMMAND_TAG(
      tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command_size_bytes;
  rc = Serialize_UINT32(
      command_size,
      &command_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command = tag_bytes +
                        command_size_bytes +
                        command_code_bytes +
                        handle_section_bytes +
                        authorization_size_bytes +
                        authorization_section_bytes +
                        parameter_section_bytes;
  CHECK(command.size() == command_size) << "Command size mismatch!";
  transceiver_->SendCommand(command, parser);
}

void PolicyNvWrittenErrorCallback(
    const Tpm::PolicyNvWrittenResponse& callback,
    TPM_RC response_code) {
  callback.Run(response_code);
}

void PolicyNvWrittenResponseParser(
    const Tpm::PolicyNvWrittenResponse& callback,
    AuthorizationDelegate* authorization_delegate,
    const std::string& response) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(PolicyNvWrittenErrorCallback, callback);
  std::string buffer(response);
  TPM_ST tag;
  std::string tag_bytes;
  rc = Parse_TPM_ST(
      &buffer,
      &tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  UINT32 response_size;
  std::string response_size_bytes;
  rc = Parse_UINT32(
      &buffer,
      &response_size,
      &response_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM_RC response_code;
  std::string response_code_bytes;
  rc = Parse_TPM_RC(
      &buffer,
      &response_code,
      &response_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (response_size != response.size()) {
    error_reporter.Run(TPM_RC_SIZE);
    return;
  }
  if (response_code != TPM_RC_SUCCESS) {
    error_reporter.Run(response_code);
    return;
  }
  TPM_CC command_code = TPM_CC_PolicyNvWritten;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string authorization_section_bytes;
  if (tag == TPM_ST_SESSIONS) {
    UINT32 parameter_section_size = buffer.size();
    rc = Parse_UINT32(&buffer, &parameter_section_size, NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    if (parameter_section_size > buffer.size()) {
      error_reporter.Run(TPM_RC_INSUFFICIENT);
      return;
    }
    authorization_section_bytes = buffer.substr(parameter_section_size);
    // Keep the parameter section in |buffer|.
    buffer.erase(parameter_section_size);
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(response_code_bytes.data(),
               response_code_bytes.size());
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(buffer.data(),
               buffer.size());
  std::string response_hash(32, 0);
  hash->Finish(string_as_array(&response_hash), response_hash.size());
  if (tag == TPM_ST_SESSIONS) {
    if (!authorization_delegate->CheckResponseAuthorization(
        response_hash,
        authorization_section_bytes)) {
      error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
      return;
    }
  }
  callback.Run(response_code);
}

void Tpm::PolicyNvWritten(
      TPMI_SH_POLICY policy_session,
      const std::string& policy_session_name,
      TPMI_YES_NO written_set,
      AuthorizationDelegate* authorization_delegate,
      const PolicyNvWrittenResponse& callback) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(PolicyNvWrittenErrorCallback, callback);
  base::Callback<void(const std::string&)> parser =
      base::Bind(PolicyNvWrittenResponseParser,
                 callback,
                 authorization_delegate);
  TPMI_ST_COMMAND_TAG tag = TPM_ST_NO_SESSIONS;
  UINT32 command_size = 10;  // Header size.
  std::string handle_section_bytes;
  std::string parameter_section_bytes;
  TPM_CC command_code = TPM_CC_PolicyNvWritten;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string policy_session_bytes;
  rc = Serialize_TPMI_SH_POLICY(
      policy_session,
      &policy_session_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string written_set_bytes;
  rc = Serialize_TPMI_YES_NO(
      written_set,
      &written_set_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(policy_session_name.data(),
               policy_session_name.size());
  handle_section_bytes += policy_session_bytes;
  command_size += policy_session_bytes.size();
  hash->Update(written_set_bytes.data(),
               written_set_bytes.size());
  parameter_section_bytes += written_set_bytes;
  command_size += written_set_bytes.size();
  std::string command_hash(32, 0);
  hash->Finish(string_as_array(&command_hash), command_hash.size());
  std::string authorization_section_bytes;
  if (!authorization_delegate->GetCommandAuthorization(
      command_hash,
      &authorization_section_bytes)) {
    error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
    return;
  }
  std::string authorization_size_bytes;
  if (!authorization_section_bytes.empty()) {
    tag = TPM_ST_SESSIONS;
    std::string tmp;
    rc = Serialize_UINT32(authorization_section_bytes.size(),
                          &authorization_size_bytes);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    command_size += authorization_size_bytes.size() +
                    authorization_section_bytes.size();
  }
  std::string tag_bytes;
  rc = Serialize_TPMI_ST_COMMAND_TAG(
      tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command_size_bytes;
  rc = Serialize_UINT32(
      command_size,
      &command_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command = tag_bytes +
                        command_size_bytes +
                        command_code_bytes +
                        handle_section_bytes +
                        authorization_size_bytes +
                        authorization_section_bytes +
                        parameter_section_bytes;
  CHECK(command.size() == command_size) << "Command size mismatch!";
  transceiver_->SendCommand(command, parser);
}

void CreatePrimaryErrorCallback(
    const Tpm::CreatePrimaryResponse& callback,
    TPM_RC response_code) {
  callback.Run(response_code,
               TPM_HANDLE(),
               TPM2B_PUBLIC(),
               TPM2B_CREATION_DATA(),
               TPM2B_DIGEST(),
               TPMT_TK_CREATION(),
               TPM2B_NAME());
}

void CreatePrimaryResponseParser(
    const Tpm::CreatePrimaryResponse& callback,
    AuthorizationDelegate* authorization_delegate,
    const std::string& response) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(CreatePrimaryErrorCallback, callback);
  std::string buffer(response);
  TPM_ST tag;
  std::string tag_bytes;
  rc = Parse_TPM_ST(
      &buffer,
      &tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  UINT32 response_size;
  std::string response_size_bytes;
  rc = Parse_UINT32(
      &buffer,
      &response_size,
      &response_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM_RC response_code;
  std::string response_code_bytes;
  rc = Parse_TPM_RC(
      &buffer,
      &response_code,
      &response_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (response_size != response.size()) {
    error_reporter.Run(TPM_RC_SIZE);
    return;
  }
  if (response_code != TPM_RC_SUCCESS) {
    error_reporter.Run(response_code);
    return;
  }
  TPM_CC command_code = TPM_CC_CreatePrimary;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string authorization_section_bytes;
  if (tag == TPM_ST_SESSIONS) {
    UINT32 parameter_section_size = buffer.size();
    rc = Parse_UINT32(&buffer, &parameter_section_size, NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    if (parameter_section_size > buffer.size()) {
      error_reporter.Run(TPM_RC_INSUFFICIENT);
      return;
    }
    authorization_section_bytes = buffer.substr(parameter_section_size);
    // Keep the parameter section in |buffer|.
    buffer.erase(parameter_section_size);
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(response_code_bytes.data(),
               response_code_bytes.size());
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(buffer.data(),
               buffer.size());
  std::string response_hash(32, 0);
  hash->Finish(string_as_array(&response_hash), response_hash.size());
  if (tag == TPM_ST_SESSIONS) {
    if (!authorization_delegate->CheckResponseAuthorization(
        response_hash,
        authorization_section_bytes)) {
      error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
      return;
    }
  }
  TPM_HANDLE object_handle;
  std::string object_handle_bytes;
  rc = Parse_TPM_HANDLE(
      &buffer,
      &object_handle,
      &object_handle_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM2B_PUBLIC out_public;
  std::string out_public_bytes;
  rc = Parse_TPM2B_PUBLIC(
      &buffer,
      &out_public,
      &out_public_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM2B_CREATION_DATA creation_data;
  std::string creation_data_bytes;
  rc = Parse_TPM2B_CREATION_DATA(
      &buffer,
      &creation_data,
      &creation_data_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM2B_DIGEST creation_hash;
  std::string creation_hash_bytes;
  rc = Parse_TPM2B_DIGEST(
      &buffer,
      &creation_hash,
      &creation_hash_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPMT_TK_CREATION creation_ticket;
  std::string creation_ticket_bytes;
  rc = Parse_TPMT_TK_CREATION(
      &buffer,
      &creation_ticket,
      &creation_ticket_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM2B_NAME name;
  std::string name_bytes;
  rc = Parse_TPM2B_NAME(
      &buffer,
      &name,
      &name_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (tag == TPM_ST_SESSIONS) {
    // Decrypt just the parameter data, not the size.
    std::string tmp = object_handle_bytes.substr(2);
    if (!authorization_delegate->DecryptResponseParameter(&tmp)) {
      error_reporter.Run(TRUNKS_RC_ENCRYPTION_FAILED);
      return;
    }
    object_handle_bytes.replace(2, std::string::npos, tmp);
    rc = Parse_TPM_HANDLE(
        &object_handle_bytes,
        &object_handle,
        NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
  }
  callback.Run(response_code,
               object_handle,
               out_public,
               creation_data,
               creation_hash,
               creation_ticket,
               name);
}

void Tpm::CreatePrimary(
      TPMI_RH_HIERARCHY primary_handle,
      const std::string& primary_handle_name,
      TPM2B_SENSITIVE_CREATE in_sensitive,
      TPM2B_PUBLIC in_public,
      TPM2B_DATA outside_info,
      TPML_PCR_SELECTION creation_pcr,
      AuthorizationDelegate* authorization_delegate,
      const CreatePrimaryResponse& callback) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(CreatePrimaryErrorCallback, callback);
  base::Callback<void(const std::string&)> parser =
      base::Bind(CreatePrimaryResponseParser,
                 callback,
                 authorization_delegate);
  TPMI_ST_COMMAND_TAG tag = TPM_ST_NO_SESSIONS;
  UINT32 command_size = 10;  // Header size.
  std::string handle_section_bytes;
  std::string parameter_section_bytes;
  TPM_CC command_code = TPM_CC_CreatePrimary;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string primary_handle_bytes;
  rc = Serialize_TPMI_RH_HIERARCHY(
      primary_handle,
      &primary_handle_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string in_sensitive_bytes;
  rc = Serialize_TPM2B_SENSITIVE_CREATE(
      in_sensitive,
      &in_sensitive_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string in_public_bytes;
  rc = Serialize_TPM2B_PUBLIC(
      in_public,
      &in_public_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string outside_info_bytes;
  rc = Serialize_TPM2B_DATA(
      outside_info,
      &outside_info_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string creation_pcr_bytes;
  rc = Serialize_TPML_PCR_SELECTION(
      creation_pcr,
      &creation_pcr_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  // Encrypt just the parameter data, not the size.
  std::string tmp = in_sensitive_bytes.substr(2);
  if (!authorization_delegate->EncryptCommandParameter(&tmp)) {
    error_reporter.Run(TRUNKS_RC_ENCRYPTION_FAILED);
    return;
  }
  in_sensitive_bytes.replace(2, std::string::npos, tmp);
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(primary_handle_name.data(),
               primary_handle_name.size());
  handle_section_bytes += primary_handle_bytes;
  command_size += primary_handle_bytes.size();
  hash->Update(in_sensitive_bytes.data(),
               in_sensitive_bytes.size());
  parameter_section_bytes += in_sensitive_bytes;
  command_size += in_sensitive_bytes.size();
  hash->Update(in_public_bytes.data(),
               in_public_bytes.size());
  parameter_section_bytes += in_public_bytes;
  command_size += in_public_bytes.size();
  hash->Update(outside_info_bytes.data(),
               outside_info_bytes.size());
  parameter_section_bytes += outside_info_bytes;
  command_size += outside_info_bytes.size();
  hash->Update(creation_pcr_bytes.data(),
               creation_pcr_bytes.size());
  parameter_section_bytes += creation_pcr_bytes;
  command_size += creation_pcr_bytes.size();
  std::string command_hash(32, 0);
  hash->Finish(string_as_array(&command_hash), command_hash.size());
  std::string authorization_section_bytes;
  if (!authorization_delegate->GetCommandAuthorization(
      command_hash,
      &authorization_section_bytes)) {
    error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
    return;
  }
  std::string authorization_size_bytes;
  if (!authorization_section_bytes.empty()) {
    tag = TPM_ST_SESSIONS;
    std::string tmp;
    rc = Serialize_UINT32(authorization_section_bytes.size(),
                          &authorization_size_bytes);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    command_size += authorization_size_bytes.size() +
                    authorization_section_bytes.size();
  }
  std::string tag_bytes;
  rc = Serialize_TPMI_ST_COMMAND_TAG(
      tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command_size_bytes;
  rc = Serialize_UINT32(
      command_size,
      &command_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command = tag_bytes +
                        command_size_bytes +
                        command_code_bytes +
                        handle_section_bytes +
                        authorization_size_bytes +
                        authorization_section_bytes +
                        parameter_section_bytes;
  CHECK(command.size() == command_size) << "Command size mismatch!";
  transceiver_->SendCommand(command, parser);
}

void HierarchyControlErrorCallback(
    const Tpm::HierarchyControlResponse& callback,
    TPM_RC response_code) {
  callback.Run(response_code);
}

void HierarchyControlResponseParser(
    const Tpm::HierarchyControlResponse& callback,
    AuthorizationDelegate* authorization_delegate,
    const std::string& response) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(HierarchyControlErrorCallback, callback);
  std::string buffer(response);
  TPM_ST tag;
  std::string tag_bytes;
  rc = Parse_TPM_ST(
      &buffer,
      &tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  UINT32 response_size;
  std::string response_size_bytes;
  rc = Parse_UINT32(
      &buffer,
      &response_size,
      &response_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM_RC response_code;
  std::string response_code_bytes;
  rc = Parse_TPM_RC(
      &buffer,
      &response_code,
      &response_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (response_size != response.size()) {
    error_reporter.Run(TPM_RC_SIZE);
    return;
  }
  if (response_code != TPM_RC_SUCCESS) {
    error_reporter.Run(response_code);
    return;
  }
  TPM_CC command_code = TPM_CC_HierarchyControl;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string authorization_section_bytes;
  if (tag == TPM_ST_SESSIONS) {
    UINT32 parameter_section_size = buffer.size();
    rc = Parse_UINT32(&buffer, &parameter_section_size, NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    if (parameter_section_size > buffer.size()) {
      error_reporter.Run(TPM_RC_INSUFFICIENT);
      return;
    }
    authorization_section_bytes = buffer.substr(parameter_section_size);
    // Keep the parameter section in |buffer|.
    buffer.erase(parameter_section_size);
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(response_code_bytes.data(),
               response_code_bytes.size());
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(buffer.data(),
               buffer.size());
  std::string response_hash(32, 0);
  hash->Finish(string_as_array(&response_hash), response_hash.size());
  if (tag == TPM_ST_SESSIONS) {
    if (!authorization_delegate->CheckResponseAuthorization(
        response_hash,
        authorization_section_bytes)) {
      error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
      return;
    }
  }
  callback.Run(response_code);
}

void Tpm::HierarchyControl(
      TPMI_RH_HIERARCHY auth_handle,
      const std::string& auth_handle_name,
      TPMI_RH_ENABLES enable,
      const std::string& enable_name,
      TPMI_YES_NO state,
      AuthorizationDelegate* authorization_delegate,
      const HierarchyControlResponse& callback) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(HierarchyControlErrorCallback, callback);
  base::Callback<void(const std::string&)> parser =
      base::Bind(HierarchyControlResponseParser,
                 callback,
                 authorization_delegate);
  TPMI_ST_COMMAND_TAG tag = TPM_ST_NO_SESSIONS;
  UINT32 command_size = 10;  // Header size.
  std::string handle_section_bytes;
  std::string parameter_section_bytes;
  TPM_CC command_code = TPM_CC_HierarchyControl;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string auth_handle_bytes;
  rc = Serialize_TPMI_RH_HIERARCHY(
      auth_handle,
      &auth_handle_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string enable_bytes;
  rc = Serialize_TPMI_RH_ENABLES(
      enable,
      &enable_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string state_bytes;
  rc = Serialize_TPMI_YES_NO(
      state,
      &state_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(auth_handle_name.data(),
               auth_handle_name.size());
  handle_section_bytes += auth_handle_bytes;
  command_size += auth_handle_bytes.size();
  hash->Update(enable_name.data(),
               enable_name.size());
  handle_section_bytes += enable_bytes;
  command_size += enable_bytes.size();
  hash->Update(state_bytes.data(),
               state_bytes.size());
  parameter_section_bytes += state_bytes;
  command_size += state_bytes.size();
  std::string command_hash(32, 0);
  hash->Finish(string_as_array(&command_hash), command_hash.size());
  std::string authorization_section_bytes;
  if (!authorization_delegate->GetCommandAuthorization(
      command_hash,
      &authorization_section_bytes)) {
    error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
    return;
  }
  std::string authorization_size_bytes;
  if (!authorization_section_bytes.empty()) {
    tag = TPM_ST_SESSIONS;
    std::string tmp;
    rc = Serialize_UINT32(authorization_section_bytes.size(),
                          &authorization_size_bytes);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    command_size += authorization_size_bytes.size() +
                    authorization_section_bytes.size();
  }
  std::string tag_bytes;
  rc = Serialize_TPMI_ST_COMMAND_TAG(
      tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command_size_bytes;
  rc = Serialize_UINT32(
      command_size,
      &command_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command = tag_bytes +
                        command_size_bytes +
                        command_code_bytes +
                        handle_section_bytes +
                        authorization_size_bytes +
                        authorization_section_bytes +
                        parameter_section_bytes;
  CHECK(command.size() == command_size) << "Command size mismatch!";
  transceiver_->SendCommand(command, parser);
}

void SetPrimaryPolicyErrorCallback(
    const Tpm::SetPrimaryPolicyResponse& callback,
    TPM_RC response_code) {
  callback.Run(response_code);
}

void SetPrimaryPolicyResponseParser(
    const Tpm::SetPrimaryPolicyResponse& callback,
    AuthorizationDelegate* authorization_delegate,
    const std::string& response) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(SetPrimaryPolicyErrorCallback, callback);
  std::string buffer(response);
  TPM_ST tag;
  std::string tag_bytes;
  rc = Parse_TPM_ST(
      &buffer,
      &tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  UINT32 response_size;
  std::string response_size_bytes;
  rc = Parse_UINT32(
      &buffer,
      &response_size,
      &response_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM_RC response_code;
  std::string response_code_bytes;
  rc = Parse_TPM_RC(
      &buffer,
      &response_code,
      &response_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (response_size != response.size()) {
    error_reporter.Run(TPM_RC_SIZE);
    return;
  }
  if (response_code != TPM_RC_SUCCESS) {
    error_reporter.Run(response_code);
    return;
  }
  TPM_CC command_code = TPM_CC_SetPrimaryPolicy;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string authorization_section_bytes;
  if (tag == TPM_ST_SESSIONS) {
    UINT32 parameter_section_size = buffer.size();
    rc = Parse_UINT32(&buffer, &parameter_section_size, NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    if (parameter_section_size > buffer.size()) {
      error_reporter.Run(TPM_RC_INSUFFICIENT);
      return;
    }
    authorization_section_bytes = buffer.substr(parameter_section_size);
    // Keep the parameter section in |buffer|.
    buffer.erase(parameter_section_size);
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(response_code_bytes.data(),
               response_code_bytes.size());
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(buffer.data(),
               buffer.size());
  std::string response_hash(32, 0);
  hash->Finish(string_as_array(&response_hash), response_hash.size());
  if (tag == TPM_ST_SESSIONS) {
    if (!authorization_delegate->CheckResponseAuthorization(
        response_hash,
        authorization_section_bytes)) {
      error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
      return;
    }
  }
  callback.Run(response_code);
}

void Tpm::SetPrimaryPolicy(
      TPMI_RH_HIERARCHY auth_handle,
      const std::string& auth_handle_name,
      TPM2B_DIGEST auth_policy,
      TPMI_ALG_HASH hash_alg,
      AuthorizationDelegate* authorization_delegate,
      const SetPrimaryPolicyResponse& callback) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(SetPrimaryPolicyErrorCallback, callback);
  base::Callback<void(const std::string&)> parser =
      base::Bind(SetPrimaryPolicyResponseParser,
                 callback,
                 authorization_delegate);
  TPMI_ST_COMMAND_TAG tag = TPM_ST_NO_SESSIONS;
  UINT32 command_size = 10;  // Header size.
  std::string handle_section_bytes;
  std::string parameter_section_bytes;
  TPM_CC command_code = TPM_CC_SetPrimaryPolicy;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string auth_handle_bytes;
  rc = Serialize_TPMI_RH_HIERARCHY(
      auth_handle,
      &auth_handle_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string auth_policy_bytes;
  rc = Serialize_TPM2B_DIGEST(
      auth_policy,
      &auth_policy_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string hash_alg_bytes;
  rc = Serialize_TPMI_ALG_HASH(
      hash_alg,
      &hash_alg_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  // Encrypt just the parameter data, not the size.
  std::string tmp = auth_policy_bytes.substr(2);
  if (!authorization_delegate->EncryptCommandParameter(&tmp)) {
    error_reporter.Run(TRUNKS_RC_ENCRYPTION_FAILED);
    return;
  }
  auth_policy_bytes.replace(2, std::string::npos, tmp);
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(auth_handle_name.data(),
               auth_handle_name.size());
  handle_section_bytes += auth_handle_bytes;
  command_size += auth_handle_bytes.size();
  hash->Update(auth_policy_bytes.data(),
               auth_policy_bytes.size());
  parameter_section_bytes += auth_policy_bytes;
  command_size += auth_policy_bytes.size();
  hash->Update(hash_alg_bytes.data(),
               hash_alg_bytes.size());
  parameter_section_bytes += hash_alg_bytes;
  command_size += hash_alg_bytes.size();
  std::string command_hash(32, 0);
  hash->Finish(string_as_array(&command_hash), command_hash.size());
  std::string authorization_section_bytes;
  if (!authorization_delegate->GetCommandAuthorization(
      command_hash,
      &authorization_section_bytes)) {
    error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
    return;
  }
  std::string authorization_size_bytes;
  if (!authorization_section_bytes.empty()) {
    tag = TPM_ST_SESSIONS;
    std::string tmp;
    rc = Serialize_UINT32(authorization_section_bytes.size(),
                          &authorization_size_bytes);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    command_size += authorization_size_bytes.size() +
                    authorization_section_bytes.size();
  }
  std::string tag_bytes;
  rc = Serialize_TPMI_ST_COMMAND_TAG(
      tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command_size_bytes;
  rc = Serialize_UINT32(
      command_size,
      &command_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command = tag_bytes +
                        command_size_bytes +
                        command_code_bytes +
                        handle_section_bytes +
                        authorization_size_bytes +
                        authorization_section_bytes +
                        parameter_section_bytes;
  CHECK(command.size() == command_size) << "Command size mismatch!";
  transceiver_->SendCommand(command, parser);
}

void ChangePPSErrorCallback(
    const Tpm::ChangePPSResponse& callback,
    TPM_RC response_code) {
  callback.Run(response_code);
}

void ChangePPSResponseParser(
    const Tpm::ChangePPSResponse& callback,
    AuthorizationDelegate* authorization_delegate,
    const std::string& response) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(ChangePPSErrorCallback, callback);
  std::string buffer(response);
  TPM_ST tag;
  std::string tag_bytes;
  rc = Parse_TPM_ST(
      &buffer,
      &tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  UINT32 response_size;
  std::string response_size_bytes;
  rc = Parse_UINT32(
      &buffer,
      &response_size,
      &response_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM_RC response_code;
  std::string response_code_bytes;
  rc = Parse_TPM_RC(
      &buffer,
      &response_code,
      &response_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (response_size != response.size()) {
    error_reporter.Run(TPM_RC_SIZE);
    return;
  }
  if (response_code != TPM_RC_SUCCESS) {
    error_reporter.Run(response_code);
    return;
  }
  TPM_CC command_code = TPM_CC_ChangePPS;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string authorization_section_bytes;
  if (tag == TPM_ST_SESSIONS) {
    UINT32 parameter_section_size = buffer.size();
    rc = Parse_UINT32(&buffer, &parameter_section_size, NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    if (parameter_section_size > buffer.size()) {
      error_reporter.Run(TPM_RC_INSUFFICIENT);
      return;
    }
    authorization_section_bytes = buffer.substr(parameter_section_size);
    // Keep the parameter section in |buffer|.
    buffer.erase(parameter_section_size);
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(response_code_bytes.data(),
               response_code_bytes.size());
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(buffer.data(),
               buffer.size());
  std::string response_hash(32, 0);
  hash->Finish(string_as_array(&response_hash), response_hash.size());
  if (tag == TPM_ST_SESSIONS) {
    if (!authorization_delegate->CheckResponseAuthorization(
        response_hash,
        authorization_section_bytes)) {
      error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
      return;
    }
  }
  callback.Run(response_code);
}

void Tpm::ChangePPS(
      TPMI_RH_PLATFORM auth_handle,
      const std::string& auth_handle_name,
      AuthorizationDelegate* authorization_delegate,
      const ChangePPSResponse& callback) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(ChangePPSErrorCallback, callback);
  base::Callback<void(const std::string&)> parser =
      base::Bind(ChangePPSResponseParser,
                 callback,
                 authorization_delegate);
  TPMI_ST_COMMAND_TAG tag = TPM_ST_NO_SESSIONS;
  UINT32 command_size = 10;  // Header size.
  std::string handle_section_bytes;
  std::string parameter_section_bytes;
  TPM_CC command_code = TPM_CC_ChangePPS;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string auth_handle_bytes;
  rc = Serialize_TPMI_RH_PLATFORM(
      auth_handle,
      &auth_handle_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(auth_handle_name.data(),
               auth_handle_name.size());
  handle_section_bytes += auth_handle_bytes;
  command_size += auth_handle_bytes.size();
  std::string command_hash(32, 0);
  hash->Finish(string_as_array(&command_hash), command_hash.size());
  std::string authorization_section_bytes;
  if (!authorization_delegate->GetCommandAuthorization(
      command_hash,
      &authorization_section_bytes)) {
    error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
    return;
  }
  std::string authorization_size_bytes;
  if (!authorization_section_bytes.empty()) {
    tag = TPM_ST_SESSIONS;
    std::string tmp;
    rc = Serialize_UINT32(authorization_section_bytes.size(),
                          &authorization_size_bytes);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    command_size += authorization_size_bytes.size() +
                    authorization_section_bytes.size();
  }
  std::string tag_bytes;
  rc = Serialize_TPMI_ST_COMMAND_TAG(
      tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command_size_bytes;
  rc = Serialize_UINT32(
      command_size,
      &command_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command = tag_bytes +
                        command_size_bytes +
                        command_code_bytes +
                        handle_section_bytes +
                        authorization_size_bytes +
                        authorization_section_bytes +
                        parameter_section_bytes;
  CHECK(command.size() == command_size) << "Command size mismatch!";
  transceiver_->SendCommand(command, parser);
}

void ChangeEPSErrorCallback(
    const Tpm::ChangeEPSResponse& callback,
    TPM_RC response_code) {
  callback.Run(response_code);
}

void ChangeEPSResponseParser(
    const Tpm::ChangeEPSResponse& callback,
    AuthorizationDelegate* authorization_delegate,
    const std::string& response) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(ChangeEPSErrorCallback, callback);
  std::string buffer(response);
  TPM_ST tag;
  std::string tag_bytes;
  rc = Parse_TPM_ST(
      &buffer,
      &tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  UINT32 response_size;
  std::string response_size_bytes;
  rc = Parse_UINT32(
      &buffer,
      &response_size,
      &response_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM_RC response_code;
  std::string response_code_bytes;
  rc = Parse_TPM_RC(
      &buffer,
      &response_code,
      &response_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (response_size != response.size()) {
    error_reporter.Run(TPM_RC_SIZE);
    return;
  }
  if (response_code != TPM_RC_SUCCESS) {
    error_reporter.Run(response_code);
    return;
  }
  TPM_CC command_code = TPM_CC_ChangeEPS;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string authorization_section_bytes;
  if (tag == TPM_ST_SESSIONS) {
    UINT32 parameter_section_size = buffer.size();
    rc = Parse_UINT32(&buffer, &parameter_section_size, NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    if (parameter_section_size > buffer.size()) {
      error_reporter.Run(TPM_RC_INSUFFICIENT);
      return;
    }
    authorization_section_bytes = buffer.substr(parameter_section_size);
    // Keep the parameter section in |buffer|.
    buffer.erase(parameter_section_size);
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(response_code_bytes.data(),
               response_code_bytes.size());
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(buffer.data(),
               buffer.size());
  std::string response_hash(32, 0);
  hash->Finish(string_as_array(&response_hash), response_hash.size());
  if (tag == TPM_ST_SESSIONS) {
    if (!authorization_delegate->CheckResponseAuthorization(
        response_hash,
        authorization_section_bytes)) {
      error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
      return;
    }
  }
  callback.Run(response_code);
}

void Tpm::ChangeEPS(
      TPMI_RH_PLATFORM auth_handle,
      const std::string& auth_handle_name,
      AuthorizationDelegate* authorization_delegate,
      const ChangeEPSResponse& callback) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(ChangeEPSErrorCallback, callback);
  base::Callback<void(const std::string&)> parser =
      base::Bind(ChangeEPSResponseParser,
                 callback,
                 authorization_delegate);
  TPMI_ST_COMMAND_TAG tag = TPM_ST_NO_SESSIONS;
  UINT32 command_size = 10;  // Header size.
  std::string handle_section_bytes;
  std::string parameter_section_bytes;
  TPM_CC command_code = TPM_CC_ChangeEPS;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string auth_handle_bytes;
  rc = Serialize_TPMI_RH_PLATFORM(
      auth_handle,
      &auth_handle_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(auth_handle_name.data(),
               auth_handle_name.size());
  handle_section_bytes += auth_handle_bytes;
  command_size += auth_handle_bytes.size();
  std::string command_hash(32, 0);
  hash->Finish(string_as_array(&command_hash), command_hash.size());
  std::string authorization_section_bytes;
  if (!authorization_delegate->GetCommandAuthorization(
      command_hash,
      &authorization_section_bytes)) {
    error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
    return;
  }
  std::string authorization_size_bytes;
  if (!authorization_section_bytes.empty()) {
    tag = TPM_ST_SESSIONS;
    std::string tmp;
    rc = Serialize_UINT32(authorization_section_bytes.size(),
                          &authorization_size_bytes);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    command_size += authorization_size_bytes.size() +
                    authorization_section_bytes.size();
  }
  std::string tag_bytes;
  rc = Serialize_TPMI_ST_COMMAND_TAG(
      tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command_size_bytes;
  rc = Serialize_UINT32(
      command_size,
      &command_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command = tag_bytes +
                        command_size_bytes +
                        command_code_bytes +
                        handle_section_bytes +
                        authorization_size_bytes +
                        authorization_section_bytes +
                        parameter_section_bytes;
  CHECK(command.size() == command_size) << "Command size mismatch!";
  transceiver_->SendCommand(command, parser);
}

void ClearErrorCallback(
    const Tpm::ClearResponse& callback,
    TPM_RC response_code) {
  callback.Run(response_code);
}

void ClearResponseParser(
    const Tpm::ClearResponse& callback,
    AuthorizationDelegate* authorization_delegate,
    const std::string& response) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(ClearErrorCallback, callback);
  std::string buffer(response);
  TPM_ST tag;
  std::string tag_bytes;
  rc = Parse_TPM_ST(
      &buffer,
      &tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  UINT32 response_size;
  std::string response_size_bytes;
  rc = Parse_UINT32(
      &buffer,
      &response_size,
      &response_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM_RC response_code;
  std::string response_code_bytes;
  rc = Parse_TPM_RC(
      &buffer,
      &response_code,
      &response_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (response_size != response.size()) {
    error_reporter.Run(TPM_RC_SIZE);
    return;
  }
  if (response_code != TPM_RC_SUCCESS) {
    error_reporter.Run(response_code);
    return;
  }
  TPM_CC command_code = TPM_CC_Clear;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string authorization_section_bytes;
  if (tag == TPM_ST_SESSIONS) {
    UINT32 parameter_section_size = buffer.size();
    rc = Parse_UINT32(&buffer, &parameter_section_size, NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    if (parameter_section_size > buffer.size()) {
      error_reporter.Run(TPM_RC_INSUFFICIENT);
      return;
    }
    authorization_section_bytes = buffer.substr(parameter_section_size);
    // Keep the parameter section in |buffer|.
    buffer.erase(parameter_section_size);
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(response_code_bytes.data(),
               response_code_bytes.size());
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(buffer.data(),
               buffer.size());
  std::string response_hash(32, 0);
  hash->Finish(string_as_array(&response_hash), response_hash.size());
  if (tag == TPM_ST_SESSIONS) {
    if (!authorization_delegate->CheckResponseAuthorization(
        response_hash,
        authorization_section_bytes)) {
      error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
      return;
    }
  }
  callback.Run(response_code);
}

void Tpm::Clear(
      TPMI_RH_CLEAR auth_handle,
      const std::string& auth_handle_name,
      AuthorizationDelegate* authorization_delegate,
      const ClearResponse& callback) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(ClearErrorCallback, callback);
  base::Callback<void(const std::string&)> parser =
      base::Bind(ClearResponseParser,
                 callback,
                 authorization_delegate);
  TPMI_ST_COMMAND_TAG tag = TPM_ST_NO_SESSIONS;
  UINT32 command_size = 10;  // Header size.
  std::string handle_section_bytes;
  std::string parameter_section_bytes;
  TPM_CC command_code = TPM_CC_Clear;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string auth_handle_bytes;
  rc = Serialize_TPMI_RH_CLEAR(
      auth_handle,
      &auth_handle_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(auth_handle_name.data(),
               auth_handle_name.size());
  handle_section_bytes += auth_handle_bytes;
  command_size += auth_handle_bytes.size();
  std::string command_hash(32, 0);
  hash->Finish(string_as_array(&command_hash), command_hash.size());
  std::string authorization_section_bytes;
  if (!authorization_delegate->GetCommandAuthorization(
      command_hash,
      &authorization_section_bytes)) {
    error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
    return;
  }
  std::string authorization_size_bytes;
  if (!authorization_section_bytes.empty()) {
    tag = TPM_ST_SESSIONS;
    std::string tmp;
    rc = Serialize_UINT32(authorization_section_bytes.size(),
                          &authorization_size_bytes);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    command_size += authorization_size_bytes.size() +
                    authorization_section_bytes.size();
  }
  std::string tag_bytes;
  rc = Serialize_TPMI_ST_COMMAND_TAG(
      tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command_size_bytes;
  rc = Serialize_UINT32(
      command_size,
      &command_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command = tag_bytes +
                        command_size_bytes +
                        command_code_bytes +
                        handle_section_bytes +
                        authorization_size_bytes +
                        authorization_section_bytes +
                        parameter_section_bytes;
  CHECK(command.size() == command_size) << "Command size mismatch!";
  transceiver_->SendCommand(command, parser);
}

void ClearControlErrorCallback(
    const Tpm::ClearControlResponse& callback,
    TPM_RC response_code) {
  callback.Run(response_code);
}

void ClearControlResponseParser(
    const Tpm::ClearControlResponse& callback,
    AuthorizationDelegate* authorization_delegate,
    const std::string& response) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(ClearControlErrorCallback, callback);
  std::string buffer(response);
  TPM_ST tag;
  std::string tag_bytes;
  rc = Parse_TPM_ST(
      &buffer,
      &tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  UINT32 response_size;
  std::string response_size_bytes;
  rc = Parse_UINT32(
      &buffer,
      &response_size,
      &response_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM_RC response_code;
  std::string response_code_bytes;
  rc = Parse_TPM_RC(
      &buffer,
      &response_code,
      &response_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (response_size != response.size()) {
    error_reporter.Run(TPM_RC_SIZE);
    return;
  }
  if (response_code != TPM_RC_SUCCESS) {
    error_reporter.Run(response_code);
    return;
  }
  TPM_CC command_code = TPM_CC_ClearControl;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string authorization_section_bytes;
  if (tag == TPM_ST_SESSIONS) {
    UINT32 parameter_section_size = buffer.size();
    rc = Parse_UINT32(&buffer, &parameter_section_size, NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    if (parameter_section_size > buffer.size()) {
      error_reporter.Run(TPM_RC_INSUFFICIENT);
      return;
    }
    authorization_section_bytes = buffer.substr(parameter_section_size);
    // Keep the parameter section in |buffer|.
    buffer.erase(parameter_section_size);
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(response_code_bytes.data(),
               response_code_bytes.size());
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(buffer.data(),
               buffer.size());
  std::string response_hash(32, 0);
  hash->Finish(string_as_array(&response_hash), response_hash.size());
  if (tag == TPM_ST_SESSIONS) {
    if (!authorization_delegate->CheckResponseAuthorization(
        response_hash,
        authorization_section_bytes)) {
      error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
      return;
    }
  }
  callback.Run(response_code);
}

void Tpm::ClearControl(
      TPMI_RH_CLEAR auth,
      const std::string& auth_name,
      TPMI_YES_NO disable,
      AuthorizationDelegate* authorization_delegate,
      const ClearControlResponse& callback) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(ClearControlErrorCallback, callback);
  base::Callback<void(const std::string&)> parser =
      base::Bind(ClearControlResponseParser,
                 callback,
                 authorization_delegate);
  TPMI_ST_COMMAND_TAG tag = TPM_ST_NO_SESSIONS;
  UINT32 command_size = 10;  // Header size.
  std::string handle_section_bytes;
  std::string parameter_section_bytes;
  TPM_CC command_code = TPM_CC_ClearControl;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string auth_bytes;
  rc = Serialize_TPMI_RH_CLEAR(
      auth,
      &auth_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string disable_bytes;
  rc = Serialize_TPMI_YES_NO(
      disable,
      &disable_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(auth_name.data(),
               auth_name.size());
  handle_section_bytes += auth_bytes;
  command_size += auth_bytes.size();
  hash->Update(disable_bytes.data(),
               disable_bytes.size());
  parameter_section_bytes += disable_bytes;
  command_size += disable_bytes.size();
  std::string command_hash(32, 0);
  hash->Finish(string_as_array(&command_hash), command_hash.size());
  std::string authorization_section_bytes;
  if (!authorization_delegate->GetCommandAuthorization(
      command_hash,
      &authorization_section_bytes)) {
    error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
    return;
  }
  std::string authorization_size_bytes;
  if (!authorization_section_bytes.empty()) {
    tag = TPM_ST_SESSIONS;
    std::string tmp;
    rc = Serialize_UINT32(authorization_section_bytes.size(),
                          &authorization_size_bytes);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    command_size += authorization_size_bytes.size() +
                    authorization_section_bytes.size();
  }
  std::string tag_bytes;
  rc = Serialize_TPMI_ST_COMMAND_TAG(
      tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command_size_bytes;
  rc = Serialize_UINT32(
      command_size,
      &command_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command = tag_bytes +
                        command_size_bytes +
                        command_code_bytes +
                        handle_section_bytes +
                        authorization_size_bytes +
                        authorization_section_bytes +
                        parameter_section_bytes;
  CHECK(command.size() == command_size) << "Command size mismatch!";
  transceiver_->SendCommand(command, parser);
}

void HierarchyChangeAuthErrorCallback(
    const Tpm::HierarchyChangeAuthResponse& callback,
    TPM_RC response_code) {
  callback.Run(response_code);
}

void HierarchyChangeAuthResponseParser(
    const Tpm::HierarchyChangeAuthResponse& callback,
    AuthorizationDelegate* authorization_delegate,
    const std::string& response) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(HierarchyChangeAuthErrorCallback, callback);
  std::string buffer(response);
  TPM_ST tag;
  std::string tag_bytes;
  rc = Parse_TPM_ST(
      &buffer,
      &tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  UINT32 response_size;
  std::string response_size_bytes;
  rc = Parse_UINT32(
      &buffer,
      &response_size,
      &response_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM_RC response_code;
  std::string response_code_bytes;
  rc = Parse_TPM_RC(
      &buffer,
      &response_code,
      &response_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (response_size != response.size()) {
    error_reporter.Run(TPM_RC_SIZE);
    return;
  }
  if (response_code != TPM_RC_SUCCESS) {
    error_reporter.Run(response_code);
    return;
  }
  TPM_CC command_code = TPM_CC_HierarchyChangeAuth;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string authorization_section_bytes;
  if (tag == TPM_ST_SESSIONS) {
    UINT32 parameter_section_size = buffer.size();
    rc = Parse_UINT32(&buffer, &parameter_section_size, NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    if (parameter_section_size > buffer.size()) {
      error_reporter.Run(TPM_RC_INSUFFICIENT);
      return;
    }
    authorization_section_bytes = buffer.substr(parameter_section_size);
    // Keep the parameter section in |buffer|.
    buffer.erase(parameter_section_size);
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(response_code_bytes.data(),
               response_code_bytes.size());
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(buffer.data(),
               buffer.size());
  std::string response_hash(32, 0);
  hash->Finish(string_as_array(&response_hash), response_hash.size());
  if (tag == TPM_ST_SESSIONS) {
    if (!authorization_delegate->CheckResponseAuthorization(
        response_hash,
        authorization_section_bytes)) {
      error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
      return;
    }
  }
  callback.Run(response_code);
}

void Tpm::HierarchyChangeAuth(
      TPMI_RH_HIERARCHY_AUTH auth_handle,
      const std::string& auth_handle_name,
      TPM2B_AUTH new_auth,
      AuthorizationDelegate* authorization_delegate,
      const HierarchyChangeAuthResponse& callback) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(HierarchyChangeAuthErrorCallback, callback);
  base::Callback<void(const std::string&)> parser =
      base::Bind(HierarchyChangeAuthResponseParser,
                 callback,
                 authorization_delegate);
  TPMI_ST_COMMAND_TAG tag = TPM_ST_NO_SESSIONS;
  UINT32 command_size = 10;  // Header size.
  std::string handle_section_bytes;
  std::string parameter_section_bytes;
  TPM_CC command_code = TPM_CC_HierarchyChangeAuth;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string auth_handle_bytes;
  rc = Serialize_TPMI_RH_HIERARCHY_AUTH(
      auth_handle,
      &auth_handle_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string new_auth_bytes;
  rc = Serialize_TPM2B_AUTH(
      new_auth,
      &new_auth_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  // Encrypt just the parameter data, not the size.
  std::string tmp = new_auth_bytes.substr(2);
  if (!authorization_delegate->EncryptCommandParameter(&tmp)) {
    error_reporter.Run(TRUNKS_RC_ENCRYPTION_FAILED);
    return;
  }
  new_auth_bytes.replace(2, std::string::npos, tmp);
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(auth_handle_name.data(),
               auth_handle_name.size());
  handle_section_bytes += auth_handle_bytes;
  command_size += auth_handle_bytes.size();
  hash->Update(new_auth_bytes.data(),
               new_auth_bytes.size());
  parameter_section_bytes += new_auth_bytes;
  command_size += new_auth_bytes.size();
  std::string command_hash(32, 0);
  hash->Finish(string_as_array(&command_hash), command_hash.size());
  std::string authorization_section_bytes;
  if (!authorization_delegate->GetCommandAuthorization(
      command_hash,
      &authorization_section_bytes)) {
    error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
    return;
  }
  std::string authorization_size_bytes;
  if (!authorization_section_bytes.empty()) {
    tag = TPM_ST_SESSIONS;
    std::string tmp;
    rc = Serialize_UINT32(authorization_section_bytes.size(),
                          &authorization_size_bytes);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    command_size += authorization_size_bytes.size() +
                    authorization_section_bytes.size();
  }
  std::string tag_bytes;
  rc = Serialize_TPMI_ST_COMMAND_TAG(
      tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command_size_bytes;
  rc = Serialize_UINT32(
      command_size,
      &command_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command = tag_bytes +
                        command_size_bytes +
                        command_code_bytes +
                        handle_section_bytes +
                        authorization_size_bytes +
                        authorization_section_bytes +
                        parameter_section_bytes;
  CHECK(command.size() == command_size) << "Command size mismatch!";
  transceiver_->SendCommand(command, parser);
}

void DictionaryAttackLockResetErrorCallback(
    const Tpm::DictionaryAttackLockResetResponse& callback,
    TPM_RC response_code) {
  callback.Run(response_code);
}

void DictionaryAttackLockResetResponseParser(
    const Tpm::DictionaryAttackLockResetResponse& callback,
    AuthorizationDelegate* authorization_delegate,
    const std::string& response) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(DictionaryAttackLockResetErrorCallback, callback);
  std::string buffer(response);
  TPM_ST tag;
  std::string tag_bytes;
  rc = Parse_TPM_ST(
      &buffer,
      &tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  UINT32 response_size;
  std::string response_size_bytes;
  rc = Parse_UINT32(
      &buffer,
      &response_size,
      &response_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM_RC response_code;
  std::string response_code_bytes;
  rc = Parse_TPM_RC(
      &buffer,
      &response_code,
      &response_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (response_size != response.size()) {
    error_reporter.Run(TPM_RC_SIZE);
    return;
  }
  if (response_code != TPM_RC_SUCCESS) {
    error_reporter.Run(response_code);
    return;
  }
  TPM_CC command_code = TPM_CC_DictionaryAttackLockReset;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string authorization_section_bytes;
  if (tag == TPM_ST_SESSIONS) {
    UINT32 parameter_section_size = buffer.size();
    rc = Parse_UINT32(&buffer, &parameter_section_size, NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    if (parameter_section_size > buffer.size()) {
      error_reporter.Run(TPM_RC_INSUFFICIENT);
      return;
    }
    authorization_section_bytes = buffer.substr(parameter_section_size);
    // Keep the parameter section in |buffer|.
    buffer.erase(parameter_section_size);
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(response_code_bytes.data(),
               response_code_bytes.size());
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(buffer.data(),
               buffer.size());
  std::string response_hash(32, 0);
  hash->Finish(string_as_array(&response_hash), response_hash.size());
  if (tag == TPM_ST_SESSIONS) {
    if (!authorization_delegate->CheckResponseAuthorization(
        response_hash,
        authorization_section_bytes)) {
      error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
      return;
    }
  }
  callback.Run(response_code);
}

void Tpm::DictionaryAttackLockReset(
      TPMI_RH_LOCKOUT lock_handle,
      const std::string& lock_handle_name,
      AuthorizationDelegate* authorization_delegate,
      const DictionaryAttackLockResetResponse& callback) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(DictionaryAttackLockResetErrorCallback, callback);
  base::Callback<void(const std::string&)> parser =
      base::Bind(DictionaryAttackLockResetResponseParser,
                 callback,
                 authorization_delegate);
  TPMI_ST_COMMAND_TAG tag = TPM_ST_NO_SESSIONS;
  UINT32 command_size = 10;  // Header size.
  std::string handle_section_bytes;
  std::string parameter_section_bytes;
  TPM_CC command_code = TPM_CC_DictionaryAttackLockReset;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string lock_handle_bytes;
  rc = Serialize_TPMI_RH_LOCKOUT(
      lock_handle,
      &lock_handle_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(lock_handle_name.data(),
               lock_handle_name.size());
  handle_section_bytes += lock_handle_bytes;
  command_size += lock_handle_bytes.size();
  std::string command_hash(32, 0);
  hash->Finish(string_as_array(&command_hash), command_hash.size());
  std::string authorization_section_bytes;
  if (!authorization_delegate->GetCommandAuthorization(
      command_hash,
      &authorization_section_bytes)) {
    error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
    return;
  }
  std::string authorization_size_bytes;
  if (!authorization_section_bytes.empty()) {
    tag = TPM_ST_SESSIONS;
    std::string tmp;
    rc = Serialize_UINT32(authorization_section_bytes.size(),
                          &authorization_size_bytes);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    command_size += authorization_size_bytes.size() +
                    authorization_section_bytes.size();
  }
  std::string tag_bytes;
  rc = Serialize_TPMI_ST_COMMAND_TAG(
      tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command_size_bytes;
  rc = Serialize_UINT32(
      command_size,
      &command_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command = tag_bytes +
                        command_size_bytes +
                        command_code_bytes +
                        handle_section_bytes +
                        authorization_size_bytes +
                        authorization_section_bytes +
                        parameter_section_bytes;
  CHECK(command.size() == command_size) << "Command size mismatch!";
  transceiver_->SendCommand(command, parser);
}

void DictionaryAttackParametersErrorCallback(
    const Tpm::DictionaryAttackParametersResponse& callback,
    TPM_RC response_code) {
  callback.Run(response_code);
}

void DictionaryAttackParametersResponseParser(
    const Tpm::DictionaryAttackParametersResponse& callback,
    AuthorizationDelegate* authorization_delegate,
    const std::string& response) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(DictionaryAttackParametersErrorCallback, callback);
  std::string buffer(response);
  TPM_ST tag;
  std::string tag_bytes;
  rc = Parse_TPM_ST(
      &buffer,
      &tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  UINT32 response_size;
  std::string response_size_bytes;
  rc = Parse_UINT32(
      &buffer,
      &response_size,
      &response_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM_RC response_code;
  std::string response_code_bytes;
  rc = Parse_TPM_RC(
      &buffer,
      &response_code,
      &response_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (response_size != response.size()) {
    error_reporter.Run(TPM_RC_SIZE);
    return;
  }
  if (response_code != TPM_RC_SUCCESS) {
    error_reporter.Run(response_code);
    return;
  }
  TPM_CC command_code = TPM_CC_DictionaryAttackParameters;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string authorization_section_bytes;
  if (tag == TPM_ST_SESSIONS) {
    UINT32 parameter_section_size = buffer.size();
    rc = Parse_UINT32(&buffer, &parameter_section_size, NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    if (parameter_section_size > buffer.size()) {
      error_reporter.Run(TPM_RC_INSUFFICIENT);
      return;
    }
    authorization_section_bytes = buffer.substr(parameter_section_size);
    // Keep the parameter section in |buffer|.
    buffer.erase(parameter_section_size);
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(response_code_bytes.data(),
               response_code_bytes.size());
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(buffer.data(),
               buffer.size());
  std::string response_hash(32, 0);
  hash->Finish(string_as_array(&response_hash), response_hash.size());
  if (tag == TPM_ST_SESSIONS) {
    if (!authorization_delegate->CheckResponseAuthorization(
        response_hash,
        authorization_section_bytes)) {
      error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
      return;
    }
  }
  callback.Run(response_code);
}

void Tpm::DictionaryAttackParameters(
      TPMI_RH_LOCKOUT lock_handle,
      const std::string& lock_handle_name,
      UINT32 new_max_tries,
      UINT32 new_recovery_time,
      UINT32 lockout_recovery,
      AuthorizationDelegate* authorization_delegate,
      const DictionaryAttackParametersResponse& callback) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(DictionaryAttackParametersErrorCallback, callback);
  base::Callback<void(const std::string&)> parser =
      base::Bind(DictionaryAttackParametersResponseParser,
                 callback,
                 authorization_delegate);
  TPMI_ST_COMMAND_TAG tag = TPM_ST_NO_SESSIONS;
  UINT32 command_size = 10;  // Header size.
  std::string handle_section_bytes;
  std::string parameter_section_bytes;
  TPM_CC command_code = TPM_CC_DictionaryAttackParameters;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string lock_handle_bytes;
  rc = Serialize_TPMI_RH_LOCKOUT(
      lock_handle,
      &lock_handle_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string new_max_tries_bytes;
  rc = Serialize_UINT32(
      new_max_tries,
      &new_max_tries_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string new_recovery_time_bytes;
  rc = Serialize_UINT32(
      new_recovery_time,
      &new_recovery_time_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string lockout_recovery_bytes;
  rc = Serialize_UINT32(
      lockout_recovery,
      &lockout_recovery_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(lock_handle_name.data(),
               lock_handle_name.size());
  handle_section_bytes += lock_handle_bytes;
  command_size += lock_handle_bytes.size();
  hash->Update(new_max_tries_bytes.data(),
               new_max_tries_bytes.size());
  parameter_section_bytes += new_max_tries_bytes;
  command_size += new_max_tries_bytes.size();
  hash->Update(new_recovery_time_bytes.data(),
               new_recovery_time_bytes.size());
  parameter_section_bytes += new_recovery_time_bytes;
  command_size += new_recovery_time_bytes.size();
  hash->Update(lockout_recovery_bytes.data(),
               lockout_recovery_bytes.size());
  parameter_section_bytes += lockout_recovery_bytes;
  command_size += lockout_recovery_bytes.size();
  std::string command_hash(32, 0);
  hash->Finish(string_as_array(&command_hash), command_hash.size());
  std::string authorization_section_bytes;
  if (!authorization_delegate->GetCommandAuthorization(
      command_hash,
      &authorization_section_bytes)) {
    error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
    return;
  }
  std::string authorization_size_bytes;
  if (!authorization_section_bytes.empty()) {
    tag = TPM_ST_SESSIONS;
    std::string tmp;
    rc = Serialize_UINT32(authorization_section_bytes.size(),
                          &authorization_size_bytes);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    command_size += authorization_size_bytes.size() +
                    authorization_section_bytes.size();
  }
  std::string tag_bytes;
  rc = Serialize_TPMI_ST_COMMAND_TAG(
      tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command_size_bytes;
  rc = Serialize_UINT32(
      command_size,
      &command_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command = tag_bytes +
                        command_size_bytes +
                        command_code_bytes +
                        handle_section_bytes +
                        authorization_size_bytes +
                        authorization_section_bytes +
                        parameter_section_bytes;
  CHECK(command.size() == command_size) << "Command size mismatch!";
  transceiver_->SendCommand(command, parser);
}

void PP_CommandsErrorCallback(
    const Tpm::PP_CommandsResponse& callback,
    TPM_RC response_code) {
  callback.Run(response_code);
}

void PP_CommandsResponseParser(
    const Tpm::PP_CommandsResponse& callback,
    AuthorizationDelegate* authorization_delegate,
    const std::string& response) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(PP_CommandsErrorCallback, callback);
  std::string buffer(response);
  TPM_ST tag;
  std::string tag_bytes;
  rc = Parse_TPM_ST(
      &buffer,
      &tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  UINT32 response_size;
  std::string response_size_bytes;
  rc = Parse_UINT32(
      &buffer,
      &response_size,
      &response_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM_RC response_code;
  std::string response_code_bytes;
  rc = Parse_TPM_RC(
      &buffer,
      &response_code,
      &response_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (response_size != response.size()) {
    error_reporter.Run(TPM_RC_SIZE);
    return;
  }
  if (response_code != TPM_RC_SUCCESS) {
    error_reporter.Run(response_code);
    return;
  }
  TPM_CC command_code = TPM_CC_PP_Commands;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string authorization_section_bytes;
  if (tag == TPM_ST_SESSIONS) {
    UINT32 parameter_section_size = buffer.size();
    rc = Parse_UINT32(&buffer, &parameter_section_size, NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    if (parameter_section_size > buffer.size()) {
      error_reporter.Run(TPM_RC_INSUFFICIENT);
      return;
    }
    authorization_section_bytes = buffer.substr(parameter_section_size);
    // Keep the parameter section in |buffer|.
    buffer.erase(parameter_section_size);
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(response_code_bytes.data(),
               response_code_bytes.size());
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(buffer.data(),
               buffer.size());
  std::string response_hash(32, 0);
  hash->Finish(string_as_array(&response_hash), response_hash.size());
  if (tag == TPM_ST_SESSIONS) {
    if (!authorization_delegate->CheckResponseAuthorization(
        response_hash,
        authorization_section_bytes)) {
      error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
      return;
    }
  }
  callback.Run(response_code);
}

void Tpm::PP_Commands(
      TPMI_RH_PLATFORM auth,
      const std::string& auth_name,
      TPML_CC set_list,
      TPML_CC clear_list,
      AuthorizationDelegate* authorization_delegate,
      const PP_CommandsResponse& callback) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(PP_CommandsErrorCallback, callback);
  base::Callback<void(const std::string&)> parser =
      base::Bind(PP_CommandsResponseParser,
                 callback,
                 authorization_delegate);
  TPMI_ST_COMMAND_TAG tag = TPM_ST_NO_SESSIONS;
  UINT32 command_size = 10;  // Header size.
  std::string handle_section_bytes;
  std::string parameter_section_bytes;
  TPM_CC command_code = TPM_CC_PP_Commands;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string auth_bytes;
  rc = Serialize_TPMI_RH_PLATFORM(
      auth,
      &auth_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string set_list_bytes;
  rc = Serialize_TPML_CC(
      set_list,
      &set_list_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string clear_list_bytes;
  rc = Serialize_TPML_CC(
      clear_list,
      &clear_list_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(auth_name.data(),
               auth_name.size());
  handle_section_bytes += auth_bytes;
  command_size += auth_bytes.size();
  hash->Update(set_list_bytes.data(),
               set_list_bytes.size());
  parameter_section_bytes += set_list_bytes;
  command_size += set_list_bytes.size();
  hash->Update(clear_list_bytes.data(),
               clear_list_bytes.size());
  parameter_section_bytes += clear_list_bytes;
  command_size += clear_list_bytes.size();
  std::string command_hash(32, 0);
  hash->Finish(string_as_array(&command_hash), command_hash.size());
  std::string authorization_section_bytes;
  if (!authorization_delegate->GetCommandAuthorization(
      command_hash,
      &authorization_section_bytes)) {
    error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
    return;
  }
  std::string authorization_size_bytes;
  if (!authorization_section_bytes.empty()) {
    tag = TPM_ST_SESSIONS;
    std::string tmp;
    rc = Serialize_UINT32(authorization_section_bytes.size(),
                          &authorization_size_bytes);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    command_size += authorization_size_bytes.size() +
                    authorization_section_bytes.size();
  }
  std::string tag_bytes;
  rc = Serialize_TPMI_ST_COMMAND_TAG(
      tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command_size_bytes;
  rc = Serialize_UINT32(
      command_size,
      &command_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command = tag_bytes +
                        command_size_bytes +
                        command_code_bytes +
                        handle_section_bytes +
                        authorization_size_bytes +
                        authorization_section_bytes +
                        parameter_section_bytes;
  CHECK(command.size() == command_size) << "Command size mismatch!";
  transceiver_->SendCommand(command, parser);
}

void SetAlgorithmSetErrorCallback(
    const Tpm::SetAlgorithmSetResponse& callback,
    TPM_RC response_code) {
  callback.Run(response_code);
}

void SetAlgorithmSetResponseParser(
    const Tpm::SetAlgorithmSetResponse& callback,
    AuthorizationDelegate* authorization_delegate,
    const std::string& response) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(SetAlgorithmSetErrorCallback, callback);
  std::string buffer(response);
  TPM_ST tag;
  std::string tag_bytes;
  rc = Parse_TPM_ST(
      &buffer,
      &tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  UINT32 response_size;
  std::string response_size_bytes;
  rc = Parse_UINT32(
      &buffer,
      &response_size,
      &response_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM_RC response_code;
  std::string response_code_bytes;
  rc = Parse_TPM_RC(
      &buffer,
      &response_code,
      &response_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (response_size != response.size()) {
    error_reporter.Run(TPM_RC_SIZE);
    return;
  }
  if (response_code != TPM_RC_SUCCESS) {
    error_reporter.Run(response_code);
    return;
  }
  TPM_CC command_code = TPM_CC_SetAlgorithmSet;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string authorization_section_bytes;
  if (tag == TPM_ST_SESSIONS) {
    UINT32 parameter_section_size = buffer.size();
    rc = Parse_UINT32(&buffer, &parameter_section_size, NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    if (parameter_section_size > buffer.size()) {
      error_reporter.Run(TPM_RC_INSUFFICIENT);
      return;
    }
    authorization_section_bytes = buffer.substr(parameter_section_size);
    // Keep the parameter section in |buffer|.
    buffer.erase(parameter_section_size);
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(response_code_bytes.data(),
               response_code_bytes.size());
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(buffer.data(),
               buffer.size());
  std::string response_hash(32, 0);
  hash->Finish(string_as_array(&response_hash), response_hash.size());
  if (tag == TPM_ST_SESSIONS) {
    if (!authorization_delegate->CheckResponseAuthorization(
        response_hash,
        authorization_section_bytes)) {
      error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
      return;
    }
  }
  callback.Run(response_code);
}

void Tpm::SetAlgorithmSet(
      TPMI_RH_PLATFORM auth_handle,
      const std::string& auth_handle_name,
      UINT32 algorithm_set,
      AuthorizationDelegate* authorization_delegate,
      const SetAlgorithmSetResponse& callback) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(SetAlgorithmSetErrorCallback, callback);
  base::Callback<void(const std::string&)> parser =
      base::Bind(SetAlgorithmSetResponseParser,
                 callback,
                 authorization_delegate);
  TPMI_ST_COMMAND_TAG tag = TPM_ST_NO_SESSIONS;
  UINT32 command_size = 10;  // Header size.
  std::string handle_section_bytes;
  std::string parameter_section_bytes;
  TPM_CC command_code = TPM_CC_SetAlgorithmSet;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string auth_handle_bytes;
  rc = Serialize_TPMI_RH_PLATFORM(
      auth_handle,
      &auth_handle_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string algorithm_set_bytes;
  rc = Serialize_UINT32(
      algorithm_set,
      &algorithm_set_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(auth_handle_name.data(),
               auth_handle_name.size());
  handle_section_bytes += auth_handle_bytes;
  command_size += auth_handle_bytes.size();
  hash->Update(algorithm_set_bytes.data(),
               algorithm_set_bytes.size());
  parameter_section_bytes += algorithm_set_bytes;
  command_size += algorithm_set_bytes.size();
  std::string command_hash(32, 0);
  hash->Finish(string_as_array(&command_hash), command_hash.size());
  std::string authorization_section_bytes;
  if (!authorization_delegate->GetCommandAuthorization(
      command_hash,
      &authorization_section_bytes)) {
    error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
    return;
  }
  std::string authorization_size_bytes;
  if (!authorization_section_bytes.empty()) {
    tag = TPM_ST_SESSIONS;
    std::string tmp;
    rc = Serialize_UINT32(authorization_section_bytes.size(),
                          &authorization_size_bytes);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    command_size += authorization_size_bytes.size() +
                    authorization_section_bytes.size();
  }
  std::string tag_bytes;
  rc = Serialize_TPMI_ST_COMMAND_TAG(
      tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command_size_bytes;
  rc = Serialize_UINT32(
      command_size,
      &command_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command = tag_bytes +
                        command_size_bytes +
                        command_code_bytes +
                        handle_section_bytes +
                        authorization_size_bytes +
                        authorization_section_bytes +
                        parameter_section_bytes;
  CHECK(command.size() == command_size) << "Command size mismatch!";
  transceiver_->SendCommand(command, parser);
}

void FieldUpgradeStartErrorCallback(
    const Tpm::FieldUpgradeStartResponse& callback,
    TPM_RC response_code) {
  callback.Run(response_code);
}

void FieldUpgradeStartResponseParser(
    const Tpm::FieldUpgradeStartResponse& callback,
    AuthorizationDelegate* authorization_delegate,
    const std::string& response) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(FieldUpgradeStartErrorCallback, callback);
  std::string buffer(response);
  TPM_ST tag;
  std::string tag_bytes;
  rc = Parse_TPM_ST(
      &buffer,
      &tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  UINT32 response_size;
  std::string response_size_bytes;
  rc = Parse_UINT32(
      &buffer,
      &response_size,
      &response_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM_RC response_code;
  std::string response_code_bytes;
  rc = Parse_TPM_RC(
      &buffer,
      &response_code,
      &response_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (response_size != response.size()) {
    error_reporter.Run(TPM_RC_SIZE);
    return;
  }
  if (response_code != TPM_RC_SUCCESS) {
    error_reporter.Run(response_code);
    return;
  }
  TPM_CC command_code = TPM_CC_FieldUpgradeStart;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string authorization_section_bytes;
  if (tag == TPM_ST_SESSIONS) {
    UINT32 parameter_section_size = buffer.size();
    rc = Parse_UINT32(&buffer, &parameter_section_size, NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    if (parameter_section_size > buffer.size()) {
      error_reporter.Run(TPM_RC_INSUFFICIENT);
      return;
    }
    authorization_section_bytes = buffer.substr(parameter_section_size);
    // Keep the parameter section in |buffer|.
    buffer.erase(parameter_section_size);
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(response_code_bytes.data(),
               response_code_bytes.size());
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(buffer.data(),
               buffer.size());
  std::string response_hash(32, 0);
  hash->Finish(string_as_array(&response_hash), response_hash.size());
  if (tag == TPM_ST_SESSIONS) {
    if (!authorization_delegate->CheckResponseAuthorization(
        response_hash,
        authorization_section_bytes)) {
      error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
      return;
    }
  }
  callback.Run(response_code);
}

void Tpm::FieldUpgradeStart(
      TPMI_RH_PLATFORM authorization,
      const std::string& authorization_name,
      TPMI_DH_OBJECT key_handle,
      const std::string& key_handle_name,
      TPM2B_DIGEST fu_digest,
      TPMT_SIGNATURE manifest_signature,
      AuthorizationDelegate* authorization_delegate,
      const FieldUpgradeStartResponse& callback) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(FieldUpgradeStartErrorCallback, callback);
  base::Callback<void(const std::string&)> parser =
      base::Bind(FieldUpgradeStartResponseParser,
                 callback,
                 authorization_delegate);
  TPMI_ST_COMMAND_TAG tag = TPM_ST_NO_SESSIONS;
  UINT32 command_size = 10;  // Header size.
  std::string handle_section_bytes;
  std::string parameter_section_bytes;
  TPM_CC command_code = TPM_CC_FieldUpgradeStart;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string authorization_bytes;
  rc = Serialize_TPMI_RH_PLATFORM(
      authorization,
      &authorization_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string key_handle_bytes;
  rc = Serialize_TPMI_DH_OBJECT(
      key_handle,
      &key_handle_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string fu_digest_bytes;
  rc = Serialize_TPM2B_DIGEST(
      fu_digest,
      &fu_digest_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string manifest_signature_bytes;
  rc = Serialize_TPMT_SIGNATURE(
      manifest_signature,
      &manifest_signature_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  // Encrypt just the parameter data, not the size.
  std::string tmp = fu_digest_bytes.substr(2);
  if (!authorization_delegate->EncryptCommandParameter(&tmp)) {
    error_reporter.Run(TRUNKS_RC_ENCRYPTION_FAILED);
    return;
  }
  fu_digest_bytes.replace(2, std::string::npos, tmp);
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(authorization_name.data(),
               authorization_name.size());
  handle_section_bytes += authorization_bytes;
  command_size += authorization_bytes.size();
  hash->Update(key_handle_name.data(),
               key_handle_name.size());
  handle_section_bytes += key_handle_bytes;
  command_size += key_handle_bytes.size();
  hash->Update(fu_digest_bytes.data(),
               fu_digest_bytes.size());
  parameter_section_bytes += fu_digest_bytes;
  command_size += fu_digest_bytes.size();
  hash->Update(manifest_signature_bytes.data(),
               manifest_signature_bytes.size());
  parameter_section_bytes += manifest_signature_bytes;
  command_size += manifest_signature_bytes.size();
  std::string command_hash(32, 0);
  hash->Finish(string_as_array(&command_hash), command_hash.size());
  std::string authorization_section_bytes;
  if (!authorization_delegate->GetCommandAuthorization(
      command_hash,
      &authorization_section_bytes)) {
    error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
    return;
  }
  std::string authorization_size_bytes;
  if (!authorization_section_bytes.empty()) {
    tag = TPM_ST_SESSIONS;
    std::string tmp;
    rc = Serialize_UINT32(authorization_section_bytes.size(),
                          &authorization_size_bytes);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    command_size += authorization_size_bytes.size() +
                    authorization_section_bytes.size();
  }
  std::string tag_bytes;
  rc = Serialize_TPMI_ST_COMMAND_TAG(
      tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command_size_bytes;
  rc = Serialize_UINT32(
      command_size,
      &command_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command = tag_bytes +
                        command_size_bytes +
                        command_code_bytes +
                        handle_section_bytes +
                        authorization_size_bytes +
                        authorization_section_bytes +
                        parameter_section_bytes;
  CHECK(command.size() == command_size) << "Command size mismatch!";
  transceiver_->SendCommand(command, parser);
}

void FieldUpgradeDataErrorCallback(
    const Tpm::FieldUpgradeDataResponse& callback,
    TPM_RC response_code) {
  callback.Run(response_code,
               TPMT_HA(),
               TPMT_HA());
}

void FieldUpgradeDataResponseParser(
    const Tpm::FieldUpgradeDataResponse& callback,
    AuthorizationDelegate* authorization_delegate,
    const std::string& response) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(FieldUpgradeDataErrorCallback, callback);
  std::string buffer(response);
  TPM_ST tag;
  std::string tag_bytes;
  rc = Parse_TPM_ST(
      &buffer,
      &tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  UINT32 response_size;
  std::string response_size_bytes;
  rc = Parse_UINT32(
      &buffer,
      &response_size,
      &response_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM_RC response_code;
  std::string response_code_bytes;
  rc = Parse_TPM_RC(
      &buffer,
      &response_code,
      &response_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (response_size != response.size()) {
    error_reporter.Run(TPM_RC_SIZE);
    return;
  }
  if (response_code != TPM_RC_SUCCESS) {
    error_reporter.Run(response_code);
    return;
  }
  TPM_CC command_code = TPM_CC_FieldUpgradeData;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string authorization_section_bytes;
  if (tag == TPM_ST_SESSIONS) {
    UINT32 parameter_section_size = buffer.size();
    rc = Parse_UINT32(&buffer, &parameter_section_size, NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    if (parameter_section_size > buffer.size()) {
      error_reporter.Run(TPM_RC_INSUFFICIENT);
      return;
    }
    authorization_section_bytes = buffer.substr(parameter_section_size);
    // Keep the parameter section in |buffer|.
    buffer.erase(parameter_section_size);
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(response_code_bytes.data(),
               response_code_bytes.size());
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(buffer.data(),
               buffer.size());
  std::string response_hash(32, 0);
  hash->Finish(string_as_array(&response_hash), response_hash.size());
  if (tag == TPM_ST_SESSIONS) {
    if (!authorization_delegate->CheckResponseAuthorization(
        response_hash,
        authorization_section_bytes)) {
      error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
      return;
    }
  }
  TPMT_HA next_digest;
  std::string next_digest_bytes;
  rc = Parse_TPMT_HA(
      &buffer,
      &next_digest,
      &next_digest_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPMT_HA first_digest;
  std::string first_digest_bytes;
  rc = Parse_TPMT_HA(
      &buffer,
      &first_digest,
      &first_digest_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (tag == TPM_ST_SESSIONS) {
    // Decrypt just the parameter data, not the size.
    std::string tmp = next_digest_bytes.substr(2);
    if (!authorization_delegate->DecryptResponseParameter(&tmp)) {
      error_reporter.Run(TRUNKS_RC_ENCRYPTION_FAILED);
      return;
    }
    next_digest_bytes.replace(2, std::string::npos, tmp);
    rc = Parse_TPMT_HA(
        &next_digest_bytes,
        &next_digest,
        NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
  }
  callback.Run(response_code,
               next_digest,
               first_digest);
}

void Tpm::FieldUpgradeData(
      TPM2B_MAX_BUFFER fu_data,
      AuthorizationDelegate* authorization_delegate,
      const FieldUpgradeDataResponse& callback) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(FieldUpgradeDataErrorCallback, callback);
  base::Callback<void(const std::string&)> parser =
      base::Bind(FieldUpgradeDataResponseParser,
                 callback,
                 authorization_delegate);
  TPMI_ST_COMMAND_TAG tag = TPM_ST_NO_SESSIONS;
  UINT32 command_size = 10;  // Header size.
  std::string handle_section_bytes;
  std::string parameter_section_bytes;
  TPM_CC command_code = TPM_CC_FieldUpgradeData;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string fu_data_bytes;
  rc = Serialize_TPM2B_MAX_BUFFER(
      fu_data,
      &fu_data_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  // Encrypt just the parameter data, not the size.
  std::string tmp = fu_data_bytes.substr(2);
  if (!authorization_delegate->EncryptCommandParameter(&tmp)) {
    error_reporter.Run(TRUNKS_RC_ENCRYPTION_FAILED);
    return;
  }
  fu_data_bytes.replace(2, std::string::npos, tmp);
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(fu_data_bytes.data(),
               fu_data_bytes.size());
  parameter_section_bytes += fu_data_bytes;
  command_size += fu_data_bytes.size();
  std::string command_hash(32, 0);
  hash->Finish(string_as_array(&command_hash), command_hash.size());
  std::string authorization_section_bytes;
  if (!authorization_delegate->GetCommandAuthorization(
      command_hash,
      &authorization_section_bytes)) {
    error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
    return;
  }
  std::string authorization_size_bytes;
  if (!authorization_section_bytes.empty()) {
    tag = TPM_ST_SESSIONS;
    std::string tmp;
    rc = Serialize_UINT32(authorization_section_bytes.size(),
                          &authorization_size_bytes);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    command_size += authorization_size_bytes.size() +
                    authorization_section_bytes.size();
  }
  std::string tag_bytes;
  rc = Serialize_TPMI_ST_COMMAND_TAG(
      tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command_size_bytes;
  rc = Serialize_UINT32(
      command_size,
      &command_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command = tag_bytes +
                        command_size_bytes +
                        command_code_bytes +
                        handle_section_bytes +
                        authorization_size_bytes +
                        authorization_section_bytes +
                        parameter_section_bytes;
  CHECK(command.size() == command_size) << "Command size mismatch!";
  transceiver_->SendCommand(command, parser);
}

void FirmwareReadErrorCallback(
    const Tpm::FirmwareReadResponse& callback,
    TPM_RC response_code) {
  callback.Run(response_code,
               TPM2B_MAX_BUFFER());
}

void FirmwareReadResponseParser(
    const Tpm::FirmwareReadResponse& callback,
    AuthorizationDelegate* authorization_delegate,
    const std::string& response) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(FirmwareReadErrorCallback, callback);
  std::string buffer(response);
  TPM_ST tag;
  std::string tag_bytes;
  rc = Parse_TPM_ST(
      &buffer,
      &tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  UINT32 response_size;
  std::string response_size_bytes;
  rc = Parse_UINT32(
      &buffer,
      &response_size,
      &response_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM_RC response_code;
  std::string response_code_bytes;
  rc = Parse_TPM_RC(
      &buffer,
      &response_code,
      &response_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (response_size != response.size()) {
    error_reporter.Run(TPM_RC_SIZE);
    return;
  }
  if (response_code != TPM_RC_SUCCESS) {
    error_reporter.Run(response_code);
    return;
  }
  TPM_CC command_code = TPM_CC_FirmwareRead;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string authorization_section_bytes;
  if (tag == TPM_ST_SESSIONS) {
    UINT32 parameter_section_size = buffer.size();
    rc = Parse_UINT32(&buffer, &parameter_section_size, NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    if (parameter_section_size > buffer.size()) {
      error_reporter.Run(TPM_RC_INSUFFICIENT);
      return;
    }
    authorization_section_bytes = buffer.substr(parameter_section_size);
    // Keep the parameter section in |buffer|.
    buffer.erase(parameter_section_size);
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(response_code_bytes.data(),
               response_code_bytes.size());
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(buffer.data(),
               buffer.size());
  std::string response_hash(32, 0);
  hash->Finish(string_as_array(&response_hash), response_hash.size());
  if (tag == TPM_ST_SESSIONS) {
    if (!authorization_delegate->CheckResponseAuthorization(
        response_hash,
        authorization_section_bytes)) {
      error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
      return;
    }
  }
  TPM2B_MAX_BUFFER fu_data;
  std::string fu_data_bytes;
  rc = Parse_TPM2B_MAX_BUFFER(
      &buffer,
      &fu_data,
      &fu_data_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (tag == TPM_ST_SESSIONS) {
    // Decrypt just the parameter data, not the size.
    std::string tmp = fu_data_bytes.substr(2);
    if (!authorization_delegate->DecryptResponseParameter(&tmp)) {
      error_reporter.Run(TRUNKS_RC_ENCRYPTION_FAILED);
      return;
    }
    fu_data_bytes.replace(2, std::string::npos, tmp);
    rc = Parse_TPM2B_MAX_BUFFER(
        &fu_data_bytes,
        &fu_data,
        NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
  }
  callback.Run(response_code,
               fu_data);
}

void Tpm::FirmwareRead(
      UINT32 sequence_number,
      AuthorizationDelegate* authorization_delegate,
      const FirmwareReadResponse& callback) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(FirmwareReadErrorCallback, callback);
  base::Callback<void(const std::string&)> parser =
      base::Bind(FirmwareReadResponseParser,
                 callback,
                 authorization_delegate);
  TPMI_ST_COMMAND_TAG tag = TPM_ST_NO_SESSIONS;
  UINT32 command_size = 10;  // Header size.
  std::string handle_section_bytes;
  std::string parameter_section_bytes;
  TPM_CC command_code = TPM_CC_FirmwareRead;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string sequence_number_bytes;
  rc = Serialize_UINT32(
      sequence_number,
      &sequence_number_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(sequence_number_bytes.data(),
               sequence_number_bytes.size());
  parameter_section_bytes += sequence_number_bytes;
  command_size += sequence_number_bytes.size();
  std::string command_hash(32, 0);
  hash->Finish(string_as_array(&command_hash), command_hash.size());
  std::string authorization_section_bytes;
  if (!authorization_delegate->GetCommandAuthorization(
      command_hash,
      &authorization_section_bytes)) {
    error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
    return;
  }
  std::string authorization_size_bytes;
  if (!authorization_section_bytes.empty()) {
    tag = TPM_ST_SESSIONS;
    std::string tmp;
    rc = Serialize_UINT32(authorization_section_bytes.size(),
                          &authorization_size_bytes);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    command_size += authorization_size_bytes.size() +
                    authorization_section_bytes.size();
  }
  std::string tag_bytes;
  rc = Serialize_TPMI_ST_COMMAND_TAG(
      tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command_size_bytes;
  rc = Serialize_UINT32(
      command_size,
      &command_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command = tag_bytes +
                        command_size_bytes +
                        command_code_bytes +
                        handle_section_bytes +
                        authorization_size_bytes +
                        authorization_section_bytes +
                        parameter_section_bytes;
  CHECK(command.size() == command_size) << "Command size mismatch!";
  transceiver_->SendCommand(command, parser);
}

void ContextSaveErrorCallback(
    const Tpm::ContextSaveResponse& callback,
    TPM_RC response_code) {
  callback.Run(response_code,
               TPMS_CONTEXT());
}

void ContextSaveResponseParser(
    const Tpm::ContextSaveResponse& callback,
    AuthorizationDelegate* authorization_delegate,
    const std::string& response) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(ContextSaveErrorCallback, callback);
  std::string buffer(response);
  TPM_ST tag;
  std::string tag_bytes;
  rc = Parse_TPM_ST(
      &buffer,
      &tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  UINT32 response_size;
  std::string response_size_bytes;
  rc = Parse_UINT32(
      &buffer,
      &response_size,
      &response_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM_RC response_code;
  std::string response_code_bytes;
  rc = Parse_TPM_RC(
      &buffer,
      &response_code,
      &response_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (response_size != response.size()) {
    error_reporter.Run(TPM_RC_SIZE);
    return;
  }
  if (response_code != TPM_RC_SUCCESS) {
    error_reporter.Run(response_code);
    return;
  }
  TPM_CC command_code = TPM_CC_ContextSave;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string authorization_section_bytes;
  if (tag == TPM_ST_SESSIONS) {
    UINT32 parameter_section_size = buffer.size();
    rc = Parse_UINT32(&buffer, &parameter_section_size, NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    if (parameter_section_size > buffer.size()) {
      error_reporter.Run(TPM_RC_INSUFFICIENT);
      return;
    }
    authorization_section_bytes = buffer.substr(parameter_section_size);
    // Keep the parameter section in |buffer|.
    buffer.erase(parameter_section_size);
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(response_code_bytes.data(),
               response_code_bytes.size());
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(buffer.data(),
               buffer.size());
  std::string response_hash(32, 0);
  hash->Finish(string_as_array(&response_hash), response_hash.size());
  if (tag == TPM_ST_SESSIONS) {
    if (!authorization_delegate->CheckResponseAuthorization(
        response_hash,
        authorization_section_bytes)) {
      error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
      return;
    }
  }
  TPMS_CONTEXT context;
  std::string context_bytes;
  rc = Parse_TPMS_CONTEXT(
      &buffer,
      &context,
      &context_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (tag == TPM_ST_SESSIONS) {
    // Decrypt just the parameter data, not the size.
    std::string tmp = context_bytes.substr(2);
    if (!authorization_delegate->DecryptResponseParameter(&tmp)) {
      error_reporter.Run(TRUNKS_RC_ENCRYPTION_FAILED);
      return;
    }
    context_bytes.replace(2, std::string::npos, tmp);
    rc = Parse_TPMS_CONTEXT(
        &context_bytes,
        &context,
        NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
  }
  callback.Run(response_code,
               context);
}

void Tpm::ContextSave(
      TPMI_DH_CONTEXT save_handle,
      const std::string& save_handle_name,
      AuthorizationDelegate* authorization_delegate,
      const ContextSaveResponse& callback) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(ContextSaveErrorCallback, callback);
  base::Callback<void(const std::string&)> parser =
      base::Bind(ContextSaveResponseParser,
                 callback,
                 authorization_delegate);
  TPMI_ST_COMMAND_TAG tag = TPM_ST_NO_SESSIONS;
  UINT32 command_size = 10;  // Header size.
  std::string handle_section_bytes;
  std::string parameter_section_bytes;
  TPM_CC command_code = TPM_CC_ContextSave;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string save_handle_bytes;
  rc = Serialize_TPMI_DH_CONTEXT(
      save_handle,
      &save_handle_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(save_handle_name.data(),
               save_handle_name.size());
  handle_section_bytes += save_handle_bytes;
  command_size += save_handle_bytes.size();
  std::string command_hash(32, 0);
  hash->Finish(string_as_array(&command_hash), command_hash.size());
  std::string authorization_section_bytes;
  if (!authorization_delegate->GetCommandAuthorization(
      command_hash,
      &authorization_section_bytes)) {
    error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
    return;
  }
  std::string authorization_size_bytes;
  if (!authorization_section_bytes.empty()) {
    tag = TPM_ST_SESSIONS;
    std::string tmp;
    rc = Serialize_UINT32(authorization_section_bytes.size(),
                          &authorization_size_bytes);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    command_size += authorization_size_bytes.size() +
                    authorization_section_bytes.size();
  }
  std::string tag_bytes;
  rc = Serialize_TPMI_ST_COMMAND_TAG(
      tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command_size_bytes;
  rc = Serialize_UINT32(
      command_size,
      &command_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command = tag_bytes +
                        command_size_bytes +
                        command_code_bytes +
                        handle_section_bytes +
                        authorization_size_bytes +
                        authorization_section_bytes +
                        parameter_section_bytes;
  CHECK(command.size() == command_size) << "Command size mismatch!";
  transceiver_->SendCommand(command, parser);
}

void ContextLoadErrorCallback(
    const Tpm::ContextLoadResponse& callback,
    TPM_RC response_code) {
  callback.Run(response_code,
               TPMI_DH_CONTEXT());
}

void ContextLoadResponseParser(
    const Tpm::ContextLoadResponse& callback,
    AuthorizationDelegate* authorization_delegate,
    const std::string& response) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(ContextLoadErrorCallback, callback);
  std::string buffer(response);
  TPM_ST tag;
  std::string tag_bytes;
  rc = Parse_TPM_ST(
      &buffer,
      &tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  UINT32 response_size;
  std::string response_size_bytes;
  rc = Parse_UINT32(
      &buffer,
      &response_size,
      &response_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM_RC response_code;
  std::string response_code_bytes;
  rc = Parse_TPM_RC(
      &buffer,
      &response_code,
      &response_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (response_size != response.size()) {
    error_reporter.Run(TPM_RC_SIZE);
    return;
  }
  if (response_code != TPM_RC_SUCCESS) {
    error_reporter.Run(response_code);
    return;
  }
  TPM_CC command_code = TPM_CC_ContextLoad;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string authorization_section_bytes;
  if (tag == TPM_ST_SESSIONS) {
    UINT32 parameter_section_size = buffer.size();
    rc = Parse_UINT32(&buffer, &parameter_section_size, NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    if (parameter_section_size > buffer.size()) {
      error_reporter.Run(TPM_RC_INSUFFICIENT);
      return;
    }
    authorization_section_bytes = buffer.substr(parameter_section_size);
    // Keep the parameter section in |buffer|.
    buffer.erase(parameter_section_size);
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(response_code_bytes.data(),
               response_code_bytes.size());
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(buffer.data(),
               buffer.size());
  std::string response_hash(32, 0);
  hash->Finish(string_as_array(&response_hash), response_hash.size());
  if (tag == TPM_ST_SESSIONS) {
    if (!authorization_delegate->CheckResponseAuthorization(
        response_hash,
        authorization_section_bytes)) {
      error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
      return;
    }
  }
  TPMI_DH_CONTEXT loaded_handle;
  std::string loaded_handle_bytes;
  rc = Parse_TPMI_DH_CONTEXT(
      &buffer,
      &loaded_handle,
      &loaded_handle_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (tag == TPM_ST_SESSIONS) {
    // Decrypt just the parameter data, not the size.
    std::string tmp = loaded_handle_bytes.substr(2);
    if (!authorization_delegate->DecryptResponseParameter(&tmp)) {
      error_reporter.Run(TRUNKS_RC_ENCRYPTION_FAILED);
      return;
    }
    loaded_handle_bytes.replace(2, std::string::npos, tmp);
    rc = Parse_TPMI_DH_CONTEXT(
        &loaded_handle_bytes,
        &loaded_handle,
        NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
  }
  callback.Run(response_code,
               loaded_handle);
}

void Tpm::ContextLoad(
      TPMS_CONTEXT context,
      AuthorizationDelegate* authorization_delegate,
      const ContextLoadResponse& callback) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(ContextLoadErrorCallback, callback);
  base::Callback<void(const std::string&)> parser =
      base::Bind(ContextLoadResponseParser,
                 callback,
                 authorization_delegate);
  TPMI_ST_COMMAND_TAG tag = TPM_ST_NO_SESSIONS;
  UINT32 command_size = 10;  // Header size.
  std::string handle_section_bytes;
  std::string parameter_section_bytes;
  TPM_CC command_code = TPM_CC_ContextLoad;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string context_bytes;
  rc = Serialize_TPMS_CONTEXT(
      context,
      &context_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(context_bytes.data(),
               context_bytes.size());
  parameter_section_bytes += context_bytes;
  command_size += context_bytes.size();
  std::string command_hash(32, 0);
  hash->Finish(string_as_array(&command_hash), command_hash.size());
  std::string authorization_section_bytes;
  if (!authorization_delegate->GetCommandAuthorization(
      command_hash,
      &authorization_section_bytes)) {
    error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
    return;
  }
  std::string authorization_size_bytes;
  if (!authorization_section_bytes.empty()) {
    tag = TPM_ST_SESSIONS;
    std::string tmp;
    rc = Serialize_UINT32(authorization_section_bytes.size(),
                          &authorization_size_bytes);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    command_size += authorization_size_bytes.size() +
                    authorization_section_bytes.size();
  }
  std::string tag_bytes;
  rc = Serialize_TPMI_ST_COMMAND_TAG(
      tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command_size_bytes;
  rc = Serialize_UINT32(
      command_size,
      &command_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command = tag_bytes +
                        command_size_bytes +
                        command_code_bytes +
                        handle_section_bytes +
                        authorization_size_bytes +
                        authorization_section_bytes +
                        parameter_section_bytes;
  CHECK(command.size() == command_size) << "Command size mismatch!";
  transceiver_->SendCommand(command, parser);
}

void FlushContextErrorCallback(
    const Tpm::FlushContextResponse& callback,
    TPM_RC response_code) {
  callback.Run(response_code);
}

void FlushContextResponseParser(
    const Tpm::FlushContextResponse& callback,
    AuthorizationDelegate* authorization_delegate,
    const std::string& response) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(FlushContextErrorCallback, callback);
  std::string buffer(response);
  TPM_ST tag;
  std::string tag_bytes;
  rc = Parse_TPM_ST(
      &buffer,
      &tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  UINT32 response_size;
  std::string response_size_bytes;
  rc = Parse_UINT32(
      &buffer,
      &response_size,
      &response_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM_RC response_code;
  std::string response_code_bytes;
  rc = Parse_TPM_RC(
      &buffer,
      &response_code,
      &response_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (response_size != response.size()) {
    error_reporter.Run(TPM_RC_SIZE);
    return;
  }
  if (response_code != TPM_RC_SUCCESS) {
    error_reporter.Run(response_code);
    return;
  }
  TPM_CC command_code = TPM_CC_FlushContext;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string authorization_section_bytes;
  if (tag == TPM_ST_SESSIONS) {
    UINT32 parameter_section_size = buffer.size();
    rc = Parse_UINT32(&buffer, &parameter_section_size, NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    if (parameter_section_size > buffer.size()) {
      error_reporter.Run(TPM_RC_INSUFFICIENT);
      return;
    }
    authorization_section_bytes = buffer.substr(parameter_section_size);
    // Keep the parameter section in |buffer|.
    buffer.erase(parameter_section_size);
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(response_code_bytes.data(),
               response_code_bytes.size());
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(buffer.data(),
               buffer.size());
  std::string response_hash(32, 0);
  hash->Finish(string_as_array(&response_hash), response_hash.size());
  if (tag == TPM_ST_SESSIONS) {
    if (!authorization_delegate->CheckResponseAuthorization(
        response_hash,
        authorization_section_bytes)) {
      error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
      return;
    }
  }
  callback.Run(response_code);
}

void Tpm::FlushContext(
      TPMI_DH_CONTEXT flush_handle,
      const std::string& flush_handle_name,
      AuthorizationDelegate* authorization_delegate,
      const FlushContextResponse& callback) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(FlushContextErrorCallback, callback);
  base::Callback<void(const std::string&)> parser =
      base::Bind(FlushContextResponseParser,
                 callback,
                 authorization_delegate);
  TPMI_ST_COMMAND_TAG tag = TPM_ST_NO_SESSIONS;
  UINT32 command_size = 10;  // Header size.
  std::string handle_section_bytes;
  std::string parameter_section_bytes;
  TPM_CC command_code = TPM_CC_FlushContext;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string flush_handle_bytes;
  rc = Serialize_TPMI_DH_CONTEXT(
      flush_handle,
      &flush_handle_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(flush_handle_name.data(),
               flush_handle_name.size());
  handle_section_bytes += flush_handle_bytes;
  command_size += flush_handle_bytes.size();
  std::string command_hash(32, 0);
  hash->Finish(string_as_array(&command_hash), command_hash.size());
  std::string authorization_section_bytes;
  if (!authorization_delegate->GetCommandAuthorization(
      command_hash,
      &authorization_section_bytes)) {
    error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
    return;
  }
  std::string authorization_size_bytes;
  if (!authorization_section_bytes.empty()) {
    tag = TPM_ST_SESSIONS;
    std::string tmp;
    rc = Serialize_UINT32(authorization_section_bytes.size(),
                          &authorization_size_bytes);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    command_size += authorization_size_bytes.size() +
                    authorization_section_bytes.size();
  }
  std::string tag_bytes;
  rc = Serialize_TPMI_ST_COMMAND_TAG(
      tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command_size_bytes;
  rc = Serialize_UINT32(
      command_size,
      &command_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command = tag_bytes +
                        command_size_bytes +
                        command_code_bytes +
                        handle_section_bytes +
                        authorization_size_bytes +
                        authorization_section_bytes +
                        parameter_section_bytes;
  CHECK(command.size() == command_size) << "Command size mismatch!";
  transceiver_->SendCommand(command, parser);
}

void EvictControlErrorCallback(
    const Tpm::EvictControlResponse& callback,
    TPM_RC response_code) {
  callback.Run(response_code);
}

void EvictControlResponseParser(
    const Tpm::EvictControlResponse& callback,
    AuthorizationDelegate* authorization_delegate,
    const std::string& response) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(EvictControlErrorCallback, callback);
  std::string buffer(response);
  TPM_ST tag;
  std::string tag_bytes;
  rc = Parse_TPM_ST(
      &buffer,
      &tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  UINT32 response_size;
  std::string response_size_bytes;
  rc = Parse_UINT32(
      &buffer,
      &response_size,
      &response_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM_RC response_code;
  std::string response_code_bytes;
  rc = Parse_TPM_RC(
      &buffer,
      &response_code,
      &response_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (response_size != response.size()) {
    error_reporter.Run(TPM_RC_SIZE);
    return;
  }
  if (response_code != TPM_RC_SUCCESS) {
    error_reporter.Run(response_code);
    return;
  }
  TPM_CC command_code = TPM_CC_EvictControl;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string authorization_section_bytes;
  if (tag == TPM_ST_SESSIONS) {
    UINT32 parameter_section_size = buffer.size();
    rc = Parse_UINT32(&buffer, &parameter_section_size, NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    if (parameter_section_size > buffer.size()) {
      error_reporter.Run(TPM_RC_INSUFFICIENT);
      return;
    }
    authorization_section_bytes = buffer.substr(parameter_section_size);
    // Keep the parameter section in |buffer|.
    buffer.erase(parameter_section_size);
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(response_code_bytes.data(),
               response_code_bytes.size());
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(buffer.data(),
               buffer.size());
  std::string response_hash(32, 0);
  hash->Finish(string_as_array(&response_hash), response_hash.size());
  if (tag == TPM_ST_SESSIONS) {
    if (!authorization_delegate->CheckResponseAuthorization(
        response_hash,
        authorization_section_bytes)) {
      error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
      return;
    }
  }
  callback.Run(response_code);
}

void Tpm::EvictControl(
      TPMI_RH_PROVISION auth,
      const std::string& auth_name,
      TPMI_DH_OBJECT object_handle,
      const std::string& object_handle_name,
      TPMI_DH_PERSISTENT persistent_handle,
      const std::string& persistent_handle_name,
      AuthorizationDelegate* authorization_delegate,
      const EvictControlResponse& callback) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(EvictControlErrorCallback, callback);
  base::Callback<void(const std::string&)> parser =
      base::Bind(EvictControlResponseParser,
                 callback,
                 authorization_delegate);
  TPMI_ST_COMMAND_TAG tag = TPM_ST_NO_SESSIONS;
  UINT32 command_size = 10;  // Header size.
  std::string handle_section_bytes;
  std::string parameter_section_bytes;
  TPM_CC command_code = TPM_CC_EvictControl;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string auth_bytes;
  rc = Serialize_TPMI_RH_PROVISION(
      auth,
      &auth_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string object_handle_bytes;
  rc = Serialize_TPMI_DH_OBJECT(
      object_handle,
      &object_handle_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string persistent_handle_bytes;
  rc = Serialize_TPMI_DH_PERSISTENT(
      persistent_handle,
      &persistent_handle_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(auth_name.data(),
               auth_name.size());
  handle_section_bytes += auth_bytes;
  command_size += auth_bytes.size();
  hash->Update(object_handle_name.data(),
               object_handle_name.size());
  handle_section_bytes += object_handle_bytes;
  command_size += object_handle_bytes.size();
  hash->Update(persistent_handle_name.data(),
               persistent_handle_name.size());
  handle_section_bytes += persistent_handle_bytes;
  command_size += persistent_handle_bytes.size();
  std::string command_hash(32, 0);
  hash->Finish(string_as_array(&command_hash), command_hash.size());
  std::string authorization_section_bytes;
  if (!authorization_delegate->GetCommandAuthorization(
      command_hash,
      &authorization_section_bytes)) {
    error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
    return;
  }
  std::string authorization_size_bytes;
  if (!authorization_section_bytes.empty()) {
    tag = TPM_ST_SESSIONS;
    std::string tmp;
    rc = Serialize_UINT32(authorization_section_bytes.size(),
                          &authorization_size_bytes);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    command_size += authorization_size_bytes.size() +
                    authorization_section_bytes.size();
  }
  std::string tag_bytes;
  rc = Serialize_TPMI_ST_COMMAND_TAG(
      tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command_size_bytes;
  rc = Serialize_UINT32(
      command_size,
      &command_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command = tag_bytes +
                        command_size_bytes +
                        command_code_bytes +
                        handle_section_bytes +
                        authorization_size_bytes +
                        authorization_section_bytes +
                        parameter_section_bytes;
  CHECK(command.size() == command_size) << "Command size mismatch!";
  transceiver_->SendCommand(command, parser);
}

void ReadClockErrorCallback(
    const Tpm::ReadClockResponse& callback,
    TPM_RC response_code) {
  callback.Run(response_code,
               TPM_RC(),
               TPMS_TIME_INFO());
}

void ReadClockResponseParser(
    const Tpm::ReadClockResponse& callback,
    AuthorizationDelegate* authorization_delegate,
    const std::string& response) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(ReadClockErrorCallback, callback);
  std::string buffer(response);
  TPM_ST tag;
  std::string tag_bytes;
  rc = Parse_TPM_ST(
      &buffer,
      &tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  UINT32 response_size;
  std::string response_size_bytes;
  rc = Parse_UINT32(
      &buffer,
      &response_size,
      &response_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM_RC response_code;
  std::string response_code_bytes;
  rc = Parse_TPM_RC(
      &buffer,
      &response_code,
      &response_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (response_size != response.size()) {
    error_reporter.Run(TPM_RC_SIZE);
    return;
  }
  if (response_code != TPM_RC_SUCCESS) {
    error_reporter.Run(response_code);
    return;
  }
  TPM_CC command_code = TPM_CC_ReadClock;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string authorization_section_bytes;
  if (tag == TPM_ST_SESSIONS) {
    UINT32 parameter_section_size = buffer.size();
    rc = Parse_UINT32(&buffer, &parameter_section_size, NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    if (parameter_section_size > buffer.size()) {
      error_reporter.Run(TPM_RC_INSUFFICIENT);
      return;
    }
    authorization_section_bytes = buffer.substr(parameter_section_size);
    // Keep the parameter section in |buffer|.
    buffer.erase(parameter_section_size);
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(response_code_bytes.data(),
               response_code_bytes.size());
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(buffer.data(),
               buffer.size());
  std::string response_hash(32, 0);
  hash->Finish(string_as_array(&response_hash), response_hash.size());
  if (tag == TPM_ST_SESSIONS) {
    if (!authorization_delegate->CheckResponseAuthorization(
        response_hash,
        authorization_section_bytes)) {
      error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
      return;
    }
  }
  TPM_RC return_code;
  std::string return_code_bytes;
  rc = Parse_TPM_RC(
      &buffer,
      &return_code,
      &return_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPMS_TIME_INFO current_time;
  std::string current_time_bytes;
  rc = Parse_TPMS_TIME_INFO(
      &buffer,
      &current_time,
      &current_time_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (tag == TPM_ST_SESSIONS) {
    // Decrypt just the parameter data, not the size.
    std::string tmp = return_code_bytes.substr(2);
    if (!authorization_delegate->DecryptResponseParameter(&tmp)) {
      error_reporter.Run(TRUNKS_RC_ENCRYPTION_FAILED);
      return;
    }
    return_code_bytes.replace(2, std::string::npos, tmp);
    rc = Parse_TPM_RC(
        &return_code_bytes,
        &return_code,
        NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
  }
  callback.Run(response_code,
               return_code,
               current_time);
}

void Tpm::ReadClock(
      AuthorizationDelegate* authorization_delegate,
      const ReadClockResponse& callback) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(ReadClockErrorCallback, callback);
  base::Callback<void(const std::string&)> parser =
      base::Bind(ReadClockResponseParser,
                 callback,
                 authorization_delegate);
  TPMI_ST_COMMAND_TAG tag = TPM_ST_NO_SESSIONS;
  UINT32 command_size = 10;  // Header size.
  std::string handle_section_bytes;
  std::string parameter_section_bytes;
  TPM_CC command_code = TPM_CC_ReadClock;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  std::string command_hash(32, 0);
  hash->Finish(string_as_array(&command_hash), command_hash.size());
  std::string authorization_section_bytes;
  if (!authorization_delegate->GetCommandAuthorization(
      command_hash,
      &authorization_section_bytes)) {
    error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
    return;
  }
  std::string authorization_size_bytes;
  if (!authorization_section_bytes.empty()) {
    tag = TPM_ST_SESSIONS;
    std::string tmp;
    rc = Serialize_UINT32(authorization_section_bytes.size(),
                          &authorization_size_bytes);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    command_size += authorization_size_bytes.size() +
                    authorization_section_bytes.size();
  }
  std::string tag_bytes;
  rc = Serialize_TPMI_ST_COMMAND_TAG(
      tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command_size_bytes;
  rc = Serialize_UINT32(
      command_size,
      &command_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command = tag_bytes +
                        command_size_bytes +
                        command_code_bytes +
                        handle_section_bytes +
                        authorization_size_bytes +
                        authorization_section_bytes +
                        parameter_section_bytes;
  CHECK(command.size() == command_size) << "Command size mismatch!";
  transceiver_->SendCommand(command, parser);
}

void ClockSetErrorCallback(
    const Tpm::ClockSetResponse& callback,
    TPM_RC response_code) {
  callback.Run(response_code,
               TPM_RC());
}

void ClockSetResponseParser(
    const Tpm::ClockSetResponse& callback,
    AuthorizationDelegate* authorization_delegate,
    const std::string& response) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(ClockSetErrorCallback, callback);
  std::string buffer(response);
  TPM_ST tag;
  std::string tag_bytes;
  rc = Parse_TPM_ST(
      &buffer,
      &tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  UINT32 response_size;
  std::string response_size_bytes;
  rc = Parse_UINT32(
      &buffer,
      &response_size,
      &response_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM_RC response_code;
  std::string response_code_bytes;
  rc = Parse_TPM_RC(
      &buffer,
      &response_code,
      &response_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (response_size != response.size()) {
    error_reporter.Run(TPM_RC_SIZE);
    return;
  }
  if (response_code != TPM_RC_SUCCESS) {
    error_reporter.Run(response_code);
    return;
  }
  TPM_CC command_code = TPM_CC_ClockSet;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string authorization_section_bytes;
  if (tag == TPM_ST_SESSIONS) {
    UINT32 parameter_section_size = buffer.size();
    rc = Parse_UINT32(&buffer, &parameter_section_size, NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    if (parameter_section_size > buffer.size()) {
      error_reporter.Run(TPM_RC_INSUFFICIENT);
      return;
    }
    authorization_section_bytes = buffer.substr(parameter_section_size);
    // Keep the parameter section in |buffer|.
    buffer.erase(parameter_section_size);
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(response_code_bytes.data(),
               response_code_bytes.size());
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(buffer.data(),
               buffer.size());
  std::string response_hash(32, 0);
  hash->Finish(string_as_array(&response_hash), response_hash.size());
  if (tag == TPM_ST_SESSIONS) {
    if (!authorization_delegate->CheckResponseAuthorization(
        response_hash,
        authorization_section_bytes)) {
      error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
      return;
    }
  }
  TPM_RC return_code;
  std::string return_code_bytes;
  rc = Parse_TPM_RC(
      &buffer,
      &return_code,
      &return_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (tag == TPM_ST_SESSIONS) {
    // Decrypt just the parameter data, not the size.
    std::string tmp = return_code_bytes.substr(2);
    if (!authorization_delegate->DecryptResponseParameter(&tmp)) {
      error_reporter.Run(TRUNKS_RC_ENCRYPTION_FAILED);
      return;
    }
    return_code_bytes.replace(2, std::string::npos, tmp);
    rc = Parse_TPM_RC(
        &return_code_bytes,
        &return_code,
        NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
  }
  callback.Run(response_code,
               return_code);
}

void Tpm::ClockSet(
      TPMI_RH_PROVISION auth,
      const std::string& auth_name,
      UINT64 new_time,
      AuthorizationDelegate* authorization_delegate,
      const ClockSetResponse& callback) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(ClockSetErrorCallback, callback);
  base::Callback<void(const std::string&)> parser =
      base::Bind(ClockSetResponseParser,
                 callback,
                 authorization_delegate);
  TPMI_ST_COMMAND_TAG tag = TPM_ST_NO_SESSIONS;
  UINT32 command_size = 10;  // Header size.
  std::string handle_section_bytes;
  std::string parameter_section_bytes;
  TPM_CC command_code = TPM_CC_ClockSet;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string auth_bytes;
  rc = Serialize_TPMI_RH_PROVISION(
      auth,
      &auth_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string new_time_bytes;
  rc = Serialize_UINT64(
      new_time,
      &new_time_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(auth_name.data(),
               auth_name.size());
  handle_section_bytes += auth_bytes;
  command_size += auth_bytes.size();
  hash->Update(new_time_bytes.data(),
               new_time_bytes.size());
  parameter_section_bytes += new_time_bytes;
  command_size += new_time_bytes.size();
  std::string command_hash(32, 0);
  hash->Finish(string_as_array(&command_hash), command_hash.size());
  std::string authorization_section_bytes;
  if (!authorization_delegate->GetCommandAuthorization(
      command_hash,
      &authorization_section_bytes)) {
    error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
    return;
  }
  std::string authorization_size_bytes;
  if (!authorization_section_bytes.empty()) {
    tag = TPM_ST_SESSIONS;
    std::string tmp;
    rc = Serialize_UINT32(authorization_section_bytes.size(),
                          &authorization_size_bytes);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    command_size += authorization_size_bytes.size() +
                    authorization_section_bytes.size();
  }
  std::string tag_bytes;
  rc = Serialize_TPMI_ST_COMMAND_TAG(
      tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command_size_bytes;
  rc = Serialize_UINT32(
      command_size,
      &command_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command = tag_bytes +
                        command_size_bytes +
                        command_code_bytes +
                        handle_section_bytes +
                        authorization_size_bytes +
                        authorization_section_bytes +
                        parameter_section_bytes;
  CHECK(command.size() == command_size) << "Command size mismatch!";
  transceiver_->SendCommand(command, parser);
}

void ClockRateAdjustErrorCallback(
    const Tpm::ClockRateAdjustResponse& callback,
    TPM_RC response_code) {
  callback.Run(response_code,
               TPM_RC());
}

void ClockRateAdjustResponseParser(
    const Tpm::ClockRateAdjustResponse& callback,
    AuthorizationDelegate* authorization_delegate,
    const std::string& response) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(ClockRateAdjustErrorCallback, callback);
  std::string buffer(response);
  TPM_ST tag;
  std::string tag_bytes;
  rc = Parse_TPM_ST(
      &buffer,
      &tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  UINT32 response_size;
  std::string response_size_bytes;
  rc = Parse_UINT32(
      &buffer,
      &response_size,
      &response_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM_RC response_code;
  std::string response_code_bytes;
  rc = Parse_TPM_RC(
      &buffer,
      &response_code,
      &response_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (response_size != response.size()) {
    error_reporter.Run(TPM_RC_SIZE);
    return;
  }
  if (response_code != TPM_RC_SUCCESS) {
    error_reporter.Run(response_code);
    return;
  }
  TPM_CC command_code = TPM_CC_ClockRateAdjust;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string authorization_section_bytes;
  if (tag == TPM_ST_SESSIONS) {
    UINT32 parameter_section_size = buffer.size();
    rc = Parse_UINT32(&buffer, &parameter_section_size, NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    if (parameter_section_size > buffer.size()) {
      error_reporter.Run(TPM_RC_INSUFFICIENT);
      return;
    }
    authorization_section_bytes = buffer.substr(parameter_section_size);
    // Keep the parameter section in |buffer|.
    buffer.erase(parameter_section_size);
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(response_code_bytes.data(),
               response_code_bytes.size());
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(buffer.data(),
               buffer.size());
  std::string response_hash(32, 0);
  hash->Finish(string_as_array(&response_hash), response_hash.size());
  if (tag == TPM_ST_SESSIONS) {
    if (!authorization_delegate->CheckResponseAuthorization(
        response_hash,
        authorization_section_bytes)) {
      error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
      return;
    }
  }
  TPM_RC return_code;
  std::string return_code_bytes;
  rc = Parse_TPM_RC(
      &buffer,
      &return_code,
      &return_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (tag == TPM_ST_SESSIONS) {
    // Decrypt just the parameter data, not the size.
    std::string tmp = return_code_bytes.substr(2);
    if (!authorization_delegate->DecryptResponseParameter(&tmp)) {
      error_reporter.Run(TRUNKS_RC_ENCRYPTION_FAILED);
      return;
    }
    return_code_bytes.replace(2, std::string::npos, tmp);
    rc = Parse_TPM_RC(
        &return_code_bytes,
        &return_code,
        NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
  }
  callback.Run(response_code,
               return_code);
}

void Tpm::ClockRateAdjust(
      TPMI_RH_PROVISION auth,
      const std::string& auth_name,
      TPM_CLOCK_ADJUST rate_adjust,
      AuthorizationDelegate* authorization_delegate,
      const ClockRateAdjustResponse& callback) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(ClockRateAdjustErrorCallback, callback);
  base::Callback<void(const std::string&)> parser =
      base::Bind(ClockRateAdjustResponseParser,
                 callback,
                 authorization_delegate);
  TPMI_ST_COMMAND_TAG tag = TPM_ST_NO_SESSIONS;
  UINT32 command_size = 10;  // Header size.
  std::string handle_section_bytes;
  std::string parameter_section_bytes;
  TPM_CC command_code = TPM_CC_ClockRateAdjust;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string auth_bytes;
  rc = Serialize_TPMI_RH_PROVISION(
      auth,
      &auth_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string rate_adjust_bytes;
  rc = Serialize_TPM_CLOCK_ADJUST(
      rate_adjust,
      &rate_adjust_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(auth_name.data(),
               auth_name.size());
  handle_section_bytes += auth_bytes;
  command_size += auth_bytes.size();
  hash->Update(rate_adjust_bytes.data(),
               rate_adjust_bytes.size());
  parameter_section_bytes += rate_adjust_bytes;
  command_size += rate_adjust_bytes.size();
  std::string command_hash(32, 0);
  hash->Finish(string_as_array(&command_hash), command_hash.size());
  std::string authorization_section_bytes;
  if (!authorization_delegate->GetCommandAuthorization(
      command_hash,
      &authorization_section_bytes)) {
    error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
    return;
  }
  std::string authorization_size_bytes;
  if (!authorization_section_bytes.empty()) {
    tag = TPM_ST_SESSIONS;
    std::string tmp;
    rc = Serialize_UINT32(authorization_section_bytes.size(),
                          &authorization_size_bytes);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    command_size += authorization_size_bytes.size() +
                    authorization_section_bytes.size();
  }
  std::string tag_bytes;
  rc = Serialize_TPMI_ST_COMMAND_TAG(
      tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command_size_bytes;
  rc = Serialize_UINT32(
      command_size,
      &command_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command = tag_bytes +
                        command_size_bytes +
                        command_code_bytes +
                        handle_section_bytes +
                        authorization_size_bytes +
                        authorization_section_bytes +
                        parameter_section_bytes;
  CHECK(command.size() == command_size) << "Command size mismatch!";
  transceiver_->SendCommand(command, parser);
}

void GetCapabilityErrorCallback(
    const Tpm::GetCapabilityResponse& callback,
    TPM_RC response_code) {
  callback.Run(response_code,
               TPMI_YES_NO(),
               TPMS_CAPABILITY_DATA());
}

void GetCapabilityResponseParser(
    const Tpm::GetCapabilityResponse& callback,
    AuthorizationDelegate* authorization_delegate,
    const std::string& response) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(GetCapabilityErrorCallback, callback);
  std::string buffer(response);
  TPM_ST tag;
  std::string tag_bytes;
  rc = Parse_TPM_ST(
      &buffer,
      &tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  UINT32 response_size;
  std::string response_size_bytes;
  rc = Parse_UINT32(
      &buffer,
      &response_size,
      &response_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM_RC response_code;
  std::string response_code_bytes;
  rc = Parse_TPM_RC(
      &buffer,
      &response_code,
      &response_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (response_size != response.size()) {
    error_reporter.Run(TPM_RC_SIZE);
    return;
  }
  if (response_code != TPM_RC_SUCCESS) {
    error_reporter.Run(response_code);
    return;
  }
  TPM_CC command_code = TPM_CC_GetCapability;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string authorization_section_bytes;
  if (tag == TPM_ST_SESSIONS) {
    UINT32 parameter_section_size = buffer.size();
    rc = Parse_UINT32(&buffer, &parameter_section_size, NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    if (parameter_section_size > buffer.size()) {
      error_reporter.Run(TPM_RC_INSUFFICIENT);
      return;
    }
    authorization_section_bytes = buffer.substr(parameter_section_size);
    // Keep the parameter section in |buffer|.
    buffer.erase(parameter_section_size);
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(response_code_bytes.data(),
               response_code_bytes.size());
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(buffer.data(),
               buffer.size());
  std::string response_hash(32, 0);
  hash->Finish(string_as_array(&response_hash), response_hash.size());
  if (tag == TPM_ST_SESSIONS) {
    if (!authorization_delegate->CheckResponseAuthorization(
        response_hash,
        authorization_section_bytes)) {
      error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
      return;
    }
  }
  TPMI_YES_NO more_data;
  std::string more_data_bytes;
  rc = Parse_TPMI_YES_NO(
      &buffer,
      &more_data,
      &more_data_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPMS_CAPABILITY_DATA capability_data;
  std::string capability_data_bytes;
  rc = Parse_TPMS_CAPABILITY_DATA(
      &buffer,
      &capability_data,
      &capability_data_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (tag == TPM_ST_SESSIONS) {
    // Decrypt just the parameter data, not the size.
    std::string tmp = more_data_bytes.substr(2);
    if (!authorization_delegate->DecryptResponseParameter(&tmp)) {
      error_reporter.Run(TRUNKS_RC_ENCRYPTION_FAILED);
      return;
    }
    more_data_bytes.replace(2, std::string::npos, tmp);
    rc = Parse_TPMI_YES_NO(
        &more_data_bytes,
        &more_data,
        NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
  }
  callback.Run(response_code,
               more_data,
               capability_data);
}

void Tpm::GetCapability(
      TPM_CAP capability,
      UINT32 property,
      UINT32 property_count,
      AuthorizationDelegate* authorization_delegate,
      const GetCapabilityResponse& callback) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(GetCapabilityErrorCallback, callback);
  base::Callback<void(const std::string&)> parser =
      base::Bind(GetCapabilityResponseParser,
                 callback,
                 authorization_delegate);
  TPMI_ST_COMMAND_TAG tag = TPM_ST_NO_SESSIONS;
  UINT32 command_size = 10;  // Header size.
  std::string handle_section_bytes;
  std::string parameter_section_bytes;
  TPM_CC command_code = TPM_CC_GetCapability;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string capability_bytes;
  rc = Serialize_TPM_CAP(
      capability,
      &capability_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string property_bytes;
  rc = Serialize_UINT32(
      property,
      &property_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string property_count_bytes;
  rc = Serialize_UINT32(
      property_count,
      &property_count_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(capability_bytes.data(),
               capability_bytes.size());
  parameter_section_bytes += capability_bytes;
  command_size += capability_bytes.size();
  hash->Update(property_bytes.data(),
               property_bytes.size());
  parameter_section_bytes += property_bytes;
  command_size += property_bytes.size();
  hash->Update(property_count_bytes.data(),
               property_count_bytes.size());
  parameter_section_bytes += property_count_bytes;
  command_size += property_count_bytes.size();
  std::string command_hash(32, 0);
  hash->Finish(string_as_array(&command_hash), command_hash.size());
  std::string authorization_section_bytes;
  if (!authorization_delegate->GetCommandAuthorization(
      command_hash,
      &authorization_section_bytes)) {
    error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
    return;
  }
  std::string authorization_size_bytes;
  if (!authorization_section_bytes.empty()) {
    tag = TPM_ST_SESSIONS;
    std::string tmp;
    rc = Serialize_UINT32(authorization_section_bytes.size(),
                          &authorization_size_bytes);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    command_size += authorization_size_bytes.size() +
                    authorization_section_bytes.size();
  }
  std::string tag_bytes;
  rc = Serialize_TPMI_ST_COMMAND_TAG(
      tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command_size_bytes;
  rc = Serialize_UINT32(
      command_size,
      &command_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command = tag_bytes +
                        command_size_bytes +
                        command_code_bytes +
                        handle_section_bytes +
                        authorization_size_bytes +
                        authorization_section_bytes +
                        parameter_section_bytes;
  CHECK(command.size() == command_size) << "Command size mismatch!";
  transceiver_->SendCommand(command, parser);
}

void TestParmsErrorCallback(
    const Tpm::TestParmsResponse& callback,
    TPM_RC response_code) {
  callback.Run(response_code);
}

void TestParmsResponseParser(
    const Tpm::TestParmsResponse& callback,
    AuthorizationDelegate* authorization_delegate,
    const std::string& response) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(TestParmsErrorCallback, callback);
  std::string buffer(response);
  TPM_ST tag;
  std::string tag_bytes;
  rc = Parse_TPM_ST(
      &buffer,
      &tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  UINT32 response_size;
  std::string response_size_bytes;
  rc = Parse_UINT32(
      &buffer,
      &response_size,
      &response_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM_RC response_code;
  std::string response_code_bytes;
  rc = Parse_TPM_RC(
      &buffer,
      &response_code,
      &response_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (response_size != response.size()) {
    error_reporter.Run(TPM_RC_SIZE);
    return;
  }
  if (response_code != TPM_RC_SUCCESS) {
    error_reporter.Run(response_code);
    return;
  }
  TPM_CC command_code = TPM_CC_TestParms;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string authorization_section_bytes;
  if (tag == TPM_ST_SESSIONS) {
    UINT32 parameter_section_size = buffer.size();
    rc = Parse_UINT32(&buffer, &parameter_section_size, NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    if (parameter_section_size > buffer.size()) {
      error_reporter.Run(TPM_RC_INSUFFICIENT);
      return;
    }
    authorization_section_bytes = buffer.substr(parameter_section_size);
    // Keep the parameter section in |buffer|.
    buffer.erase(parameter_section_size);
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(response_code_bytes.data(),
               response_code_bytes.size());
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(buffer.data(),
               buffer.size());
  std::string response_hash(32, 0);
  hash->Finish(string_as_array(&response_hash), response_hash.size());
  if (tag == TPM_ST_SESSIONS) {
    if (!authorization_delegate->CheckResponseAuthorization(
        response_hash,
        authorization_section_bytes)) {
      error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
      return;
    }
  }
  callback.Run(response_code);
}

void Tpm::TestParms(
      TPMT_PUBLIC_PARMS parameters,
      AuthorizationDelegate* authorization_delegate,
      const TestParmsResponse& callback) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(TestParmsErrorCallback, callback);
  base::Callback<void(const std::string&)> parser =
      base::Bind(TestParmsResponseParser,
                 callback,
                 authorization_delegate);
  TPMI_ST_COMMAND_TAG tag = TPM_ST_NO_SESSIONS;
  UINT32 command_size = 10;  // Header size.
  std::string handle_section_bytes;
  std::string parameter_section_bytes;
  TPM_CC command_code = TPM_CC_TestParms;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string parameters_bytes;
  rc = Serialize_TPMT_PUBLIC_PARMS(
      parameters,
      &parameters_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(parameters_bytes.data(),
               parameters_bytes.size());
  parameter_section_bytes += parameters_bytes;
  command_size += parameters_bytes.size();
  std::string command_hash(32, 0);
  hash->Finish(string_as_array(&command_hash), command_hash.size());
  std::string authorization_section_bytes;
  if (!authorization_delegate->GetCommandAuthorization(
      command_hash,
      &authorization_section_bytes)) {
    error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
    return;
  }
  std::string authorization_size_bytes;
  if (!authorization_section_bytes.empty()) {
    tag = TPM_ST_SESSIONS;
    std::string tmp;
    rc = Serialize_UINT32(authorization_section_bytes.size(),
                          &authorization_size_bytes);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    command_size += authorization_size_bytes.size() +
                    authorization_section_bytes.size();
  }
  std::string tag_bytes;
  rc = Serialize_TPMI_ST_COMMAND_TAG(
      tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command_size_bytes;
  rc = Serialize_UINT32(
      command_size,
      &command_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command = tag_bytes +
                        command_size_bytes +
                        command_code_bytes +
                        handle_section_bytes +
                        authorization_size_bytes +
                        authorization_section_bytes +
                        parameter_section_bytes;
  CHECK(command.size() == command_size) << "Command size mismatch!";
  transceiver_->SendCommand(command, parser);
}

void NV_DefineSpaceErrorCallback(
    const Tpm::NV_DefineSpaceResponse& callback,
    TPM_RC response_code) {
  callback.Run(response_code);
}

void NV_DefineSpaceResponseParser(
    const Tpm::NV_DefineSpaceResponse& callback,
    AuthorizationDelegate* authorization_delegate,
    const std::string& response) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(NV_DefineSpaceErrorCallback, callback);
  std::string buffer(response);
  TPM_ST tag;
  std::string tag_bytes;
  rc = Parse_TPM_ST(
      &buffer,
      &tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  UINT32 response_size;
  std::string response_size_bytes;
  rc = Parse_UINT32(
      &buffer,
      &response_size,
      &response_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM_RC response_code;
  std::string response_code_bytes;
  rc = Parse_TPM_RC(
      &buffer,
      &response_code,
      &response_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (response_size != response.size()) {
    error_reporter.Run(TPM_RC_SIZE);
    return;
  }
  if (response_code != TPM_RC_SUCCESS) {
    error_reporter.Run(response_code);
    return;
  }
  TPM_CC command_code = TPM_CC_NV_DefineSpace;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string authorization_section_bytes;
  if (tag == TPM_ST_SESSIONS) {
    UINT32 parameter_section_size = buffer.size();
    rc = Parse_UINT32(&buffer, &parameter_section_size, NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    if (parameter_section_size > buffer.size()) {
      error_reporter.Run(TPM_RC_INSUFFICIENT);
      return;
    }
    authorization_section_bytes = buffer.substr(parameter_section_size);
    // Keep the parameter section in |buffer|.
    buffer.erase(parameter_section_size);
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(response_code_bytes.data(),
               response_code_bytes.size());
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(buffer.data(),
               buffer.size());
  std::string response_hash(32, 0);
  hash->Finish(string_as_array(&response_hash), response_hash.size());
  if (tag == TPM_ST_SESSIONS) {
    if (!authorization_delegate->CheckResponseAuthorization(
        response_hash,
        authorization_section_bytes)) {
      error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
      return;
    }
  }
  callback.Run(response_code);
}

void Tpm::NV_DefineSpace(
      TPMI_RH_PROVISION auth_handle,
      const std::string& auth_handle_name,
      TPM2B_AUTH auth,
      TPM2B_NV_PUBLIC public_info,
      AuthorizationDelegate* authorization_delegate,
      const NV_DefineSpaceResponse& callback) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(NV_DefineSpaceErrorCallback, callback);
  base::Callback<void(const std::string&)> parser =
      base::Bind(NV_DefineSpaceResponseParser,
                 callback,
                 authorization_delegate);
  TPMI_ST_COMMAND_TAG tag = TPM_ST_NO_SESSIONS;
  UINT32 command_size = 10;  // Header size.
  std::string handle_section_bytes;
  std::string parameter_section_bytes;
  TPM_CC command_code = TPM_CC_NV_DefineSpace;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string auth_handle_bytes;
  rc = Serialize_TPMI_RH_PROVISION(
      auth_handle,
      &auth_handle_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string auth_bytes;
  rc = Serialize_TPM2B_AUTH(
      auth,
      &auth_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string public_info_bytes;
  rc = Serialize_TPM2B_NV_PUBLIC(
      public_info,
      &public_info_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  // Encrypt just the parameter data, not the size.
  std::string tmp = auth_bytes.substr(2);
  if (!authorization_delegate->EncryptCommandParameter(&tmp)) {
    error_reporter.Run(TRUNKS_RC_ENCRYPTION_FAILED);
    return;
  }
  auth_bytes.replace(2, std::string::npos, tmp);
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(auth_handle_name.data(),
               auth_handle_name.size());
  handle_section_bytes += auth_handle_bytes;
  command_size += auth_handle_bytes.size();
  hash->Update(auth_bytes.data(),
               auth_bytes.size());
  parameter_section_bytes += auth_bytes;
  command_size += auth_bytes.size();
  hash->Update(public_info_bytes.data(),
               public_info_bytes.size());
  parameter_section_bytes += public_info_bytes;
  command_size += public_info_bytes.size();
  std::string command_hash(32, 0);
  hash->Finish(string_as_array(&command_hash), command_hash.size());
  std::string authorization_section_bytes;
  if (!authorization_delegate->GetCommandAuthorization(
      command_hash,
      &authorization_section_bytes)) {
    error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
    return;
  }
  std::string authorization_size_bytes;
  if (!authorization_section_bytes.empty()) {
    tag = TPM_ST_SESSIONS;
    std::string tmp;
    rc = Serialize_UINT32(authorization_section_bytes.size(),
                          &authorization_size_bytes);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    command_size += authorization_size_bytes.size() +
                    authorization_section_bytes.size();
  }
  std::string tag_bytes;
  rc = Serialize_TPMI_ST_COMMAND_TAG(
      tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command_size_bytes;
  rc = Serialize_UINT32(
      command_size,
      &command_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command = tag_bytes +
                        command_size_bytes +
                        command_code_bytes +
                        handle_section_bytes +
                        authorization_size_bytes +
                        authorization_section_bytes +
                        parameter_section_bytes;
  CHECK(command.size() == command_size) << "Command size mismatch!";
  transceiver_->SendCommand(command, parser);
}

void NV_UndefineSpaceErrorCallback(
    const Tpm::NV_UndefineSpaceResponse& callback,
    TPM_RC response_code) {
  callback.Run(response_code);
}

void NV_UndefineSpaceResponseParser(
    const Tpm::NV_UndefineSpaceResponse& callback,
    AuthorizationDelegate* authorization_delegate,
    const std::string& response) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(NV_UndefineSpaceErrorCallback, callback);
  std::string buffer(response);
  TPM_ST tag;
  std::string tag_bytes;
  rc = Parse_TPM_ST(
      &buffer,
      &tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  UINT32 response_size;
  std::string response_size_bytes;
  rc = Parse_UINT32(
      &buffer,
      &response_size,
      &response_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM_RC response_code;
  std::string response_code_bytes;
  rc = Parse_TPM_RC(
      &buffer,
      &response_code,
      &response_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (response_size != response.size()) {
    error_reporter.Run(TPM_RC_SIZE);
    return;
  }
  if (response_code != TPM_RC_SUCCESS) {
    error_reporter.Run(response_code);
    return;
  }
  TPM_CC command_code = TPM_CC_NV_UndefineSpace;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string authorization_section_bytes;
  if (tag == TPM_ST_SESSIONS) {
    UINT32 parameter_section_size = buffer.size();
    rc = Parse_UINT32(&buffer, &parameter_section_size, NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    if (parameter_section_size > buffer.size()) {
      error_reporter.Run(TPM_RC_INSUFFICIENT);
      return;
    }
    authorization_section_bytes = buffer.substr(parameter_section_size);
    // Keep the parameter section in |buffer|.
    buffer.erase(parameter_section_size);
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(response_code_bytes.data(),
               response_code_bytes.size());
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(buffer.data(),
               buffer.size());
  std::string response_hash(32, 0);
  hash->Finish(string_as_array(&response_hash), response_hash.size());
  if (tag == TPM_ST_SESSIONS) {
    if (!authorization_delegate->CheckResponseAuthorization(
        response_hash,
        authorization_section_bytes)) {
      error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
      return;
    }
  }
  callback.Run(response_code);
}

void Tpm::NV_UndefineSpace(
      TPMI_RH_PROVISION auth_handle,
      const std::string& auth_handle_name,
      TPMI_RH_NV_INDEX nv_index,
      const std::string& nv_index_name,
      AuthorizationDelegate* authorization_delegate,
      const NV_UndefineSpaceResponse& callback) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(NV_UndefineSpaceErrorCallback, callback);
  base::Callback<void(const std::string&)> parser =
      base::Bind(NV_UndefineSpaceResponseParser,
                 callback,
                 authorization_delegate);
  TPMI_ST_COMMAND_TAG tag = TPM_ST_NO_SESSIONS;
  UINT32 command_size = 10;  // Header size.
  std::string handle_section_bytes;
  std::string parameter_section_bytes;
  TPM_CC command_code = TPM_CC_NV_UndefineSpace;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string auth_handle_bytes;
  rc = Serialize_TPMI_RH_PROVISION(
      auth_handle,
      &auth_handle_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string nv_index_bytes;
  rc = Serialize_TPMI_RH_NV_INDEX(
      nv_index,
      &nv_index_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(auth_handle_name.data(),
               auth_handle_name.size());
  handle_section_bytes += auth_handle_bytes;
  command_size += auth_handle_bytes.size();
  hash->Update(nv_index_name.data(),
               nv_index_name.size());
  handle_section_bytes += nv_index_bytes;
  command_size += nv_index_bytes.size();
  std::string command_hash(32, 0);
  hash->Finish(string_as_array(&command_hash), command_hash.size());
  std::string authorization_section_bytes;
  if (!authorization_delegate->GetCommandAuthorization(
      command_hash,
      &authorization_section_bytes)) {
    error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
    return;
  }
  std::string authorization_size_bytes;
  if (!authorization_section_bytes.empty()) {
    tag = TPM_ST_SESSIONS;
    std::string tmp;
    rc = Serialize_UINT32(authorization_section_bytes.size(),
                          &authorization_size_bytes);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    command_size += authorization_size_bytes.size() +
                    authorization_section_bytes.size();
  }
  std::string tag_bytes;
  rc = Serialize_TPMI_ST_COMMAND_TAG(
      tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command_size_bytes;
  rc = Serialize_UINT32(
      command_size,
      &command_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command = tag_bytes +
                        command_size_bytes +
                        command_code_bytes +
                        handle_section_bytes +
                        authorization_size_bytes +
                        authorization_section_bytes +
                        parameter_section_bytes;
  CHECK(command.size() == command_size) << "Command size mismatch!";
  transceiver_->SendCommand(command, parser);
}

void NV_UndefineSpaceSpecialErrorCallback(
    const Tpm::NV_UndefineSpaceSpecialResponse& callback,
    TPM_RC response_code) {
  callback.Run(response_code);
}

void NV_UndefineSpaceSpecialResponseParser(
    const Tpm::NV_UndefineSpaceSpecialResponse& callback,
    AuthorizationDelegate* authorization_delegate,
    const std::string& response) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(NV_UndefineSpaceSpecialErrorCallback, callback);
  std::string buffer(response);
  TPM_ST tag;
  std::string tag_bytes;
  rc = Parse_TPM_ST(
      &buffer,
      &tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  UINT32 response_size;
  std::string response_size_bytes;
  rc = Parse_UINT32(
      &buffer,
      &response_size,
      &response_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM_RC response_code;
  std::string response_code_bytes;
  rc = Parse_TPM_RC(
      &buffer,
      &response_code,
      &response_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (response_size != response.size()) {
    error_reporter.Run(TPM_RC_SIZE);
    return;
  }
  if (response_code != TPM_RC_SUCCESS) {
    error_reporter.Run(response_code);
    return;
  }
  TPM_CC command_code = TPM_CC_NV_UndefineSpaceSpecial;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string authorization_section_bytes;
  if (tag == TPM_ST_SESSIONS) {
    UINT32 parameter_section_size = buffer.size();
    rc = Parse_UINT32(&buffer, &parameter_section_size, NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    if (parameter_section_size > buffer.size()) {
      error_reporter.Run(TPM_RC_INSUFFICIENT);
      return;
    }
    authorization_section_bytes = buffer.substr(parameter_section_size);
    // Keep the parameter section in |buffer|.
    buffer.erase(parameter_section_size);
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(response_code_bytes.data(),
               response_code_bytes.size());
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(buffer.data(),
               buffer.size());
  std::string response_hash(32, 0);
  hash->Finish(string_as_array(&response_hash), response_hash.size());
  if (tag == TPM_ST_SESSIONS) {
    if (!authorization_delegate->CheckResponseAuthorization(
        response_hash,
        authorization_section_bytes)) {
      error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
      return;
    }
  }
  callback.Run(response_code);
}

void Tpm::NV_UndefineSpaceSpecial(
      TPMI_RH_NV_INDEX nv_index,
      const std::string& nv_index_name,
      TPMI_RH_PLATFORM platform,
      const std::string& platform_name,
      AuthorizationDelegate* authorization_delegate,
      const NV_UndefineSpaceSpecialResponse& callback) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(NV_UndefineSpaceSpecialErrorCallback, callback);
  base::Callback<void(const std::string&)> parser =
      base::Bind(NV_UndefineSpaceSpecialResponseParser,
                 callback,
                 authorization_delegate);
  TPMI_ST_COMMAND_TAG tag = TPM_ST_NO_SESSIONS;
  UINT32 command_size = 10;  // Header size.
  std::string handle_section_bytes;
  std::string parameter_section_bytes;
  TPM_CC command_code = TPM_CC_NV_UndefineSpaceSpecial;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string nv_index_bytes;
  rc = Serialize_TPMI_RH_NV_INDEX(
      nv_index,
      &nv_index_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string platform_bytes;
  rc = Serialize_TPMI_RH_PLATFORM(
      platform,
      &platform_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(nv_index_name.data(),
               nv_index_name.size());
  handle_section_bytes += nv_index_bytes;
  command_size += nv_index_bytes.size();
  hash->Update(platform_name.data(),
               platform_name.size());
  handle_section_bytes += platform_bytes;
  command_size += platform_bytes.size();
  std::string command_hash(32, 0);
  hash->Finish(string_as_array(&command_hash), command_hash.size());
  std::string authorization_section_bytes;
  if (!authorization_delegate->GetCommandAuthorization(
      command_hash,
      &authorization_section_bytes)) {
    error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
    return;
  }
  std::string authorization_size_bytes;
  if (!authorization_section_bytes.empty()) {
    tag = TPM_ST_SESSIONS;
    std::string tmp;
    rc = Serialize_UINT32(authorization_section_bytes.size(),
                          &authorization_size_bytes);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    command_size += authorization_size_bytes.size() +
                    authorization_section_bytes.size();
  }
  std::string tag_bytes;
  rc = Serialize_TPMI_ST_COMMAND_TAG(
      tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command_size_bytes;
  rc = Serialize_UINT32(
      command_size,
      &command_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command = tag_bytes +
                        command_size_bytes +
                        command_code_bytes +
                        handle_section_bytes +
                        authorization_size_bytes +
                        authorization_section_bytes +
                        parameter_section_bytes;
  CHECK(command.size() == command_size) << "Command size mismatch!";
  transceiver_->SendCommand(command, parser);
}

void NV_ReadPublicErrorCallback(
    const Tpm::NV_ReadPublicResponse& callback,
    TPM_RC response_code) {
  callback.Run(response_code,
               TPM2B_NV_PUBLIC(),
               TPM2B_NAME());
}

void NV_ReadPublicResponseParser(
    const Tpm::NV_ReadPublicResponse& callback,
    AuthorizationDelegate* authorization_delegate,
    const std::string& response) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(NV_ReadPublicErrorCallback, callback);
  std::string buffer(response);
  TPM_ST tag;
  std::string tag_bytes;
  rc = Parse_TPM_ST(
      &buffer,
      &tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  UINT32 response_size;
  std::string response_size_bytes;
  rc = Parse_UINT32(
      &buffer,
      &response_size,
      &response_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM_RC response_code;
  std::string response_code_bytes;
  rc = Parse_TPM_RC(
      &buffer,
      &response_code,
      &response_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (response_size != response.size()) {
    error_reporter.Run(TPM_RC_SIZE);
    return;
  }
  if (response_code != TPM_RC_SUCCESS) {
    error_reporter.Run(response_code);
    return;
  }
  TPM_CC command_code = TPM_CC_NV_ReadPublic;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string authorization_section_bytes;
  if (tag == TPM_ST_SESSIONS) {
    UINT32 parameter_section_size = buffer.size();
    rc = Parse_UINT32(&buffer, &parameter_section_size, NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    if (parameter_section_size > buffer.size()) {
      error_reporter.Run(TPM_RC_INSUFFICIENT);
      return;
    }
    authorization_section_bytes = buffer.substr(parameter_section_size);
    // Keep the parameter section in |buffer|.
    buffer.erase(parameter_section_size);
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(response_code_bytes.data(),
               response_code_bytes.size());
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(buffer.data(),
               buffer.size());
  std::string response_hash(32, 0);
  hash->Finish(string_as_array(&response_hash), response_hash.size());
  if (tag == TPM_ST_SESSIONS) {
    if (!authorization_delegate->CheckResponseAuthorization(
        response_hash,
        authorization_section_bytes)) {
      error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
      return;
    }
  }
  TPM2B_NV_PUBLIC nv_public;
  std::string nv_public_bytes;
  rc = Parse_TPM2B_NV_PUBLIC(
      &buffer,
      &nv_public,
      &nv_public_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM2B_NAME nv_name;
  std::string nv_name_bytes;
  rc = Parse_TPM2B_NAME(
      &buffer,
      &nv_name,
      &nv_name_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (tag == TPM_ST_SESSIONS) {
    // Decrypt just the parameter data, not the size.
    std::string tmp = nv_public_bytes.substr(2);
    if (!authorization_delegate->DecryptResponseParameter(&tmp)) {
      error_reporter.Run(TRUNKS_RC_ENCRYPTION_FAILED);
      return;
    }
    nv_public_bytes.replace(2, std::string::npos, tmp);
    rc = Parse_TPM2B_NV_PUBLIC(
        &nv_public_bytes,
        &nv_public,
        NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
  }
  callback.Run(response_code,
               nv_public,
               nv_name);
}

void Tpm::NV_ReadPublic(
      TPMI_RH_NV_INDEX nv_index,
      const std::string& nv_index_name,
      AuthorizationDelegate* authorization_delegate,
      const NV_ReadPublicResponse& callback) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(NV_ReadPublicErrorCallback, callback);
  base::Callback<void(const std::string&)> parser =
      base::Bind(NV_ReadPublicResponseParser,
                 callback,
                 authorization_delegate);
  TPMI_ST_COMMAND_TAG tag = TPM_ST_NO_SESSIONS;
  UINT32 command_size = 10;  // Header size.
  std::string handle_section_bytes;
  std::string parameter_section_bytes;
  TPM_CC command_code = TPM_CC_NV_ReadPublic;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string nv_index_bytes;
  rc = Serialize_TPMI_RH_NV_INDEX(
      nv_index,
      &nv_index_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(nv_index_name.data(),
               nv_index_name.size());
  handle_section_bytes += nv_index_bytes;
  command_size += nv_index_bytes.size();
  std::string command_hash(32, 0);
  hash->Finish(string_as_array(&command_hash), command_hash.size());
  std::string authorization_section_bytes;
  if (!authorization_delegate->GetCommandAuthorization(
      command_hash,
      &authorization_section_bytes)) {
    error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
    return;
  }
  std::string authorization_size_bytes;
  if (!authorization_section_bytes.empty()) {
    tag = TPM_ST_SESSIONS;
    std::string tmp;
    rc = Serialize_UINT32(authorization_section_bytes.size(),
                          &authorization_size_bytes);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    command_size += authorization_size_bytes.size() +
                    authorization_section_bytes.size();
  }
  std::string tag_bytes;
  rc = Serialize_TPMI_ST_COMMAND_TAG(
      tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command_size_bytes;
  rc = Serialize_UINT32(
      command_size,
      &command_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command = tag_bytes +
                        command_size_bytes +
                        command_code_bytes +
                        handle_section_bytes +
                        authorization_size_bytes +
                        authorization_section_bytes +
                        parameter_section_bytes;
  CHECK(command.size() == command_size) << "Command size mismatch!";
  transceiver_->SendCommand(command, parser);
}

void NV_WriteErrorCallback(
    const Tpm::NV_WriteResponse& callback,
    TPM_RC response_code) {
  callback.Run(response_code);
}

void NV_WriteResponseParser(
    const Tpm::NV_WriteResponse& callback,
    AuthorizationDelegate* authorization_delegate,
    const std::string& response) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(NV_WriteErrorCallback, callback);
  std::string buffer(response);
  TPM_ST tag;
  std::string tag_bytes;
  rc = Parse_TPM_ST(
      &buffer,
      &tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  UINT32 response_size;
  std::string response_size_bytes;
  rc = Parse_UINT32(
      &buffer,
      &response_size,
      &response_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM_RC response_code;
  std::string response_code_bytes;
  rc = Parse_TPM_RC(
      &buffer,
      &response_code,
      &response_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (response_size != response.size()) {
    error_reporter.Run(TPM_RC_SIZE);
    return;
  }
  if (response_code != TPM_RC_SUCCESS) {
    error_reporter.Run(response_code);
    return;
  }
  TPM_CC command_code = TPM_CC_NV_Write;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string authorization_section_bytes;
  if (tag == TPM_ST_SESSIONS) {
    UINT32 parameter_section_size = buffer.size();
    rc = Parse_UINT32(&buffer, &parameter_section_size, NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    if (parameter_section_size > buffer.size()) {
      error_reporter.Run(TPM_RC_INSUFFICIENT);
      return;
    }
    authorization_section_bytes = buffer.substr(parameter_section_size);
    // Keep the parameter section in |buffer|.
    buffer.erase(parameter_section_size);
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(response_code_bytes.data(),
               response_code_bytes.size());
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(buffer.data(),
               buffer.size());
  std::string response_hash(32, 0);
  hash->Finish(string_as_array(&response_hash), response_hash.size());
  if (tag == TPM_ST_SESSIONS) {
    if (!authorization_delegate->CheckResponseAuthorization(
        response_hash,
        authorization_section_bytes)) {
      error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
      return;
    }
  }
  callback.Run(response_code);
}

void Tpm::NV_Write(
      TPMI_RH_NV_AUTH auth_handle,
      const std::string& auth_handle_name,
      TPMI_RH_NV_INDEX nv_index,
      const std::string& nv_index_name,
      TPM2B_MAX_NV_BUFFER data,
      UINT16 offset,
      AuthorizationDelegate* authorization_delegate,
      const NV_WriteResponse& callback) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(NV_WriteErrorCallback, callback);
  base::Callback<void(const std::string&)> parser =
      base::Bind(NV_WriteResponseParser,
                 callback,
                 authorization_delegate);
  TPMI_ST_COMMAND_TAG tag = TPM_ST_NO_SESSIONS;
  UINT32 command_size = 10;  // Header size.
  std::string handle_section_bytes;
  std::string parameter_section_bytes;
  TPM_CC command_code = TPM_CC_NV_Write;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string auth_handle_bytes;
  rc = Serialize_TPMI_RH_NV_AUTH(
      auth_handle,
      &auth_handle_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string nv_index_bytes;
  rc = Serialize_TPMI_RH_NV_INDEX(
      nv_index,
      &nv_index_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string data_bytes;
  rc = Serialize_TPM2B_MAX_NV_BUFFER(
      data,
      &data_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string offset_bytes;
  rc = Serialize_UINT16(
      offset,
      &offset_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  // Encrypt just the parameter data, not the size.
  std::string tmp = data_bytes.substr(2);
  if (!authorization_delegate->EncryptCommandParameter(&tmp)) {
    error_reporter.Run(TRUNKS_RC_ENCRYPTION_FAILED);
    return;
  }
  data_bytes.replace(2, std::string::npos, tmp);
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(auth_handle_name.data(),
               auth_handle_name.size());
  handle_section_bytes += auth_handle_bytes;
  command_size += auth_handle_bytes.size();
  hash->Update(nv_index_name.data(),
               nv_index_name.size());
  handle_section_bytes += nv_index_bytes;
  command_size += nv_index_bytes.size();
  hash->Update(data_bytes.data(),
               data_bytes.size());
  parameter_section_bytes += data_bytes;
  command_size += data_bytes.size();
  hash->Update(offset_bytes.data(),
               offset_bytes.size());
  parameter_section_bytes += offset_bytes;
  command_size += offset_bytes.size();
  std::string command_hash(32, 0);
  hash->Finish(string_as_array(&command_hash), command_hash.size());
  std::string authorization_section_bytes;
  if (!authorization_delegate->GetCommandAuthorization(
      command_hash,
      &authorization_section_bytes)) {
    error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
    return;
  }
  std::string authorization_size_bytes;
  if (!authorization_section_bytes.empty()) {
    tag = TPM_ST_SESSIONS;
    std::string tmp;
    rc = Serialize_UINT32(authorization_section_bytes.size(),
                          &authorization_size_bytes);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    command_size += authorization_size_bytes.size() +
                    authorization_section_bytes.size();
  }
  std::string tag_bytes;
  rc = Serialize_TPMI_ST_COMMAND_TAG(
      tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command_size_bytes;
  rc = Serialize_UINT32(
      command_size,
      &command_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command = tag_bytes +
                        command_size_bytes +
                        command_code_bytes +
                        handle_section_bytes +
                        authorization_size_bytes +
                        authorization_section_bytes +
                        parameter_section_bytes;
  CHECK(command.size() == command_size) << "Command size mismatch!";
  transceiver_->SendCommand(command, parser);
}

void NV_IncrementErrorCallback(
    const Tpm::NV_IncrementResponse& callback,
    TPM_RC response_code) {
  callback.Run(response_code);
}

void NV_IncrementResponseParser(
    const Tpm::NV_IncrementResponse& callback,
    AuthorizationDelegate* authorization_delegate,
    const std::string& response) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(NV_IncrementErrorCallback, callback);
  std::string buffer(response);
  TPM_ST tag;
  std::string tag_bytes;
  rc = Parse_TPM_ST(
      &buffer,
      &tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  UINT32 response_size;
  std::string response_size_bytes;
  rc = Parse_UINT32(
      &buffer,
      &response_size,
      &response_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM_RC response_code;
  std::string response_code_bytes;
  rc = Parse_TPM_RC(
      &buffer,
      &response_code,
      &response_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (response_size != response.size()) {
    error_reporter.Run(TPM_RC_SIZE);
    return;
  }
  if (response_code != TPM_RC_SUCCESS) {
    error_reporter.Run(response_code);
    return;
  }
  TPM_CC command_code = TPM_CC_NV_Increment;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string authorization_section_bytes;
  if (tag == TPM_ST_SESSIONS) {
    UINT32 parameter_section_size = buffer.size();
    rc = Parse_UINT32(&buffer, &parameter_section_size, NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    if (parameter_section_size > buffer.size()) {
      error_reporter.Run(TPM_RC_INSUFFICIENT);
      return;
    }
    authorization_section_bytes = buffer.substr(parameter_section_size);
    // Keep the parameter section in |buffer|.
    buffer.erase(parameter_section_size);
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(response_code_bytes.data(),
               response_code_bytes.size());
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(buffer.data(),
               buffer.size());
  std::string response_hash(32, 0);
  hash->Finish(string_as_array(&response_hash), response_hash.size());
  if (tag == TPM_ST_SESSIONS) {
    if (!authorization_delegate->CheckResponseAuthorization(
        response_hash,
        authorization_section_bytes)) {
      error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
      return;
    }
  }
  callback.Run(response_code);
}

void Tpm::NV_Increment(
      TPMI_RH_NV_AUTH auth_handle,
      const std::string& auth_handle_name,
      TPMI_RH_NV_INDEX nv_index,
      const std::string& nv_index_name,
      AuthorizationDelegate* authorization_delegate,
      const NV_IncrementResponse& callback) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(NV_IncrementErrorCallback, callback);
  base::Callback<void(const std::string&)> parser =
      base::Bind(NV_IncrementResponseParser,
                 callback,
                 authorization_delegate);
  TPMI_ST_COMMAND_TAG tag = TPM_ST_NO_SESSIONS;
  UINT32 command_size = 10;  // Header size.
  std::string handle_section_bytes;
  std::string parameter_section_bytes;
  TPM_CC command_code = TPM_CC_NV_Increment;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string auth_handle_bytes;
  rc = Serialize_TPMI_RH_NV_AUTH(
      auth_handle,
      &auth_handle_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string nv_index_bytes;
  rc = Serialize_TPMI_RH_NV_INDEX(
      nv_index,
      &nv_index_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(auth_handle_name.data(),
               auth_handle_name.size());
  handle_section_bytes += auth_handle_bytes;
  command_size += auth_handle_bytes.size();
  hash->Update(nv_index_name.data(),
               nv_index_name.size());
  handle_section_bytes += nv_index_bytes;
  command_size += nv_index_bytes.size();
  std::string command_hash(32, 0);
  hash->Finish(string_as_array(&command_hash), command_hash.size());
  std::string authorization_section_bytes;
  if (!authorization_delegate->GetCommandAuthorization(
      command_hash,
      &authorization_section_bytes)) {
    error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
    return;
  }
  std::string authorization_size_bytes;
  if (!authorization_section_bytes.empty()) {
    tag = TPM_ST_SESSIONS;
    std::string tmp;
    rc = Serialize_UINT32(authorization_section_bytes.size(),
                          &authorization_size_bytes);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    command_size += authorization_size_bytes.size() +
                    authorization_section_bytes.size();
  }
  std::string tag_bytes;
  rc = Serialize_TPMI_ST_COMMAND_TAG(
      tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command_size_bytes;
  rc = Serialize_UINT32(
      command_size,
      &command_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command = tag_bytes +
                        command_size_bytes +
                        command_code_bytes +
                        handle_section_bytes +
                        authorization_size_bytes +
                        authorization_section_bytes +
                        parameter_section_bytes;
  CHECK(command.size() == command_size) << "Command size mismatch!";
  transceiver_->SendCommand(command, parser);
}

void NV_ExtendErrorCallback(
    const Tpm::NV_ExtendResponse& callback,
    TPM_RC response_code) {
  callback.Run(response_code);
}

void NV_ExtendResponseParser(
    const Tpm::NV_ExtendResponse& callback,
    AuthorizationDelegate* authorization_delegate,
    const std::string& response) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(NV_ExtendErrorCallback, callback);
  std::string buffer(response);
  TPM_ST tag;
  std::string tag_bytes;
  rc = Parse_TPM_ST(
      &buffer,
      &tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  UINT32 response_size;
  std::string response_size_bytes;
  rc = Parse_UINT32(
      &buffer,
      &response_size,
      &response_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM_RC response_code;
  std::string response_code_bytes;
  rc = Parse_TPM_RC(
      &buffer,
      &response_code,
      &response_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (response_size != response.size()) {
    error_reporter.Run(TPM_RC_SIZE);
    return;
  }
  if (response_code != TPM_RC_SUCCESS) {
    error_reporter.Run(response_code);
    return;
  }
  TPM_CC command_code = TPM_CC_NV_Extend;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string authorization_section_bytes;
  if (tag == TPM_ST_SESSIONS) {
    UINT32 parameter_section_size = buffer.size();
    rc = Parse_UINT32(&buffer, &parameter_section_size, NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    if (parameter_section_size > buffer.size()) {
      error_reporter.Run(TPM_RC_INSUFFICIENT);
      return;
    }
    authorization_section_bytes = buffer.substr(parameter_section_size);
    // Keep the parameter section in |buffer|.
    buffer.erase(parameter_section_size);
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(response_code_bytes.data(),
               response_code_bytes.size());
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(buffer.data(),
               buffer.size());
  std::string response_hash(32, 0);
  hash->Finish(string_as_array(&response_hash), response_hash.size());
  if (tag == TPM_ST_SESSIONS) {
    if (!authorization_delegate->CheckResponseAuthorization(
        response_hash,
        authorization_section_bytes)) {
      error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
      return;
    }
  }
  callback.Run(response_code);
}

void Tpm::NV_Extend(
      TPMI_RH_NV_AUTH auth_handle,
      const std::string& auth_handle_name,
      TPMI_RH_NV_INDEX nv_index,
      const std::string& nv_index_name,
      TPM2B_MAX_NV_BUFFER data,
      AuthorizationDelegate* authorization_delegate,
      const NV_ExtendResponse& callback) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(NV_ExtendErrorCallback, callback);
  base::Callback<void(const std::string&)> parser =
      base::Bind(NV_ExtendResponseParser,
                 callback,
                 authorization_delegate);
  TPMI_ST_COMMAND_TAG tag = TPM_ST_NO_SESSIONS;
  UINT32 command_size = 10;  // Header size.
  std::string handle_section_bytes;
  std::string parameter_section_bytes;
  TPM_CC command_code = TPM_CC_NV_Extend;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string auth_handle_bytes;
  rc = Serialize_TPMI_RH_NV_AUTH(
      auth_handle,
      &auth_handle_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string nv_index_bytes;
  rc = Serialize_TPMI_RH_NV_INDEX(
      nv_index,
      &nv_index_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string data_bytes;
  rc = Serialize_TPM2B_MAX_NV_BUFFER(
      data,
      &data_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  // Encrypt just the parameter data, not the size.
  std::string tmp = data_bytes.substr(2);
  if (!authorization_delegate->EncryptCommandParameter(&tmp)) {
    error_reporter.Run(TRUNKS_RC_ENCRYPTION_FAILED);
    return;
  }
  data_bytes.replace(2, std::string::npos, tmp);
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(auth_handle_name.data(),
               auth_handle_name.size());
  handle_section_bytes += auth_handle_bytes;
  command_size += auth_handle_bytes.size();
  hash->Update(nv_index_name.data(),
               nv_index_name.size());
  handle_section_bytes += nv_index_bytes;
  command_size += nv_index_bytes.size();
  hash->Update(data_bytes.data(),
               data_bytes.size());
  parameter_section_bytes += data_bytes;
  command_size += data_bytes.size();
  std::string command_hash(32, 0);
  hash->Finish(string_as_array(&command_hash), command_hash.size());
  std::string authorization_section_bytes;
  if (!authorization_delegate->GetCommandAuthorization(
      command_hash,
      &authorization_section_bytes)) {
    error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
    return;
  }
  std::string authorization_size_bytes;
  if (!authorization_section_bytes.empty()) {
    tag = TPM_ST_SESSIONS;
    std::string tmp;
    rc = Serialize_UINT32(authorization_section_bytes.size(),
                          &authorization_size_bytes);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    command_size += authorization_size_bytes.size() +
                    authorization_section_bytes.size();
  }
  std::string tag_bytes;
  rc = Serialize_TPMI_ST_COMMAND_TAG(
      tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command_size_bytes;
  rc = Serialize_UINT32(
      command_size,
      &command_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command = tag_bytes +
                        command_size_bytes +
                        command_code_bytes +
                        handle_section_bytes +
                        authorization_size_bytes +
                        authorization_section_bytes +
                        parameter_section_bytes;
  CHECK(command.size() == command_size) << "Command size mismatch!";
  transceiver_->SendCommand(command, parser);
}

void NV_SetBitsErrorCallback(
    const Tpm::NV_SetBitsResponse& callback,
    TPM_RC response_code) {
  callback.Run(response_code);
}

void NV_SetBitsResponseParser(
    const Tpm::NV_SetBitsResponse& callback,
    AuthorizationDelegate* authorization_delegate,
    const std::string& response) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(NV_SetBitsErrorCallback, callback);
  std::string buffer(response);
  TPM_ST tag;
  std::string tag_bytes;
  rc = Parse_TPM_ST(
      &buffer,
      &tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  UINT32 response_size;
  std::string response_size_bytes;
  rc = Parse_UINT32(
      &buffer,
      &response_size,
      &response_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM_RC response_code;
  std::string response_code_bytes;
  rc = Parse_TPM_RC(
      &buffer,
      &response_code,
      &response_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (response_size != response.size()) {
    error_reporter.Run(TPM_RC_SIZE);
    return;
  }
  if (response_code != TPM_RC_SUCCESS) {
    error_reporter.Run(response_code);
    return;
  }
  TPM_CC command_code = TPM_CC_NV_SetBits;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string authorization_section_bytes;
  if (tag == TPM_ST_SESSIONS) {
    UINT32 parameter_section_size = buffer.size();
    rc = Parse_UINT32(&buffer, &parameter_section_size, NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    if (parameter_section_size > buffer.size()) {
      error_reporter.Run(TPM_RC_INSUFFICIENT);
      return;
    }
    authorization_section_bytes = buffer.substr(parameter_section_size);
    // Keep the parameter section in |buffer|.
    buffer.erase(parameter_section_size);
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(response_code_bytes.data(),
               response_code_bytes.size());
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(buffer.data(),
               buffer.size());
  std::string response_hash(32, 0);
  hash->Finish(string_as_array(&response_hash), response_hash.size());
  if (tag == TPM_ST_SESSIONS) {
    if (!authorization_delegate->CheckResponseAuthorization(
        response_hash,
        authorization_section_bytes)) {
      error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
      return;
    }
  }
  callback.Run(response_code);
}

void Tpm::NV_SetBits(
      TPMI_RH_NV_AUTH auth_handle,
      const std::string& auth_handle_name,
      TPMI_RH_NV_INDEX nv_index,
      const std::string& nv_index_name,
      UINT64 bits,
      AuthorizationDelegate* authorization_delegate,
      const NV_SetBitsResponse& callback) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(NV_SetBitsErrorCallback, callback);
  base::Callback<void(const std::string&)> parser =
      base::Bind(NV_SetBitsResponseParser,
                 callback,
                 authorization_delegate);
  TPMI_ST_COMMAND_TAG tag = TPM_ST_NO_SESSIONS;
  UINT32 command_size = 10;  // Header size.
  std::string handle_section_bytes;
  std::string parameter_section_bytes;
  TPM_CC command_code = TPM_CC_NV_SetBits;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string auth_handle_bytes;
  rc = Serialize_TPMI_RH_NV_AUTH(
      auth_handle,
      &auth_handle_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string nv_index_bytes;
  rc = Serialize_TPMI_RH_NV_INDEX(
      nv_index,
      &nv_index_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string bits_bytes;
  rc = Serialize_UINT64(
      bits,
      &bits_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(auth_handle_name.data(),
               auth_handle_name.size());
  handle_section_bytes += auth_handle_bytes;
  command_size += auth_handle_bytes.size();
  hash->Update(nv_index_name.data(),
               nv_index_name.size());
  handle_section_bytes += nv_index_bytes;
  command_size += nv_index_bytes.size();
  hash->Update(bits_bytes.data(),
               bits_bytes.size());
  parameter_section_bytes += bits_bytes;
  command_size += bits_bytes.size();
  std::string command_hash(32, 0);
  hash->Finish(string_as_array(&command_hash), command_hash.size());
  std::string authorization_section_bytes;
  if (!authorization_delegate->GetCommandAuthorization(
      command_hash,
      &authorization_section_bytes)) {
    error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
    return;
  }
  std::string authorization_size_bytes;
  if (!authorization_section_bytes.empty()) {
    tag = TPM_ST_SESSIONS;
    std::string tmp;
    rc = Serialize_UINT32(authorization_section_bytes.size(),
                          &authorization_size_bytes);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    command_size += authorization_size_bytes.size() +
                    authorization_section_bytes.size();
  }
  std::string tag_bytes;
  rc = Serialize_TPMI_ST_COMMAND_TAG(
      tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command_size_bytes;
  rc = Serialize_UINT32(
      command_size,
      &command_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command = tag_bytes +
                        command_size_bytes +
                        command_code_bytes +
                        handle_section_bytes +
                        authorization_size_bytes +
                        authorization_section_bytes +
                        parameter_section_bytes;
  CHECK(command.size() == command_size) << "Command size mismatch!";
  transceiver_->SendCommand(command, parser);
}

void NV_WriteLockErrorCallback(
    const Tpm::NV_WriteLockResponse& callback,
    TPM_RC response_code) {
  callback.Run(response_code);
}

void NV_WriteLockResponseParser(
    const Tpm::NV_WriteLockResponse& callback,
    AuthorizationDelegate* authorization_delegate,
    const std::string& response) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(NV_WriteLockErrorCallback, callback);
  std::string buffer(response);
  TPM_ST tag;
  std::string tag_bytes;
  rc = Parse_TPM_ST(
      &buffer,
      &tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  UINT32 response_size;
  std::string response_size_bytes;
  rc = Parse_UINT32(
      &buffer,
      &response_size,
      &response_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM_RC response_code;
  std::string response_code_bytes;
  rc = Parse_TPM_RC(
      &buffer,
      &response_code,
      &response_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (response_size != response.size()) {
    error_reporter.Run(TPM_RC_SIZE);
    return;
  }
  if (response_code != TPM_RC_SUCCESS) {
    error_reporter.Run(response_code);
    return;
  }
  TPM_CC command_code = TPM_CC_NV_WriteLock;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string authorization_section_bytes;
  if (tag == TPM_ST_SESSIONS) {
    UINT32 parameter_section_size = buffer.size();
    rc = Parse_UINT32(&buffer, &parameter_section_size, NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    if (parameter_section_size > buffer.size()) {
      error_reporter.Run(TPM_RC_INSUFFICIENT);
      return;
    }
    authorization_section_bytes = buffer.substr(parameter_section_size);
    // Keep the parameter section in |buffer|.
    buffer.erase(parameter_section_size);
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(response_code_bytes.data(),
               response_code_bytes.size());
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(buffer.data(),
               buffer.size());
  std::string response_hash(32, 0);
  hash->Finish(string_as_array(&response_hash), response_hash.size());
  if (tag == TPM_ST_SESSIONS) {
    if (!authorization_delegate->CheckResponseAuthorization(
        response_hash,
        authorization_section_bytes)) {
      error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
      return;
    }
  }
  callback.Run(response_code);
}

void Tpm::NV_WriteLock(
      TPMI_RH_NV_AUTH auth_handle,
      const std::string& auth_handle_name,
      TPMI_RH_NV_INDEX nv_index,
      const std::string& nv_index_name,
      AuthorizationDelegate* authorization_delegate,
      const NV_WriteLockResponse& callback) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(NV_WriteLockErrorCallback, callback);
  base::Callback<void(const std::string&)> parser =
      base::Bind(NV_WriteLockResponseParser,
                 callback,
                 authorization_delegate);
  TPMI_ST_COMMAND_TAG tag = TPM_ST_NO_SESSIONS;
  UINT32 command_size = 10;  // Header size.
  std::string handle_section_bytes;
  std::string parameter_section_bytes;
  TPM_CC command_code = TPM_CC_NV_WriteLock;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string auth_handle_bytes;
  rc = Serialize_TPMI_RH_NV_AUTH(
      auth_handle,
      &auth_handle_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string nv_index_bytes;
  rc = Serialize_TPMI_RH_NV_INDEX(
      nv_index,
      &nv_index_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(auth_handle_name.data(),
               auth_handle_name.size());
  handle_section_bytes += auth_handle_bytes;
  command_size += auth_handle_bytes.size();
  hash->Update(nv_index_name.data(),
               nv_index_name.size());
  handle_section_bytes += nv_index_bytes;
  command_size += nv_index_bytes.size();
  std::string command_hash(32, 0);
  hash->Finish(string_as_array(&command_hash), command_hash.size());
  std::string authorization_section_bytes;
  if (!authorization_delegate->GetCommandAuthorization(
      command_hash,
      &authorization_section_bytes)) {
    error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
    return;
  }
  std::string authorization_size_bytes;
  if (!authorization_section_bytes.empty()) {
    tag = TPM_ST_SESSIONS;
    std::string tmp;
    rc = Serialize_UINT32(authorization_section_bytes.size(),
                          &authorization_size_bytes);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    command_size += authorization_size_bytes.size() +
                    authorization_section_bytes.size();
  }
  std::string tag_bytes;
  rc = Serialize_TPMI_ST_COMMAND_TAG(
      tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command_size_bytes;
  rc = Serialize_UINT32(
      command_size,
      &command_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command = tag_bytes +
                        command_size_bytes +
                        command_code_bytes +
                        handle_section_bytes +
                        authorization_size_bytes +
                        authorization_section_bytes +
                        parameter_section_bytes;
  CHECK(command.size() == command_size) << "Command size mismatch!";
  transceiver_->SendCommand(command, parser);
}

void NV_GlobalWriteLockErrorCallback(
    const Tpm::NV_GlobalWriteLockResponse& callback,
    TPM_RC response_code) {
  callback.Run(response_code);
}

void NV_GlobalWriteLockResponseParser(
    const Tpm::NV_GlobalWriteLockResponse& callback,
    AuthorizationDelegate* authorization_delegate,
    const std::string& response) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(NV_GlobalWriteLockErrorCallback, callback);
  std::string buffer(response);
  TPM_ST tag;
  std::string tag_bytes;
  rc = Parse_TPM_ST(
      &buffer,
      &tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  UINT32 response_size;
  std::string response_size_bytes;
  rc = Parse_UINT32(
      &buffer,
      &response_size,
      &response_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM_RC response_code;
  std::string response_code_bytes;
  rc = Parse_TPM_RC(
      &buffer,
      &response_code,
      &response_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (response_size != response.size()) {
    error_reporter.Run(TPM_RC_SIZE);
    return;
  }
  if (response_code != TPM_RC_SUCCESS) {
    error_reporter.Run(response_code);
    return;
  }
  TPM_CC command_code = TPM_CC_NV_GlobalWriteLock;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string authorization_section_bytes;
  if (tag == TPM_ST_SESSIONS) {
    UINT32 parameter_section_size = buffer.size();
    rc = Parse_UINT32(&buffer, &parameter_section_size, NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    if (parameter_section_size > buffer.size()) {
      error_reporter.Run(TPM_RC_INSUFFICIENT);
      return;
    }
    authorization_section_bytes = buffer.substr(parameter_section_size);
    // Keep the parameter section in |buffer|.
    buffer.erase(parameter_section_size);
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(response_code_bytes.data(),
               response_code_bytes.size());
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(buffer.data(),
               buffer.size());
  std::string response_hash(32, 0);
  hash->Finish(string_as_array(&response_hash), response_hash.size());
  if (tag == TPM_ST_SESSIONS) {
    if (!authorization_delegate->CheckResponseAuthorization(
        response_hash,
        authorization_section_bytes)) {
      error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
      return;
    }
  }
  callback.Run(response_code);
}

void Tpm::NV_GlobalWriteLock(
      TPMI_RH_PROVISION auth_handle,
      const std::string& auth_handle_name,
      AuthorizationDelegate* authorization_delegate,
      const NV_GlobalWriteLockResponse& callback) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(NV_GlobalWriteLockErrorCallback, callback);
  base::Callback<void(const std::string&)> parser =
      base::Bind(NV_GlobalWriteLockResponseParser,
                 callback,
                 authorization_delegate);
  TPMI_ST_COMMAND_TAG tag = TPM_ST_NO_SESSIONS;
  UINT32 command_size = 10;  // Header size.
  std::string handle_section_bytes;
  std::string parameter_section_bytes;
  TPM_CC command_code = TPM_CC_NV_GlobalWriteLock;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string auth_handle_bytes;
  rc = Serialize_TPMI_RH_PROVISION(
      auth_handle,
      &auth_handle_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(auth_handle_name.data(),
               auth_handle_name.size());
  handle_section_bytes += auth_handle_bytes;
  command_size += auth_handle_bytes.size();
  std::string command_hash(32, 0);
  hash->Finish(string_as_array(&command_hash), command_hash.size());
  std::string authorization_section_bytes;
  if (!authorization_delegate->GetCommandAuthorization(
      command_hash,
      &authorization_section_bytes)) {
    error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
    return;
  }
  std::string authorization_size_bytes;
  if (!authorization_section_bytes.empty()) {
    tag = TPM_ST_SESSIONS;
    std::string tmp;
    rc = Serialize_UINT32(authorization_section_bytes.size(),
                          &authorization_size_bytes);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    command_size += authorization_size_bytes.size() +
                    authorization_section_bytes.size();
  }
  std::string tag_bytes;
  rc = Serialize_TPMI_ST_COMMAND_TAG(
      tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command_size_bytes;
  rc = Serialize_UINT32(
      command_size,
      &command_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command = tag_bytes +
                        command_size_bytes +
                        command_code_bytes +
                        handle_section_bytes +
                        authorization_size_bytes +
                        authorization_section_bytes +
                        parameter_section_bytes;
  CHECK(command.size() == command_size) << "Command size mismatch!";
  transceiver_->SendCommand(command, parser);
}

void NV_ReadErrorCallback(
    const Tpm::NV_ReadResponse& callback,
    TPM_RC response_code) {
  callback.Run(response_code,
               TPM2B_MAX_NV_BUFFER());
}

void NV_ReadResponseParser(
    const Tpm::NV_ReadResponse& callback,
    AuthorizationDelegate* authorization_delegate,
    const std::string& response) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(NV_ReadErrorCallback, callback);
  std::string buffer(response);
  TPM_ST tag;
  std::string tag_bytes;
  rc = Parse_TPM_ST(
      &buffer,
      &tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  UINT32 response_size;
  std::string response_size_bytes;
  rc = Parse_UINT32(
      &buffer,
      &response_size,
      &response_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM_RC response_code;
  std::string response_code_bytes;
  rc = Parse_TPM_RC(
      &buffer,
      &response_code,
      &response_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (response_size != response.size()) {
    error_reporter.Run(TPM_RC_SIZE);
    return;
  }
  if (response_code != TPM_RC_SUCCESS) {
    error_reporter.Run(response_code);
    return;
  }
  TPM_CC command_code = TPM_CC_NV_Read;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string authorization_section_bytes;
  if (tag == TPM_ST_SESSIONS) {
    UINT32 parameter_section_size = buffer.size();
    rc = Parse_UINT32(&buffer, &parameter_section_size, NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    if (parameter_section_size > buffer.size()) {
      error_reporter.Run(TPM_RC_INSUFFICIENT);
      return;
    }
    authorization_section_bytes = buffer.substr(parameter_section_size);
    // Keep the parameter section in |buffer|.
    buffer.erase(parameter_section_size);
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(response_code_bytes.data(),
               response_code_bytes.size());
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(buffer.data(),
               buffer.size());
  std::string response_hash(32, 0);
  hash->Finish(string_as_array(&response_hash), response_hash.size());
  if (tag == TPM_ST_SESSIONS) {
    if (!authorization_delegate->CheckResponseAuthorization(
        response_hash,
        authorization_section_bytes)) {
      error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
      return;
    }
  }
  TPM2B_MAX_NV_BUFFER data;
  std::string data_bytes;
  rc = Parse_TPM2B_MAX_NV_BUFFER(
      &buffer,
      &data,
      &data_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (tag == TPM_ST_SESSIONS) {
    // Decrypt just the parameter data, not the size.
    std::string tmp = data_bytes.substr(2);
    if (!authorization_delegate->DecryptResponseParameter(&tmp)) {
      error_reporter.Run(TRUNKS_RC_ENCRYPTION_FAILED);
      return;
    }
    data_bytes.replace(2, std::string::npos, tmp);
    rc = Parse_TPM2B_MAX_NV_BUFFER(
        &data_bytes,
        &data,
        NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
  }
  callback.Run(response_code,
               data);
}

void Tpm::NV_Read(
      TPMI_RH_NV_AUTH auth_handle,
      const std::string& auth_handle_name,
      TPMI_RH_NV_INDEX nv_index,
      const std::string& nv_index_name,
      UINT16 size,
      UINT16 offset,
      AuthorizationDelegate* authorization_delegate,
      const NV_ReadResponse& callback) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(NV_ReadErrorCallback, callback);
  base::Callback<void(const std::string&)> parser =
      base::Bind(NV_ReadResponseParser,
                 callback,
                 authorization_delegate);
  TPMI_ST_COMMAND_TAG tag = TPM_ST_NO_SESSIONS;
  UINT32 command_size = 10;  // Header size.
  std::string handle_section_bytes;
  std::string parameter_section_bytes;
  TPM_CC command_code = TPM_CC_NV_Read;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string auth_handle_bytes;
  rc = Serialize_TPMI_RH_NV_AUTH(
      auth_handle,
      &auth_handle_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string nv_index_bytes;
  rc = Serialize_TPMI_RH_NV_INDEX(
      nv_index,
      &nv_index_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string size_bytes;
  rc = Serialize_UINT16(
      size,
      &size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string offset_bytes;
  rc = Serialize_UINT16(
      offset,
      &offset_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(auth_handle_name.data(),
               auth_handle_name.size());
  handle_section_bytes += auth_handle_bytes;
  command_size += auth_handle_bytes.size();
  hash->Update(nv_index_name.data(),
               nv_index_name.size());
  handle_section_bytes += nv_index_bytes;
  command_size += nv_index_bytes.size();
  hash->Update(size_bytes.data(),
               size_bytes.size());
  parameter_section_bytes += size_bytes;
  command_size += size_bytes.size();
  hash->Update(offset_bytes.data(),
               offset_bytes.size());
  parameter_section_bytes += offset_bytes;
  command_size += offset_bytes.size();
  std::string command_hash(32, 0);
  hash->Finish(string_as_array(&command_hash), command_hash.size());
  std::string authorization_section_bytes;
  if (!authorization_delegate->GetCommandAuthorization(
      command_hash,
      &authorization_section_bytes)) {
    error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
    return;
  }
  std::string authorization_size_bytes;
  if (!authorization_section_bytes.empty()) {
    tag = TPM_ST_SESSIONS;
    std::string tmp;
    rc = Serialize_UINT32(authorization_section_bytes.size(),
                          &authorization_size_bytes);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    command_size += authorization_size_bytes.size() +
                    authorization_section_bytes.size();
  }
  std::string tag_bytes;
  rc = Serialize_TPMI_ST_COMMAND_TAG(
      tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command_size_bytes;
  rc = Serialize_UINT32(
      command_size,
      &command_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command = tag_bytes +
                        command_size_bytes +
                        command_code_bytes +
                        handle_section_bytes +
                        authorization_size_bytes +
                        authorization_section_bytes +
                        parameter_section_bytes;
  CHECK(command.size() == command_size) << "Command size mismatch!";
  transceiver_->SendCommand(command, parser);
}

void NV_ReadLockErrorCallback(
    const Tpm::NV_ReadLockResponse& callback,
    TPM_RC response_code) {
  callback.Run(response_code);
}

void NV_ReadLockResponseParser(
    const Tpm::NV_ReadLockResponse& callback,
    AuthorizationDelegate* authorization_delegate,
    const std::string& response) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(NV_ReadLockErrorCallback, callback);
  std::string buffer(response);
  TPM_ST tag;
  std::string tag_bytes;
  rc = Parse_TPM_ST(
      &buffer,
      &tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  UINT32 response_size;
  std::string response_size_bytes;
  rc = Parse_UINT32(
      &buffer,
      &response_size,
      &response_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM_RC response_code;
  std::string response_code_bytes;
  rc = Parse_TPM_RC(
      &buffer,
      &response_code,
      &response_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (response_size != response.size()) {
    error_reporter.Run(TPM_RC_SIZE);
    return;
  }
  if (response_code != TPM_RC_SUCCESS) {
    error_reporter.Run(response_code);
    return;
  }
  TPM_CC command_code = TPM_CC_NV_ReadLock;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string authorization_section_bytes;
  if (tag == TPM_ST_SESSIONS) {
    UINT32 parameter_section_size = buffer.size();
    rc = Parse_UINT32(&buffer, &parameter_section_size, NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    if (parameter_section_size > buffer.size()) {
      error_reporter.Run(TPM_RC_INSUFFICIENT);
      return;
    }
    authorization_section_bytes = buffer.substr(parameter_section_size);
    // Keep the parameter section in |buffer|.
    buffer.erase(parameter_section_size);
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(response_code_bytes.data(),
               response_code_bytes.size());
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(buffer.data(),
               buffer.size());
  std::string response_hash(32, 0);
  hash->Finish(string_as_array(&response_hash), response_hash.size());
  if (tag == TPM_ST_SESSIONS) {
    if (!authorization_delegate->CheckResponseAuthorization(
        response_hash,
        authorization_section_bytes)) {
      error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
      return;
    }
  }
  callback.Run(response_code);
}

void Tpm::NV_ReadLock(
      TPMI_RH_NV_AUTH auth_handle,
      const std::string& auth_handle_name,
      TPMI_RH_NV_INDEX nv_index,
      const std::string& nv_index_name,
      AuthorizationDelegate* authorization_delegate,
      const NV_ReadLockResponse& callback) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(NV_ReadLockErrorCallback, callback);
  base::Callback<void(const std::string&)> parser =
      base::Bind(NV_ReadLockResponseParser,
                 callback,
                 authorization_delegate);
  TPMI_ST_COMMAND_TAG tag = TPM_ST_NO_SESSIONS;
  UINT32 command_size = 10;  // Header size.
  std::string handle_section_bytes;
  std::string parameter_section_bytes;
  TPM_CC command_code = TPM_CC_NV_ReadLock;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string auth_handle_bytes;
  rc = Serialize_TPMI_RH_NV_AUTH(
      auth_handle,
      &auth_handle_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string nv_index_bytes;
  rc = Serialize_TPMI_RH_NV_INDEX(
      nv_index,
      &nv_index_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(auth_handle_name.data(),
               auth_handle_name.size());
  handle_section_bytes += auth_handle_bytes;
  command_size += auth_handle_bytes.size();
  hash->Update(nv_index_name.data(),
               nv_index_name.size());
  handle_section_bytes += nv_index_bytes;
  command_size += nv_index_bytes.size();
  std::string command_hash(32, 0);
  hash->Finish(string_as_array(&command_hash), command_hash.size());
  std::string authorization_section_bytes;
  if (!authorization_delegate->GetCommandAuthorization(
      command_hash,
      &authorization_section_bytes)) {
    error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
    return;
  }
  std::string authorization_size_bytes;
  if (!authorization_section_bytes.empty()) {
    tag = TPM_ST_SESSIONS;
    std::string tmp;
    rc = Serialize_UINT32(authorization_section_bytes.size(),
                          &authorization_size_bytes);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    command_size += authorization_size_bytes.size() +
                    authorization_section_bytes.size();
  }
  std::string tag_bytes;
  rc = Serialize_TPMI_ST_COMMAND_TAG(
      tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command_size_bytes;
  rc = Serialize_UINT32(
      command_size,
      &command_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command = tag_bytes +
                        command_size_bytes +
                        command_code_bytes +
                        handle_section_bytes +
                        authorization_size_bytes +
                        authorization_section_bytes +
                        parameter_section_bytes;
  CHECK(command.size() == command_size) << "Command size mismatch!";
  transceiver_->SendCommand(command, parser);
}

void NV_ChangeAuthErrorCallback(
    const Tpm::NV_ChangeAuthResponse& callback,
    TPM_RC response_code) {
  callback.Run(response_code);
}

void NV_ChangeAuthResponseParser(
    const Tpm::NV_ChangeAuthResponse& callback,
    AuthorizationDelegate* authorization_delegate,
    const std::string& response) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(NV_ChangeAuthErrorCallback, callback);
  std::string buffer(response);
  TPM_ST tag;
  std::string tag_bytes;
  rc = Parse_TPM_ST(
      &buffer,
      &tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  UINT32 response_size;
  std::string response_size_bytes;
  rc = Parse_UINT32(
      &buffer,
      &response_size,
      &response_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM_RC response_code;
  std::string response_code_bytes;
  rc = Parse_TPM_RC(
      &buffer,
      &response_code,
      &response_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (response_size != response.size()) {
    error_reporter.Run(TPM_RC_SIZE);
    return;
  }
  if (response_code != TPM_RC_SUCCESS) {
    error_reporter.Run(response_code);
    return;
  }
  TPM_CC command_code = TPM_CC_NV_ChangeAuth;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string authorization_section_bytes;
  if (tag == TPM_ST_SESSIONS) {
    UINT32 parameter_section_size = buffer.size();
    rc = Parse_UINT32(&buffer, &parameter_section_size, NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    if (parameter_section_size > buffer.size()) {
      error_reporter.Run(TPM_RC_INSUFFICIENT);
      return;
    }
    authorization_section_bytes = buffer.substr(parameter_section_size);
    // Keep the parameter section in |buffer|.
    buffer.erase(parameter_section_size);
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(response_code_bytes.data(),
               response_code_bytes.size());
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(buffer.data(),
               buffer.size());
  std::string response_hash(32, 0);
  hash->Finish(string_as_array(&response_hash), response_hash.size());
  if (tag == TPM_ST_SESSIONS) {
    if (!authorization_delegate->CheckResponseAuthorization(
        response_hash,
        authorization_section_bytes)) {
      error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
      return;
    }
  }
  callback.Run(response_code);
}

void Tpm::NV_ChangeAuth(
      TPMI_RH_NV_INDEX nv_index,
      const std::string& nv_index_name,
      TPM2B_AUTH new_auth,
      AuthorizationDelegate* authorization_delegate,
      const NV_ChangeAuthResponse& callback) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(NV_ChangeAuthErrorCallback, callback);
  base::Callback<void(const std::string&)> parser =
      base::Bind(NV_ChangeAuthResponseParser,
                 callback,
                 authorization_delegate);
  TPMI_ST_COMMAND_TAG tag = TPM_ST_NO_SESSIONS;
  UINT32 command_size = 10;  // Header size.
  std::string handle_section_bytes;
  std::string parameter_section_bytes;
  TPM_CC command_code = TPM_CC_NV_ChangeAuth;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string nv_index_bytes;
  rc = Serialize_TPMI_RH_NV_INDEX(
      nv_index,
      &nv_index_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string new_auth_bytes;
  rc = Serialize_TPM2B_AUTH(
      new_auth,
      &new_auth_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  // Encrypt just the parameter data, not the size.
  std::string tmp = new_auth_bytes.substr(2);
  if (!authorization_delegate->EncryptCommandParameter(&tmp)) {
    error_reporter.Run(TRUNKS_RC_ENCRYPTION_FAILED);
    return;
  }
  new_auth_bytes.replace(2, std::string::npos, tmp);
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(nv_index_name.data(),
               nv_index_name.size());
  handle_section_bytes += nv_index_bytes;
  command_size += nv_index_bytes.size();
  hash->Update(new_auth_bytes.data(),
               new_auth_bytes.size());
  parameter_section_bytes += new_auth_bytes;
  command_size += new_auth_bytes.size();
  std::string command_hash(32, 0);
  hash->Finish(string_as_array(&command_hash), command_hash.size());
  std::string authorization_section_bytes;
  if (!authorization_delegate->GetCommandAuthorization(
      command_hash,
      &authorization_section_bytes)) {
    error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
    return;
  }
  std::string authorization_size_bytes;
  if (!authorization_section_bytes.empty()) {
    tag = TPM_ST_SESSIONS;
    std::string tmp;
    rc = Serialize_UINT32(authorization_section_bytes.size(),
                          &authorization_size_bytes);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    command_size += authorization_size_bytes.size() +
                    authorization_section_bytes.size();
  }
  std::string tag_bytes;
  rc = Serialize_TPMI_ST_COMMAND_TAG(
      tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command_size_bytes;
  rc = Serialize_UINT32(
      command_size,
      &command_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command = tag_bytes +
                        command_size_bytes +
                        command_code_bytes +
                        handle_section_bytes +
                        authorization_size_bytes +
                        authorization_section_bytes +
                        parameter_section_bytes;
  CHECK(command.size() == command_size) << "Command size mismatch!";
  transceiver_->SendCommand(command, parser);
}

void NV_CertifyErrorCallback(
    const Tpm::NV_CertifyResponse& callback,
    TPM_RC response_code) {
  callback.Run(response_code,
               TPM2B_ATTEST(),
               TPMT_SIGNATURE());
}

void NV_CertifyResponseParser(
    const Tpm::NV_CertifyResponse& callback,
    AuthorizationDelegate* authorization_delegate,
    const std::string& response) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(NV_CertifyErrorCallback, callback);
  std::string buffer(response);
  TPM_ST tag;
  std::string tag_bytes;
  rc = Parse_TPM_ST(
      &buffer,
      &tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  UINT32 response_size;
  std::string response_size_bytes;
  rc = Parse_UINT32(
      &buffer,
      &response_size,
      &response_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPM_RC response_code;
  std::string response_code_bytes;
  rc = Parse_TPM_RC(
      &buffer,
      &response_code,
      &response_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (response_size != response.size()) {
    error_reporter.Run(TPM_RC_SIZE);
    return;
  }
  if (response_code != TPM_RC_SUCCESS) {
    error_reporter.Run(response_code);
    return;
  }
  TPM_CC command_code = TPM_CC_NV_Certify;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string authorization_section_bytes;
  if (tag == TPM_ST_SESSIONS) {
    UINT32 parameter_section_size = buffer.size();
    rc = Parse_UINT32(&buffer, &parameter_section_size, NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    if (parameter_section_size > buffer.size()) {
      error_reporter.Run(TPM_RC_INSUFFICIENT);
      return;
    }
    authorization_section_bytes = buffer.substr(parameter_section_size);
    // Keep the parameter section in |buffer|.
    buffer.erase(parameter_section_size);
  }
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(response_code_bytes.data(),
               response_code_bytes.size());
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(buffer.data(),
               buffer.size());
  std::string response_hash(32, 0);
  hash->Finish(string_as_array(&response_hash), response_hash.size());
  if (tag == TPM_ST_SESSIONS) {
    if (!authorization_delegate->CheckResponseAuthorization(
        response_hash,
        authorization_section_bytes)) {
      error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
      return;
    }
  }
  TPM2B_ATTEST certify_info;
  std::string certify_info_bytes;
  rc = Parse_TPM2B_ATTEST(
      &buffer,
      &certify_info,
      &certify_info_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  TPMT_SIGNATURE signature;
  std::string signature_bytes;
  rc = Parse_TPMT_SIGNATURE(
      &buffer,
      &signature,
      &signature_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  if (tag == TPM_ST_SESSIONS) {
    // Decrypt just the parameter data, not the size.
    std::string tmp = certify_info_bytes.substr(2);
    if (!authorization_delegate->DecryptResponseParameter(&tmp)) {
      error_reporter.Run(TRUNKS_RC_ENCRYPTION_FAILED);
      return;
    }
    certify_info_bytes.replace(2, std::string::npos, tmp);
    rc = Parse_TPM2B_ATTEST(
        &certify_info_bytes,
        &certify_info,
        NULL);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
  }
  callback.Run(response_code,
               certify_info,
               signature);
}

void Tpm::NV_Certify(
      TPMI_DH_OBJECT sign_handle,
      const std::string& sign_handle_name,
      TPMI_RH_NV_AUTH auth_handle,
      const std::string& auth_handle_name,
      TPMI_RH_NV_INDEX nv_index,
      const std::string& nv_index_name,
      TPM2B_DATA qualifying_data,
      TPMT_SIG_SCHEME in_scheme,
      UINT16 size,
      UINT16 offset,
      AuthorizationDelegate* authorization_delegate,
      const NV_CertifyResponse& callback) {
  TPM_RC rc = TPM_RC_SUCCESS;
  base::Callback<void(TPM_RC)> error_reporter =
      base::Bind(NV_CertifyErrorCallback, callback);
  base::Callback<void(const std::string&)> parser =
      base::Bind(NV_CertifyResponseParser,
                 callback,
                 authorization_delegate);
  TPMI_ST_COMMAND_TAG tag = TPM_ST_NO_SESSIONS;
  UINT32 command_size = 10;  // Header size.
  std::string handle_section_bytes;
  std::string parameter_section_bytes;
  TPM_CC command_code = TPM_CC_NV_Certify;
  std::string command_code_bytes;
  rc = Serialize_TPM_CC(
      command_code,
      &command_code_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string sign_handle_bytes;
  rc = Serialize_TPMI_DH_OBJECT(
      sign_handle,
      &sign_handle_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string auth_handle_bytes;
  rc = Serialize_TPMI_RH_NV_AUTH(
      auth_handle,
      &auth_handle_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string nv_index_bytes;
  rc = Serialize_TPMI_RH_NV_INDEX(
      nv_index,
      &nv_index_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string qualifying_data_bytes;
  rc = Serialize_TPM2B_DATA(
      qualifying_data,
      &qualifying_data_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string in_scheme_bytes;
  rc = Serialize_TPMT_SIG_SCHEME(
      in_scheme,
      &in_scheme_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string size_bytes;
  rc = Serialize_UINT16(
      size,
      &size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string offset_bytes;
  rc = Serialize_UINT16(
      offset,
      &offset_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  // Encrypt just the parameter data, not the size.
  std::string tmp = qualifying_data_bytes.substr(2);
  if (!authorization_delegate->EncryptCommandParameter(&tmp)) {
    error_reporter.Run(TRUNKS_RC_ENCRYPTION_FAILED);
    return;
  }
  qualifying_data_bytes.replace(2, std::string::npos, tmp);
  scoped_ptr<crypto::SecureHash> hash(crypto::SecureHash::Create(
      crypto::SecureHash::SHA256));
  hash->Update(command_code_bytes.data(),
               command_code_bytes.size());
  hash->Update(sign_handle_name.data(),
               sign_handle_name.size());
  handle_section_bytes += sign_handle_bytes;
  command_size += sign_handle_bytes.size();
  hash->Update(auth_handle_name.data(),
               auth_handle_name.size());
  handle_section_bytes += auth_handle_bytes;
  command_size += auth_handle_bytes.size();
  hash->Update(nv_index_name.data(),
               nv_index_name.size());
  handle_section_bytes += nv_index_bytes;
  command_size += nv_index_bytes.size();
  hash->Update(qualifying_data_bytes.data(),
               qualifying_data_bytes.size());
  parameter_section_bytes += qualifying_data_bytes;
  command_size += qualifying_data_bytes.size();
  hash->Update(in_scheme_bytes.data(),
               in_scheme_bytes.size());
  parameter_section_bytes += in_scheme_bytes;
  command_size += in_scheme_bytes.size();
  hash->Update(size_bytes.data(),
               size_bytes.size());
  parameter_section_bytes += size_bytes;
  command_size += size_bytes.size();
  hash->Update(offset_bytes.data(),
               offset_bytes.size());
  parameter_section_bytes += offset_bytes;
  command_size += offset_bytes.size();
  std::string command_hash(32, 0);
  hash->Finish(string_as_array(&command_hash), command_hash.size());
  std::string authorization_section_bytes;
  if (!authorization_delegate->GetCommandAuthorization(
      command_hash,
      &authorization_section_bytes)) {
    error_reporter.Run(TRUNKS_RC_AUTHORIZATION_FAILED);
    return;
  }
  std::string authorization_size_bytes;
  if (!authorization_section_bytes.empty()) {
    tag = TPM_ST_SESSIONS;
    std::string tmp;
    rc = Serialize_UINT32(authorization_section_bytes.size(),
                          &authorization_size_bytes);
    if (rc != TPM_RC_SUCCESS) {
      error_reporter.Run(rc);
      return;
    }
    command_size += authorization_size_bytes.size() +
                    authorization_section_bytes.size();
  }
  std::string tag_bytes;
  rc = Serialize_TPMI_ST_COMMAND_TAG(
      tag,
      &tag_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command_size_bytes;
  rc = Serialize_UINT32(
      command_size,
      &command_size_bytes);
  if (rc != TPM_RC_SUCCESS) {
    error_reporter.Run(rc);
    return;
  }
  std::string command = tag_bytes +
                        command_size_bytes +
                        command_code_bytes +
                        handle_section_bytes +
                        authorization_size_bytes +
                        authorization_section_bytes +
                        parameter_section_bytes;
  CHECK(command.size() == command_size) << "Command size mismatch!";
  transceiver_->SendCommand(command, parser);
}

}  // namespace trunks
