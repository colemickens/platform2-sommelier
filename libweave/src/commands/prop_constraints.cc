// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libweave/src/commands/prop_constraints.h"

#include <base/json/json_writer.h>

#include "libweave/src/commands/prop_values.h"
#include "libweave/src/commands/schema_constants.h"
#include "libweave/src/string_utils.h"

namespace weave {

namespace {

// Helper function to convert a property value to string, which is used for
// error reporting.
std::string PropValueToString(const PropValue& value) {
  std::string result;
  auto json = value.ToJson();
  CHECK(json);
  base::JSONWriter::Write(*json, &result);
  return result;
}

}  // anonymous namespace

// Constraint ----------------------------------------------------------------
Constraint::~Constraint() {
}

bool Constraint::ReportErrorLessThan(chromeos::ErrorPtr* error,
                                     const std::string& val,
                                     const std::string& limit) {
  chromeos::Error::AddToPrintf(
      error, FROM_HERE, errors::commands::kDomain,
      errors::commands::kOutOfRange,
      "Value %s is out of range. It must not be less than %s", val.c_str(),
      limit.c_str());
  return false;
}

bool Constraint::ReportErrorGreaterThan(chromeos::ErrorPtr* error,
                                        const std::string& val,
                                        const std::string& limit) {
  chromeos::Error::AddToPrintf(
      error, FROM_HERE, errors::commands::kDomain,
      errors::commands::kOutOfRange,
      "Value %s is out of range. It must not be greater than %s", val.c_str(),
      limit.c_str());
  return false;
}

bool Constraint::ReportErrorNotOneOf(chromeos::ErrorPtr* error,
                                     const std::string& val,
                                     const std::vector<std::string>& values) {
  chromeos::Error::AddToPrintf(error, FROM_HERE, errors::commands::kDomain,
                               errors::commands::kOutOfRange,
                               "Value %s is invalid. Expected one of [%s]",
                               val.c_str(), Join(",", values).c_str());
  return false;
}

void Constraint::AddToJsonDict(base::DictionaryValue* dict,
                               bool overridden_only) const {
  if (!overridden_only || HasOverriddenAttributes()) {
    auto value = ToJson();
    CHECK(value);
    dict->SetWithoutPathExpansion(GetDictKey(), value.release());
  }
}

// ConstraintStringLength -----------------------------------------------------
ConstraintStringLength::ConstraintStringLength(
    const InheritableAttribute<int>& limit)
    : limit_(limit) {
}
ConstraintStringLength::ConstraintStringLength(int limit) : limit_(limit) {
}

bool ConstraintStringLength::HasOverriddenAttributes() const {
  return !limit_.is_inherited;
}

std::unique_ptr<base::Value> ConstraintStringLength::ToJson() const {
  return TypedValueToJson(limit_.value);
}

// ConstraintStringLengthMin --------------------------------------------------
ConstraintStringLengthMin::ConstraintStringLengthMin(
    const InheritableAttribute<int>& limit)
    : ConstraintStringLength(limit) {
}
ConstraintStringLengthMin::ConstraintStringLengthMin(int limit)
    : ConstraintStringLength(limit) {
}

bool ConstraintStringLengthMin::Validate(const PropValue& value,
                                         chromeos::ErrorPtr* error) const {
  CHECK(value.GetString()) << "Expecting a string value for this constraint";
  const std::string& str = value.GetString()->GetValue();
  int length = static_cast<int>(str.size());
  if (length < limit_.value) {
    if (limit_.value == 1) {
      chromeos::Error::AddTo(error, FROM_HERE, errors::commands::kDomain,
                             errors::commands::kOutOfRange,
                             "String must not be empty");
    } else {
      chromeos::Error::AddToPrintf(error, FROM_HERE, errors::commands::kDomain,
                                   errors::commands::kOutOfRange,
                                   "String must be at least %d characters long,"
                                   " actual length of string '%s' is %d",
                                   limit_.value, str.c_str(), length);
    }
    return false;
  }
  return true;
}

std::unique_ptr<Constraint> ConstraintStringLengthMin::Clone() const {
  return std::unique_ptr<Constraint>{new ConstraintStringLengthMin{limit_}};
}

std::unique_ptr<Constraint> ConstraintStringLengthMin::CloneAsInherited()
    const {
  return std::unique_ptr<Constraint>{
      new ConstraintStringLengthMin{limit_.value}};
}

// ConstraintStringLengthMax --------------------------------------------------
ConstraintStringLengthMax::ConstraintStringLengthMax(
    const InheritableAttribute<int>& limit)
    : ConstraintStringLength(limit) {
}
ConstraintStringLengthMax::ConstraintStringLengthMax(int limit)
    : ConstraintStringLength(limit) {
}

bool ConstraintStringLengthMax::Validate(const PropValue& value,
                                         chromeos::ErrorPtr* error) const {
  CHECK(value.GetString()) << "Expecting a string value for this constraint";
  const std::string& str = value.GetString()->GetValue();
  int length = static_cast<int>(str.size());
  if (length > limit_.value) {
    chromeos::Error::AddToPrintf(error, FROM_HERE, errors::commands::kDomain,
                                 errors::commands::kOutOfRange,
                                 "String must be no more than %d character(s) "
                                 "long, actual length of string '%s' is %d",
                                 limit_.value, str.c_str(), length);
    return false;
  }
  return true;
}

std::unique_ptr<Constraint> ConstraintStringLengthMax::Clone() const {
  return std::unique_ptr<Constraint>{new ConstraintStringLengthMax{limit_}};
}

std::unique_ptr<Constraint> ConstraintStringLengthMax::CloneAsInherited()
    const {
  return std::unique_ptr<Constraint>{
      new ConstraintStringLengthMax{limit_.value}};
}

// ConstraintOneOf --------------------------------------------------
ConstraintOneOf::ConstraintOneOf(InheritableAttribute<ValueVector> set)
    : set_(std::move(set)) {}
ConstraintOneOf::ConstraintOneOf(ValueVector set) : set_(std::move(set)) {}

bool ConstraintOneOf::Validate(const PropValue& value,
                               chromeos::ErrorPtr* error) const {
  for (const auto& item : set_.value) {
    if (value.IsEqual(item.get()))
      return true;
  }
  std::vector<std::string> choice_list;
  choice_list.reserve(set_.value.size());
  for (const auto& item : set_.value) {
    choice_list.push_back(PropValueToString(*item));
  }
  return ReportErrorNotOneOf(error, PropValueToString(value), choice_list);
}

std::unique_ptr<Constraint> ConstraintOneOf::Clone() const {
  InheritableAttribute<ValueVector> attr;
  attr.is_inherited = set_.is_inherited;
  attr.value.reserve(set_.value.size());
  for (const auto& prop_value : set_.value) {
    attr.value.push_back(prop_value->Clone());
  }
  return std::unique_ptr<Constraint>{new ConstraintOneOf{std::move(attr)}};
}

std::unique_ptr<Constraint> ConstraintOneOf::CloneAsInherited() const {
  ValueVector cloned;
  cloned.reserve(set_.value.size());
  for (const auto& prop_value : set_.value) {
    cloned.push_back(prop_value->Clone());
  }
  return std::unique_ptr<Constraint>{new ConstraintOneOf{std::move(cloned)}};
}

std::unique_ptr<base::Value> ConstraintOneOf::ToJson() const {
  return TypedValueToJson(set_.value);
}

const char* ConstraintOneOf::GetDictKey() const {
  return commands::attributes::kOneOf_Enum;
}

}  // namespace weave
