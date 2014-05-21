// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "buffet/commands/prop_types.h"

#include <algorithm>
#include <limits>
#include <set>

#include <base/json/json_writer.h>
#include <base/logging.h>
#include <base/values.h>

#include "buffet/commands/prop_values.h"
#include "buffet/commands/object_schema.h"
#include "buffet/commands/schema_constants.h"
#include "buffet/string_utils.h"

namespace buffet {

// PropType -------------------------------------------------------------------
PropType::PropType() {
}

PropType::~PropType() {
}

std::string PropType::GetTypeAsString() const {
  return GetTypeStringFromType(GetType());
}

bool PropType::HasOverriddenAttributes() const {
  for (const auto& pair : constraints_) {
    if (pair.second->HasOverriddenAttributes())
      return true;
  }
  return false;
}

std::unique_ptr<base::Value> PropType::ToJson(bool full_schema,
                                               ErrorPtr* error) const {
  if (!full_schema && !HasOverriddenAttributes()) {
    if (based_on_schema_)
      return std::unique_ptr<base::Value>(new base::DictionaryValue);
    return TypedValueToJson(GetTypeAsString());
  }

  std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue);
  if (full_schema) {
    // If we are asked for full_schema definition, then we need to output every
    // parameter property, including the "type", and any constraints.
    // So, we write the "type" only if asked for full schema.
    // Otherwise we will be able to infer the parameter type based on
    // the constraints and their types.
    // That is, the following JSONs could possibly define a parameter:
    // {'type':'integer'} -> explicit "integer" with no constraints
    // {'minimum':10} -> no type specified, but since we have "minimum"
    //                   and 10 is integer, than this is an integer
    //                   parameter with min constraint.
    // {'enum':[1,2,3]} -> integer with OneOf constraint.
    // And so is this: [1,2,3] -> an array of ints assume it's an "int" enum.
    dict->SetString(commands::attributes::kType, GetTypeAsString());
  }

  if (!full_schema && constraints_.size() == 1) {
    // If we are not asked for full schema, and we have only one constraint
    // which is OneOf, we short-circuit the whole thing and return just
    // the array [1,2,3] instead of an object with "enum" property like:
    // {'enum':[1,2,3]}
    auto p = constraints_.find(ConstraintType::OneOf);
    if (p != constraints_.end()) {
      return p->second->ToJson(error);
    }
  }

  for (const auto& pair : constraints_) {
    if (!pair.second->AddToJsonDict(dict.get(), !full_schema, error))
      return std::unique_ptr<base::Value>();
  }
  return std::unique_ptr<base::Value>(dict.release());
}

bool PropType::FromJson(const base::DictionaryValue* value,
                        const PropType* schema, ErrorPtr* error) {
  if (schema && schema->GetType() != GetType()) {
    Error::AddToPrintf(error, commands::errors::kDomain,
                       commands::errors::kPropTypeChanged,
                       "Redefining a command of type %s as %s",
                       schema->GetTypeAsString().c_str(),
                       GetTypeAsString().c_str());
    return false;
  }
  based_on_schema_ = (schema != nullptr);
  constraints_.clear();
  if (schema) {
    for (const auto& pair : schema->GetConstraints()) {
      std::shared_ptr<Constraint> inherited(pair.second->CloneAsInherited());
      constraints_.insert(std::make_pair(pair.first, inherited));
    }
  }
  return true;
}

void PropType::AddConstraint(std::shared_ptr<Constraint> constraint) {
  constraints_[constraint->GetType()] = constraint;
}

void PropType::RemoveConstraint(ConstraintType constraint_type) {
  constraints_.erase(constraint_type);
}

const Constraint* PropType::GetConstraint(
    ConstraintType constraint_type) const {
  auto p = constraints_.find(constraint_type);
  return p != constraints_.end() ? p->second.get() : nullptr;
}

Constraint* PropType::GetConstraint(ConstraintType constraint_type) {
  auto p = constraints_.find(constraint_type);
  return p != constraints_.end() ? p->second.get() : nullptr;
}

bool PropType::ValidateConstraints(const Any& value, ErrorPtr* error) const {
  for (const auto& pair : constraints_) {
    if (!pair.second->Validate(value, error))
      return false;
  }
  return true;
}

const PropType::TypeMap& PropType::GetTypeMap() {
  static TypeMap map = {
    {ValueType::Int,         "integer"},
    {ValueType::Double,      "number"},
    {ValueType::String,      "string"},
    {ValueType::Boolean,     "boolean"},
  };
  return map;
}

std::string PropType::GetTypeStringFromType(ValueType type) {
  for (const auto& pair : GetTypeMap()) {
    if (pair.first == type)
      return pair.second;
  }
  LOG(FATAL) << "Type map is missing a type";
  return std::string();
}

bool PropType::GetTypeFromTypeString(const std::string& name, ValueType* type) {
  for (const auto& pair : GetTypeMap()) {
    if (pair.second == name) {
      *type = pair.first;
      return true;
    }
  }
  return false;
}

std::unique_ptr<PropType> PropType::Create(ValueType type) {
  PropType* prop = nullptr;
  switch (type) {
  case buffet::ValueType::Int:
    prop = new IntPropType;
    break;
  case buffet::ValueType::Double:
    prop = new DoublePropType;
    break;
  case buffet::ValueType::String:
    prop = new StringPropType;
    break;
  case buffet::ValueType::Boolean:
    prop = new BooleanPropType;
    break;
  }
  return std::unique_ptr<PropType>(prop);
}

template<typename T>
bool TypedValueFromJson(const base::Value* value_in, T* value_out,
                        ErrorPtr* error) {
  if (!TypedValueFromJson(value_in, value_out)) {
    std::string value_as_string;
    base::JSONWriter::Write(value_in, &value_as_string);
    Error::AddToPrintf(
        error, commands::errors::kDomain, commands::errors::kTypeMismatch,
        "Unable to convert %s to %s", value_as_string.c_str(),
        PropType::GetTypeStringFromType(GetValueType<T>()).c_str());
    return false;
  }
  return true;
}

template<typename T>
static std::shared_ptr<Constraint> LoadOneOfConstraint(
    const base::DictionaryValue* value, ErrorPtr* error) {
  const base::ListValue* list = nullptr;
  if (!value->GetListWithoutPathExpansion(commands::attributes::kOneOf_Enum,
                                          &list)) {
    Error::AddTo(error, commands::errors::kDomain,
                 commands::errors::kTypeMismatch, "Expecting an array");
    return std::shared_ptr<Constraint>();
  }
  std::vector<T> set;
  set.reserve(list->GetSize());
  for (const base::Value* item : *list) {
    T val{};
    if (!TypedValueFromJson(item, &val, error))
      return std::shared_ptr<Constraint>();
    set.push_back(val);
  }
  InheritableAttribute<std::vector<T>> val(set, false);
  return std::make_shared<ConstraintOneOf<T>>(val);
}

template<class ConstraintClass, typename T>
static std::shared_ptr<Constraint> LoadMinMaxConstraint(
    const char* dict_key, const base::DictionaryValue* value, ErrorPtr* error) {
  InheritableAttribute<T> limit;

  const base::Value* src_val = nullptr;
  CHECK(value->Get(dict_key, &src_val)) << "Unable to get min/max constraints";
  if (!TypedValueFromJson(src_val, &limit.value, error))
    return std::shared_ptr<Constraint>();
  limit.is_inherited = false;

  return std::make_shared<ConstraintClass>(limit);
}

template<typename T>
bool NumericPropTypeBase::FromJsonHelper(const base::DictionaryValue* value,
                                         const PropType* schema,
                                         ErrorPtr* error) {
  if (!PropType::FromJson(value, schema, error))
    return false;

  if (value->HasKey(commands::attributes::kOneOf_Enum)) {
    auto constraint = LoadOneOfConstraint<T>(value, error);
    if (!constraint)
      return false;
    AddConstraint(constraint);
    RemoveConstraint(ConstraintType::Min);
    RemoveConstraint(ConstraintType::Max);
  } else {
    if (value->HasKey(commands::attributes::kNumeric_Min)) {
      auto constraint = LoadMinMaxConstraint<ConstraintMin<T>, T>(
          commands::attributes::kNumeric_Min, value, error);
      if (!constraint)
        return false;
      AddConstraint(constraint);
      RemoveConstraint(ConstraintType::OneOf);
    }
    if (value->HasKey(commands::attributes::kNumeric_Max)) {
      auto constraint = LoadMinMaxConstraint<ConstraintMax<T>, T>(
          commands::attributes::kNumeric_Max, value, error);
      if (!constraint)
        return false;
      AddConstraint(constraint);
      RemoveConstraint(ConstraintType::OneOf);
    }
  }

  return true;
}

// IntPropType ----------------------------------------------------------------

std::shared_ptr<PropType> IntPropType::Clone() const {
  return std::make_shared<IntPropType>(*this);
}

std::shared_ptr<PropValue> IntPropType::CreateValue() const {
  return std::make_shared<IntValue>();
}

// DoublePropType -------------------------------------------------------------

std::shared_ptr<PropType> DoublePropType::Clone() const {
  return std::make_shared<DoublePropType>(*this);
}

std::shared_ptr<PropValue> DoublePropType::CreateValue() const {
  return std::make_shared<DoubleValue>();
}

// StringPropType -------------------------------------------------------------

std::shared_ptr<PropType> StringPropType::Clone() const {
  return std::make_shared<StringPropType>(*this);
}

std::shared_ptr<PropValue> StringPropType::CreateValue() const {
  return std::make_shared<StringValue>();
}

bool StringPropType::FromJson(const base::DictionaryValue* value,
                              const PropType* schema, ErrorPtr* error) {
  if (!PropType::FromJson(value, schema, error))
    return false;
  if (value->HasKey(commands::attributes::kOneOf_Enum)) {
    auto constraint = LoadOneOfConstraint<std::string>(value, error);
    if (!constraint)
      return false;
    AddConstraint(constraint);
    RemoveConstraint(ConstraintType::StringLengthMin);
    RemoveConstraint(ConstraintType::StringLengthMax);
  } else {
    if (value->HasKey(commands::attributes::kString_MinLength)) {
      auto constraint = LoadMinMaxConstraint<ConstraintStringLengthMin, int>(
          commands::attributes::kString_MinLength, value, error);
      if (!constraint)
        return false;
      AddConstraint(constraint);
      RemoveConstraint(ConstraintType::OneOf);
    }
    if (value->HasKey(commands::attributes::kString_MaxLength)) {
      auto constraint = LoadMinMaxConstraint<ConstraintStringLengthMax, int>(
          commands::attributes::kString_MaxLength, value, error);
      if (!constraint)
        return false;
      AddConstraint(constraint);
      RemoveConstraint(ConstraintType::OneOf);
    }
  }
  return true;
}

void StringPropType::AddLengthConstraint(int min_len, int max_len) {
  InheritableAttribute<int> min_attr(min_len, false);
  InheritableAttribute<int> max_attr(max_len, false);
  AddConstraint(std::make_shared<ConstraintStringLengthMin>(min_attr));
  AddConstraint(std::make_shared<ConstraintStringLengthMax>(max_attr));
}

int StringPropType::GetMinLength() const {
  auto slc = static_cast<const ConstraintStringLength*>(
      GetConstraint(ConstraintType::StringLengthMin));
  return slc ? slc->limit_.value : 0;
}

int StringPropType::GetMaxLength() const {
  auto slc = static_cast<const ConstraintStringLength*>(
      GetConstraint(ConstraintType::StringLengthMax));
  return slc ? slc->limit_.value : std::numeric_limits<int>::max();
}

// BooleanPropType -----------------------------------------------------------

std::shared_ptr<PropType> BooleanPropType::Clone() const {
  return std::make_shared<BooleanPropType>(*this);
}

std::shared_ptr<PropValue> BooleanPropType::CreateValue() const {
  return std::make_shared<BooleanValue>();
}

bool BooleanPropType::FromJson(const base::DictionaryValue* value,
                               const PropType* schema, ErrorPtr* error) {
  if (!PropType::FromJson(value, schema, error))
    return false;

  if (value->HasKey(commands::attributes::kOneOf_Enum)) {
    auto constraint = LoadOneOfConstraint<bool>(value, error);
    if (!constraint)
      return false;
    AddConstraint(constraint);
  }

  return true;
}

}  // namespace buffet
