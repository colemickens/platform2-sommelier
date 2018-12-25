// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <memory>
#include <string>

#include <base/strings/stringprintf.h>
#include <base/strings/string_number_conversions.h>

#include "runtime_probe/probe_result_checker.h"
#include "runtime_probe/utils/type_utils.h"

namespace {
typedef runtime_probe::ValidatorOperator ValidatorOperator;

constexpr const char* GetPrefix(ValidatorOperator op) {
  switch (op) {
    case ValidatorOperator::NOP:
      return "!nop ";
    case ValidatorOperator::RE:
      return "!re ";
    case ValidatorOperator::EQ:
      return "!eq ";
    case ValidatorOperator::NE:
      return "!ne ";
    case ValidatorOperator::GT:
      return "!gt ";
    case ValidatorOperator::GE:
      return "!ge ";
    case ValidatorOperator::LT:
      return "!lt ";
    case ValidatorOperator::LE:
      return "!le ";
    default:
      DCHECK(false) << "should never reach here";
  }
  return nullptr;
}

constexpr const char* ToString(ValidatorOperator op) {
  switch (op) {
    case ValidatorOperator::NOP:
      return "NOP";
    case ValidatorOperator::RE:
      return "RE";
    case ValidatorOperator::EQ:
      return "EQ";
    case ValidatorOperator::NE:
      return "NE";
    case ValidatorOperator::GT:
      return "GT";
    case ValidatorOperator::GE:
      return "GE";
    case ValidatorOperator::LT:
      return "LT";
    case ValidatorOperator::LE:
      return "LE";
    default:
      DCHECK(false) << "should never reach here";
  }
  return nullptr;
}

bool SplitValidateRuleString(const base::StringPiece& validate_rule,
                             ValidatorOperator* operator_,
                             base::StringPiece* operand) {
  if (validate_rule.empty()) {
    *operator_ = ValidatorOperator::NOP;
    *operand = "";
    return true;
  }

  auto first_space_idx = validate_rule.find_first_of(' ');
  auto prefix = validate_rule.substr(0, first_space_idx + 1);
  auto rest = validate_rule.substr(first_space_idx + 1);

  for (int i = 0; i < static_cast<int>(ValidatorOperator::NUM_OP); i++) {
    auto op = static_cast<ValidatorOperator>(i);
    if (prefix == GetPrefix(op)) {
      *operator_ = op;
      if (op != ValidatorOperator::NOP)  // NOP shouldn't have operand.
        *operand = rest;
      return true;
    }
  }
  return false;
}

template <typename ConverterType>
std::unique_ptr<ConverterType> BuildNumericConverter(
    const base::StringPiece& validate_rule) {
  ValidatorOperator op;
  base::StringPiece rest;

  if (SplitValidateRuleString(validate_rule, &op, &rest)) {
    if (op == ValidatorOperator::NOP)
      return std::make_unique<ConverterType>(op, 0);

    if (op == ValidatorOperator::EQ || op == ValidatorOperator::NE ||
        op == ValidatorOperator::GT || op == ValidatorOperator::GE ||
        op == ValidatorOperator::LT || op == ValidatorOperator::LE) {
      typename ConverterType::OperandType operand;
      if (ConverterType::StringToOperand(rest.as_string(), &operand)) {
        return std::make_unique<ConverterType>(op, operand);
      } else {
        LOG(ERROR) << "Can't convert to operand: " << rest.as_string();
      }
    }
  }
  LOG(ERROR) << "Invalid validate rule: " << validate_rule;
  return nullptr;
}

}  // namespace

namespace runtime_probe {

typedef FieldConverter::ReturnCode ReturnCode;

std::string StringFieldConverter::ToString() const {
  return base::StringPrintf("StringFieldConverter(%s, %s)",
                            ::ToString(operator_), operand_.c_str());
}

std::string IntegerFieldConverter::ToString() const {
  return base::StringPrintf("IntegerFieldConverter(%s, %d)",
                            ::ToString(operator_), operand_);
}

std::string HexFieldConverter::ToString() const {
  return base::StringPrintf("IntegerFieldConverter(%s, 0x%x)",
                            ::ToString(operator_), operand_);
}

std::string DoubleFieldConverter::ToString() const {
  return base::StringPrintf("IntegerFieldConverter(%s, %f)",
                            ::ToString(operator_), operand_);
}

std::unique_ptr<StringFieldConverter> StringFieldConverter::Build(
    const base::StringPiece& validate_rule) {
  ValidatorOperator op;
  base::StringPiece pattern;

  if (SplitValidateRuleString(validate_rule, &op, &pattern)) {
    if (op == ValidatorOperator::NOP)
      return std::make_unique<StringFieldConverter>(op, "");

    if (op == ValidatorOperator::EQ || op == ValidatorOperator::NE) {
      return std::make_unique<StringFieldConverter>(op, pattern);
    }

    if (op == ValidatorOperator::RE) {
      auto instance = std::make_unique<StringFieldConverter>(op, pattern);
      if (instance->regex_->error().empty()) {
        // No error, the pattern is valid.
        return instance;
      }
      // Error string is set to non-empty if there are errors.
      LOG(ERROR) << "Invalid pattern: " << pattern;
      LOG(ERROR) << instance->regex_->error();
    }
  }

  LOG(ERROR) << "Invalid validate rule: " << validate_rule;
  return nullptr;
}

std::unique_ptr<IntegerFieldConverter> IntegerFieldConverter::Build(
    const base::StringPiece& validate_rule) {
  return BuildNumericConverter<IntegerFieldConverter>(validate_rule);
}

std::unique_ptr<HexFieldConverter> HexFieldConverter::Build(
    const base::StringPiece& validate_rule) {
  return BuildNumericConverter<HexFieldConverter>(validate_rule);
}

std::unique_ptr<DoubleFieldConverter> DoubleFieldConverter::Build(
    const base::StringPiece& validate_rule) {
  return BuildNumericConverter<DoubleFieldConverter>(validate_rule);
}

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
      if (StringToInt(string_value, &int_value)) {
        dict_value->SetInteger(field_name, int_value);
        return ReturnCode::OK;
      } else {
        LOG(ERROR) << "Failed to convert '" << string_value << "' to integer.";
        return ReturnCode::INCOMPATIBLE_VALUE;
      }
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
      if (HexStringToInt(string_value, &int_value)) {
        dict_value->SetInteger(field_name, int_value);
        return ReturnCode::OK;
      } else {
        LOG(ERROR) << "Failed to convert '" << string_value << "' to integer.";
        return ReturnCode::INCOMPATIBLE_VALUE;
      }
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
      double double_value;
      if (StringToDouble(string_value, &double_value)) {
        dict_value->SetDouble(field_name, double_value);
        return ReturnCode::OK;
      } else {
        LOG(ERROR) << "Failed to convert '" << string_value << "' to double.";
        return ReturnCode::INCOMPATIBLE_VALUE;
      }
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

    std::string validate_rule;
    if (list_value->GetSize() == 3 && !list_value->GetString(2, &validate_rule))
      return print_error_and_return();

    if (expect_type == "str") {
      (*target)[key] = StringFieldConverter::Build(validate_rule);
    } else if (expect_type == "int") {
      (*target)[key] = IntegerFieldConverter::Build(validate_rule);
    } else if (expect_type == "double") {
      (*target)[key] = DoubleFieldConverter::Build(validate_rule);
    } else if (expect_type == "hex") {
      (*target)[key] = HexFieldConverter::Build(validate_rule);
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

  // Try to convert and validate each required fields.
  // Any failures will cause the final result be |false|.
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

  // |ProbeStatement| will remove this element from final results, there is no
  // need to continue.
  if (!success) {
    VLOG(3) << "probe_result = " << *probe_result;
    return false;
  }

  // Try to convert and validate each optional fields.
  // For failures, just remove them from probe_result and continue.
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
