// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "buffet/commands/unittest_utils.h"

#include <base/json/json_reader.h>
#include <base/json/json_writer.h>

namespace buffet {
namespace unittests {

std::unique_ptr<base::Value> CreateValue(const char* json) {
  std::string json2(json);
  // Convert apostrophes to double-quotes so JSONReader can parse the string.
  std::replace(json2.begin(), json2.end(), '\'', '"');
  return std::unique_ptr<base::Value>(base::JSONReader::Read(json2));
}

std::unique_ptr<base::DictionaryValue> CreateDictionaryValue(const char* json) {
  std::string json2(json);
  std::replace(json2.begin(), json2.end(), '\'', '"');
  base::Value* value = base::JSONReader::Read(json2);
  base::DictionaryValue* dict;
  value->GetAsDictionary(&dict);
  return std::unique_ptr<base::DictionaryValue>(dict);
}

std::string ValueToString(const base::Value* value) {
  std::string json;
  base::JSONWriter::Write(value, &json);
  std::replace(json.begin(), json.end(), '"', '\'');
  return json;
}

}  // namespace unittests
}  // namespace buffet
