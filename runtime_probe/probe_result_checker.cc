/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <map>
#include <memory>
#include <string>

#include <base/strings/string_number_conversions.h>

#include "runtime_probe/probe_result_checker.h"

namespace runtime_probe {

typedef FieldConverter::ReturnCode ReturnCode;

ReturnCode StringFieldConverter::Convert(
    const std::string& field_name,
    base::DictionaryValue* const dict_value) const {
  const base::Value* value;

  CHECK(dict_value);

  if (!dict_value->Get(field_name, &value))
    return ReturnCode::FIELD_NOT_FOUND;

  switch (value->type()) {
    case base::Value::Type::DOUBLE:
      dict_value->SetString(field_name, std::to_string(value->GetDouble()));
      return ReturnCode::OK;
    case base::Value::Type::INTEGER:
      dict_value->SetString(field_name, std::to_string(value->GetInt()));
      return ReturnCode::OK;
    case base::Value::Type::NONE:
      dict_value->SetString(field_name, "null");
      return ReturnCode::OK;
    case base::Value::Type::STRING:
      return ReturnCode::OK;
    default:
      return ReturnCode::INCOMPATIBLE_VALUE;
  }
}

ReturnCode IntegerFieldConverter::Convert(
    const std::string& field_name,
    base::DictionaryValue* const dict_value) const {
  const base::Value* value;

  CHECK(dict_value);

  if (!dict_value->Get(field_name, &value))
    return ReturnCode::FIELD_NOT_FOUND;

  switch (value->type()) {
    case base::Value::Type::DOUBLE:
      dict_value->SetInteger(field_name, value->GetDouble());
      return ReturnCode::OK;
    case base::Value::Type::INTEGER:
      return ReturnCode::OK;
    case base::Value::Type::STRING: {
      std::string string_value = value->GetString();
      int int_value;
      /* base::StringToInt() only returns true on "perfect" conversion,
       *
       * That is, if the function returns false, it doesn't mean the conversion
       * failed, but might due to leading / trailing whitespace or other
       * characters.
       *
       * For now, let's print a warning message and proceed.
       */
      if (!base::StringToInt(string_value, &int_value)) {
        if (int_value == 0) {
          LOG(WARNING) << "'" << string_value << "' is converted to "
                       << int_value << ", which might not be expected.";
        }
      }

      dict_value->SetInteger(field_name, int_value);
      return ReturnCode::OK;
    }
    default:
      return ReturnCode::INCOMPATIBLE_VALUE;
  }
}

ReturnCode HexFieldConverter::Convert(
    const std::string& field_name,
    base::DictionaryValue* const dict_value) const {
  const base::Value* value;

  CHECK(dict_value);

  if (!dict_value->Get(field_name, &value))
    return ReturnCode::FIELD_NOT_FOUND;

  switch (value->type()) {
    case base::Value::Type::DOUBLE:
      dict_value->SetInteger(field_name, value->GetDouble());
      return ReturnCode::OK;
    case base::Value::Type::INTEGER:
      return ReturnCode::OK;
    case base::Value::Type::STRING: {
      std::string string_value = value->GetString();
      int int_value;
      /* base::HexStringToInt() only returns true on "perfect" conversion,
       *
       * That is, if the function returns false, it doesn't mean the conversion
       * failed, but might due to leading / trailing whitespace or other
       * characters.
       *
       * For now, let's print a warning message and proceed.
       */
      if (!base::HexStringToInt(string_value, &int_value)) {
        if (int_value == 0) {
          LOG(WARNING) << "'" << string_value << "' is converted to "
                       << int_value << ", which might not be expected.";
        }
      }

      dict_value->SetInteger(field_name, int_value);
      return ReturnCode::OK;
    }
    default:
      return ReturnCode::INCOMPATIBLE_VALUE;
  }
}

ReturnCode DoubleFieldConverter::Convert(
    const std::string& field_name,
    base::DictionaryValue* const dict_value) const {
  const base::Value* value;

  CHECK(dict_value);

  if (!dict_value->Get(field_name, &value))
    return ReturnCode::FIELD_NOT_FOUND;

  switch (value->type()) {
    case base::Value::Type::DOUBLE:
      return ReturnCode::OK;
    case base::Value::Type::INTEGER:
      dict_value->SetDouble(field_name, value->GetInt());
      return ReturnCode::OK;
    case base::Value::Type::STRING: {
      std::string string_value = value->GetString();
      dict_value->SetDouble(field_name, std::stod(string_value));
      return ReturnCode::OK;
    }
    default:
      return ReturnCode::INCOMPATIBLE_VALUE;
  }
}

std::unique_ptr<ProbeResultChecker> ProbeResultChecker::FromDictionaryValue(
    const base::DictionaryValue& dict_value) {
  std::unique_ptr<ProbeResultChecker> instance{new ProbeResultChecker};

  for (base::DictionaryValue::Iterator it{dict_value}; !it.IsAtEnd();
       it.Advance()) {
    const auto& key = it.key();
    auto print_error_and_return = [&it]() {
      LOG(ERROR) << "'expect' attribute should be a DictionaryValue whose "
                 << "values are [<required:bool>, <expected_type:string>, "
                 << "<optional_validate_rule:string>], got: " << it.value();
      return nullptr;
    };

    const base::ListValue* list_value;

    if (!it.value().GetAsList(&list_value) || list_value->GetSize() < 2 ||
        list_value->GetSize() > 3) {
      return print_error_and_return();
    }

    bool required;
    if (!list_value->GetBoolean(0, &required))
      return print_error_and_return();
    auto* target =
        required ? &instance->required_fields_ : &instance->optional_fields_;

    std::string expect_type;
    if (!list_value->GetString(1, &expect_type))
      return print_error_and_return();

    // TODO(b/121354690): handle validate rule
    if (expect_type == "str") {
      (*target)[key] = std::make_unique<StringFieldConverter>();
    } else if (expect_type == "int") {
      (*target)[key] = std::make_unique<IntegerFieldConverter>();
    } else if (expect_type == "double") {
      (*target)[key] = std::make_unique<DoubleFieldConverter>();
    } else if (expect_type == "hex") {
      (*target)[key] = std::make_unique<HexFieldConverter>();
    } else {
      LOG(ERROR) << "Unknown 'expect_type': " << expect_type;
      return nullptr;
    }
  }

  return instance;
}

bool ProbeResultChecker::Apply(base::DictionaryValue* probe_result) const {
  bool success = true;

  CHECK(probe_result != nullptr);

  /* Try to convert and valid each required fields.
   * Any failures will cause the final result be |false|.
   */
  for (const auto& it : required_fields_) {
    if (!probe_result->HasKey(it.first)) {
      LOG(ERROR) << "Missing key: " << it.first;
      success = false;
      break;
    }

    auto return_code = it.second->Convert(it.first, probe_result);
    if (return_code != ReturnCode::OK) {
      const base::Value* value;

      probe_result->Get(it.first, &value);
      LOG(ERROR) << "Failed to apply " << it.second->ToString() << " on "
                 << value << "(ReturnCode = " << static_cast<int>(return_code)
                 << ")";

      success = false;
      break;
    }
  }

  /* |ProbeStatement| will remove this element from final results, there is no
   * need to continue.
   */
  if (!success) {
    VLOG(1) << "probe_result = " << *probe_result;
    return false;
  }

  /* Try to convert and valid each optional fields.
   * For failures, just remove them from probe_result and continue.
   */
  for (const auto& it : optional_fields_) {
    if (!probe_result->HasKey(it.first))
      continue;

    auto return_code = it.second->Convert(it.first, probe_result);
    if (return_code != ReturnCode::OK) {
      VLOG(1) << "Optional field '" << it.first << "' has unexpected value, "
              << "remove it from probe result.";
      probe_result->Remove(it.first, nullptr);
    }
  }

  return true;
}

}  // namespace runtime_probe
