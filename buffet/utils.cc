// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "buffet/utils.h"

#include <map>
#include <string>

#include <base/files/file_util.h>
#include <base/json/json_reader.h>
#include <chromeos/errors/error_codes.h>

namespace buffet {

namespace {

// Truncates a string if it is too long. Used for error reporting with really
// long JSON strings.
std::string LimitString(const std::string& text, size_t max_len) {
  if (text.size() <= max_len)
    return text;
  return text.substr(0, max_len - 3) + "...";
}

const size_t kMaxStrLen = 1700;  // Log messages are limited to 2000 chars.

}  // anonymous namespace

const char kErrorDomainBuffet[] = "buffet";
const char kFileReadError[] = "file_read_error";
const char kInvalidCategoryError[] = "invalid_category";
const char kInvalidPackageError[] = "invalid_package";

std::unique_ptr<const base::DictionaryValue> LoadJsonDict(
    const base::FilePath& json_file_path, chromeos::ErrorPtr* error) {
  std::string json_string;
  if (!base::ReadFileToString(json_file_path, &json_string)) {
    chromeos::errors::system::AddSystemError(error, FROM_HERE, errno);
    chromeos::Error::AddToPrintf(error, FROM_HERE, kErrorDomainBuffet,
                                 kFileReadError,
                                 "Failed to read file '%s'",
                                 json_file_path.value().c_str());
    return {};
  }
  return LoadJsonDict(json_string, error);
}

std::unique_ptr<const base::DictionaryValue> LoadJsonDict(
    const std::string& json_string, chromeos::ErrorPtr* error) {
  std::unique_ptr<const base::DictionaryValue> result;
  std::string error_message;
  base::Value* value = base::JSONReader::ReadAndReturnError(
      json_string, base::JSON_PARSE_RFC, nullptr, &error_message);
  if (!value) {
    chromeos::Error::AddToPrintf(error, FROM_HERE,
                                 chromeos::errors::json::kDomain,
                                 chromeos::errors::json::kParseError,
                                 "Error parsing JSON string '%s': %s",
                                 LimitString(json_string, kMaxStrLen).c_str(),
                                 error_message.c_str());
    return result;
  }
  const base::DictionaryValue* dict_value = nullptr;
  if (!value->GetAsDictionary(&dict_value)) {
    delete value;
    chromeos::Error::AddToPrintf(error, FROM_HERE,
                                 chromeos::errors::json::kDomain,
                                 chromeos::errors::json::kObjectExpected,
                                 "JSON string '%s' is not a JSON object",
                                 LimitString(json_string, kMaxStrLen).c_str());
    return result;
  }
  result.reset(dict_value);
  return result;
}

}  // namespace buffet
