// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <chromeos/data_encoding.h>

#include <memory>

#include <base/logging.h>
#include <base/strings/stringprintf.h>
#include <chromeos/strings/string_utils.h>
#include <openssl/bio.h>
#include <openssl/evp.h>

namespace {

using BIO_ptr_t = std::unique_ptr<BIO, void(*)(BIO*)>;

inline int HexToDec(int hex) {
  int dec = -1;
  if (hex >= '0' && hex <= '9') {
    dec = hex - '0';
  } else if (hex >= 'A' && hex <= 'F') {
    dec = hex - 'A' + 10;
  } else if (hex >= 'a' && hex <= 'f') {
    dec = hex - 'a' + 10;
  }
  return dec;
}

// Helper for Base64Encode() and Base64EncodeWrapLines().
std::string Base64EncodeHelper(const void* data, size_t size, bool wrap_lines) {
  BIO_ptr_t base64{BIO_new(BIO_f_base64()), BIO_free_all};
  if (!wrap_lines)
    BIO_set_flags(base64.get(), BIO_FLAGS_BASE64_NO_NL);
  base64.reset(BIO_push(base64.release(), BIO_new(BIO_s_mem())));
  CHECK_EQ(static_cast<int>(size), BIO_write(base64.get(), data, size));
  CHECK_EQ(1, BIO_flush(base64.get()));
  char* encoded_data = nullptr;
  size_t encoded_data_size = BIO_get_mem_data(base64.get(), &encoded_data);
  return std::string{encoded_data, encoded_data_size};
}

}  // namespace

/////////////////////////////////////////////////////////////////////////
namespace chromeos {
namespace data_encoding {

std::string UrlEncode(const char* data, bool encodeSpaceAsPlus) {
  std::string result;

  while (*data) {
    char c = *data++;
    // According to RFC3986 (http://www.faqs.org/rfcs/rfc3986.html),
    // section 2.3. - Unreserved Characters
    if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') ||
        (c >= 'a' && c <= 'z') || c == '-' || c == '.' || c == '_' ||
        c == '~') {
      result += c;
    } else if (c == ' ' && encodeSpaceAsPlus) {
      // For historical reasons, some URLs have spaces encoded as '+',
      // this also applies to form data encoded as
      // 'application/x-www-form-urlencoded'
      result += '+';
    } else {
      base::StringAppendF(&result,
                          "%%%02X",
                          static_cast<unsigned char>(c));  // Encode as %NN
    }
  }
  return result;
}

std::string UrlDecode(const char* data) {
  std::string result;
  while (*data) {
    char c = *data++;
    int part1 = 0, part2 = 0;
    // HexToDec would return -1 even for character 0 (end of string),
    // so it is safe to access data[0] and data[1] without overrunning the buf.
    if (c == '%' && (part1 = HexToDec(data[0])) >= 0 &&
        (part2 = HexToDec(data[1])) >= 0) {
      c = static_cast<char>((part1 << 4) | part2);
      data += 2;
    } else if (c == '+') {
      c = ' ';
    }
    result += c;
  }
  return result;
}

std::string WebParamsEncode(const WebParamList& params,
                            bool encodeSpaceAsPlus) {
  std::vector<std::string> pairs;
  pairs.reserve(params.size());
  for (const auto& p : params) {
    std::string key = UrlEncode(p.first.c_str(), encodeSpaceAsPlus);
    std::string value = UrlEncode(p.second.c_str(), encodeSpaceAsPlus);
    pairs.push_back(chromeos::string_utils::Join('=', key, value));
  }

  return chromeos::string_utils::Join('&', pairs);
}

WebParamList WebParamsDecode(const std::string& data) {
  WebParamList result;
  std::vector<std::string> params = chromeos::string_utils::Split(data, '&');
  for (const auto& p : params) {
    auto pair = chromeos::string_utils::SplitAtFirst(p, '=');
    result.emplace_back(UrlDecode(pair.first.c_str()),
                        UrlDecode(pair.second.c_str()));
  }
  return result;
}

std::string Base64Encode(const void* data, size_t size) {
  return Base64EncodeHelper(data, size, false);
}

std::string Base64EncodeWrapLines(const void* data, size_t size) {
  return Base64EncodeHelper(data, size, true);
}

bool Base64Decode(const std::string& input, chromeos::Blob* output) {
  BIO_ptr_t base64{BIO_new(BIO_f_base64()), BIO_free_all};
  std::string temp_buffer;
  const std::string* data = &input;
  if (input.find_first_of("\r\n") != std::string::npos) {
    // If we have line breaks in the string, make sure we have the ending one.
    if (input.back() != '\n') {
      temp_buffer = input;
      temp_buffer.push_back('\n');
      data = &temp_buffer;
    }
  } else {
    // We have no line breaks. Tell BIO that...
    BIO_set_flags(base64.get(), BIO_FLAGS_BASE64_NO_NL);
  }
  BIO* bmem = BIO_new_mem_buf(const_cast<char*>(data->data()), data->size());
  if (!bmem) {
    LOG(ERROR) << "Unable to get BIO buffer to decode base64 string";
    return false;
  }

  base64.reset(BIO_push(base64.release(), bmem));
  // base64 decoded data has 25% fewer bytes than the original (since every
  // 3 source octets are encoded as 4 characters in base64).
  // The upper estimate of the output data buffer is (encoded_size * 0.75),
  // while the actual memory requirements could be a bit lower if the encoded
  // data has any padding characters and/or line breaks.
  output->resize(input.size() * 3 / 4);

  int size_read = BIO_read(base64.get(), output->data(), output->size());
  if (size_read < 0)
    return false;
  output->resize(size_read);

  // If the source data had any invalid characters, BIO_read will return 0,
  // so let's use this as an indication of error. Here we make an assumption
  // that trying to decode an empty string is an error in itself.
  return (size_read > 0);
}

}  // namespace data_encoding
}  // namespace chromeos
