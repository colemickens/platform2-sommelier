// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "buffet/commands/prop_constraints.h"
#include "buffet/commands/prop_values.h"
#include "buffet/commands/schema_constants.h"

namespace buffet {

// Constraint ----------------------------------------------------------------
Constraint::~Constraint() {}

bool Constraint::ReportErrorLessThan(chromeos::ErrorPtr* error,
                                     const std::string& val,
                                     const std::string& limit) {
  chromeos::Error::AddToPrintf(
      error, FROM_HERE, errors::commands::kDomain,
      errors::commands::kOutOfRange,
      "Value %s is out of range. It must not be less than %s",
      val.c_str(), limit.c_str());
  return false;
}

bool Constraint::ReportErrorGreaterThan(chromeos::ErrorPtr* error,
                                        const std::string& val,
                                        const std::string& limit) {
  chromeos::Error::AddToPrintf(
      error, FROM_HERE, errors::commands::kDomain,
      errors::commands::kOutOfRange,
      "Value %s is out of range. It must not be greater than %s",
      val.c_str(), limit.c_str());
  return false;
}

bool Constraint::ReportErrorNotOneOf(chromeos::ErrorPtr* error,
                                     const std::string& val,
                                     const std::vector<std::string>& values) {
  chromeos::Error::AddToPrintf(error, FROM_HERE, errors::commands::kDomain,
                               errors::commands::kOutOfRange,
                               "Value %s is invalid. Expected one of [%s]",
                               val.c_str(),
                               chromeos::string_utils::Join(',',
                                                            values).c_str());
  return false;
}

bool Constraint::AddToJsonDict(base::DictionaryValue* dict,
                               bool overridden_only,
                               chromeos::ErrorPtr* error) const {
  if (!overridden_only || HasOverriddenAttributes()) {
    auto value = ToJson(error);
    if (!value)
      return false;
    dict->SetWithoutPathExpansion(GetDictKey(), value.release());
  }
  return true;
}

// ConstraintStringLength -----------------------------------------------------
ConstraintStringLength::ConstraintStringLength(
    const InheritableAttribute<int>& limit) : limit_(limit) {}
ConstraintStringLength::ConstraintStringLength(int limit) : limit_(limit) {}

bool ConstraintStringLength::HasOverriddenAttributes() const {
  return !limit_.is_inherited;
}

std::unique_ptr<base::Value> ConstraintStringLength::ToJson(
    chromeos::ErrorPtr* error) const {
  return TypedValueToJson(limit_.value, error);
}

// ConstraintStringLengthMin --------------------------------------------------
ConstraintStringLengthMin::ConstraintStringLengthMin(
    const InheritableAttribute<int>& limit) : ConstraintStringLength(limit) {}
ConstraintStringLengthMin::ConstraintStringLengthMin(int limit)
    : ConstraintStringLength(limit) {}

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

std::shared_ptr<Constraint>
    ConstraintStringLengthMin::CloneAsInherited() const {
  return std::make_shared<ConstraintStringLengthMin>(limit_.value);
}

// ConstraintStringLengthMax --------------------------------------------------
ConstraintStringLengthMax::ConstraintStringLengthMax(
    const InheritableAttribute<int>& limit) : ConstraintStringLength(limit) {}
ConstraintStringLengthMax::ConstraintStringLengthMax(int limit)
    : ConstraintStringLength(limit) {}

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

std::shared_ptr<Constraint>
    ConstraintStringLengthMax::CloneAsInherited() const {
  return std::make_shared<ConstraintStringLengthMax>(limit_.value);
}

}  // namespace buffet
