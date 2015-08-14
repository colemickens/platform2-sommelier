// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBWEAVE_SRC_COMMANDS_PROP_CONSTRAINTS_H_
#define LIBWEAVE_SRC_COMMANDS_PROP_CONSTRAINTS_H_

#include <string>
#include <type_traits>
#include <vector>

#include <base/macros.h>
#include <base/values.h>
#include <chromeos/errors/error.h>

#include "libweave/src/commands/prop_values.h"
#include "libweave/src/commands/schema_constants.h"
#include "libweave/src/commands/schema_utils.h"
#include "libweave/src/string_utils.h"

namespace weave {

enum class ConstraintType { Min, Max, StringLengthMin, StringLengthMax, OneOf };

// Abstract base class for all parameter constraints. Many constraints are
// type-dependent. Thus, a numeric parameter could have "minimum" and/or
// "maximum" constraints specified. Some constraints, such as "OneOf" apply to
// any data type.
class Constraint {
 public:
  Constraint() = default;
  virtual ~Constraint();

  // Gets the constraint type.
  virtual ConstraintType GetType() const = 0;

  // Checks if any of the constraint properties/attributes are overridden
  // from their base schema definition. If the constraint is inherited, then
  // it will not be written to JSON when saving partial schema.
  virtual bool HasOverriddenAttributes() const = 0;

  // Validates a parameter against the constraint. Returns true if parameter
  // value satisfies the constraint, otherwise fills the optional |error| with
  // the details for the failure.
  virtual bool Validate(const PropValue& value,
                        chromeos::ErrorPtr* error) const = 0;

  // Makes a full copy of this Constraint instance.
  virtual std::unique_ptr<Constraint> Clone() const = 0;

  // Makes a copy of the constraint object, marking all the attributes
  // as inherited from the original definition.
  virtual std::unique_ptr<Constraint> CloneAsInherited() const = 0;

  // Saves the constraint into the specified JSON |dict| object, representing
  // the object schema. If |overridden_only| is set to true, then the
  // inherited constraints will not be added to the schema object.
  virtual void AddToJsonDict(base::DictionaryValue* dict,
                             bool overridden_only) const;

  // Saves the value of constraint to JSON value. E.g., if the numeric
  // constraint was defined as {"minimum":20} this will create a JSON value
  // of 20. The current design implies that each constraint has one value
  // only. If this assumption changes, this interface needs to be updated
  // accordingly.
  virtual std::unique_ptr<base::Value> ToJson() const = 0;

  // Overloaded by the concrete class implementation, it should return the
  // JSON object property name to store the constraint's value as.
  // E.g., if the numeric constraint was defined as {"minimum":20} this
  // method should return "minimum".
  virtual const char* GetDictKey() const = 0;

 protected:
  // Static helper methods to format common constraint validation errors.
  // They fill the |error| object with specific error message.
  // Since these functions could be used by constraint objects for various
  // data types, the values used in validation are expected to be
  // send as strings already.
  static bool ReportErrorLessThan(chromeos::ErrorPtr* error,
                                  const std::string& val,
                                  const std::string& limit);
  static bool ReportErrorGreaterThan(chromeos::ErrorPtr* error,
                                     const std::string& val,
                                     const std::string& limit);

  static bool ReportErrorNotOneOf(chromeos::ErrorPtr* error,
                                  const std::string& val,
                                  const std::vector<std::string>& values);

 private:
  DISALLOW_COPY_AND_ASSIGN(Constraint);
};

// ConstraintMinMaxBase is a base class for numeric Minimum and Maximum
// constraints.
template <typename T>
class ConstraintMinMaxBase : public Constraint {
 public:
  explicit ConstraintMinMaxBase(const InheritableAttribute<T>& limit)
      : limit_(limit) {}
  explicit ConstraintMinMaxBase(const T& limit) : limit_(limit) {}

  // Implementation of Constraint::HasOverriddenAttributes().
  bool HasOverriddenAttributes() const override { return !limit_.is_inherited; }

  // Implementation of Constraint::ToJson().
  std::unique_ptr<base::Value> ToJson() const override {
    return TypedValueToJson(limit_.value);
  }

  // Stores the upper/lower value limit for maximum/minimum constraint.
  // |limit_.is_inherited| indicates whether the constraint is inherited
  // from base schema or overridden.
  InheritableAttribute<T> limit_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ConstraintMinMaxBase);
};

// Implementation of Minimum value constraint for Integer/Double types.
template <typename T>
class ConstraintMin : public ConstraintMinMaxBase<T> {
 public:
  explicit ConstraintMin(const InheritableAttribute<T>& limit)
      : ConstraintMinMaxBase<T>(limit) {}
  explicit ConstraintMin(const T& limit) : ConstraintMinMaxBase<T>(limit) {}

  // Implementation of Constraint::GetType().
  ConstraintType GetType() const override { return ConstraintType::Min; }

  // Implementation of Constraint::Validate().
  bool Validate(const PropValue& value,
                chromeos::ErrorPtr* error) const override {
    const T& v = static_cast<const TypedValueBase<T>&>(value).GetValue();
    if (v < this->limit_.value) {
      return this->ReportErrorLessThan(error, std::to_string(v),
                                       std::to_string(this->limit_.value));
    }
    return true;
  }

  // Implementation of Constraint::Clone().
  std::unique_ptr<Constraint> Clone() const override {
    return std::unique_ptr<Constraint>{new ConstraintMin{this->limit_}};
  }

  // Implementation of Constraint::CloneAsInherited().
  std::unique_ptr<Constraint> CloneAsInherited() const override {
    return std::unique_ptr<Constraint>{new ConstraintMin{this->limit_.value}};
  }

  // Implementation of Constraint::GetDictKey().
  const char* GetDictKey() const override {
    return commands::attributes::kNumeric_Min;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ConstraintMin);
};

// Implementation of Maximum value constraint for Integer/Double types.
template <typename T>
class ConstraintMax : public ConstraintMinMaxBase<T> {
 public:
  explicit ConstraintMax(const InheritableAttribute<T>& limit)
      : ConstraintMinMaxBase<T>(limit) {}
  explicit ConstraintMax(const T& limit) : ConstraintMinMaxBase<T>(limit) {}

  // Implementation of Constraint::GetType().
  ConstraintType GetType() const override { return ConstraintType::Max; }

  // Implementation of Constraint::Validate().
  bool Validate(const PropValue& value,
                chromeos::ErrorPtr* error) const override {
    const T& v = static_cast<const TypedValueBase<T>&>(value).GetValue();
    if (v > this->limit_.value)
      return this->ReportErrorGreaterThan(error, std::to_string(v),
                                          std::to_string(this->limit_.value));
    return true;
  }

  // Implementation of Constraint::Clone().
  std::unique_ptr<Constraint> Clone() const override {
    return std::unique_ptr<Constraint>{new ConstraintMax{this->limit_}};
  }

  // Implementation of Constraint::CloneAsInherited().
  std::unique_ptr<Constraint> CloneAsInherited() const override {
    return std::unique_ptr<Constraint>{new ConstraintMax{this->limit_.value}};
  }

  // Implementation of Constraint::GetDictKey().
  const char* GetDictKey() const override {
    return commands::attributes::kNumeric_Max;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ConstraintMax);
};

// ConstraintStringLength is a base class for Minimum/Maximum string length
// constraints, similar to ConstraintMinMaxBase of numeric types.
class ConstraintStringLength : public Constraint {
 public:
  explicit ConstraintStringLength(const InheritableAttribute<int>& limit);
  explicit ConstraintStringLength(int limit);

  // Implementation of Constraint::HasOverriddenAttributes().
  bool HasOverriddenAttributes() const override;
  // Implementation of Constraint::ToJson().
  std::unique_ptr<base::Value> ToJson() const override;

  // Stores the upper/lower value limit for string length constraint.
  // |limit_.is_inherited| indicates whether the constraint is inherited
  // from base schema or overridden.
  InheritableAttribute<int> limit_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ConstraintStringLength);
};

// Implementation of Minimum string length constraint.
class ConstraintStringLengthMin : public ConstraintStringLength {
 public:
  explicit ConstraintStringLengthMin(const InheritableAttribute<int>& limit);
  explicit ConstraintStringLengthMin(int limit);

  // Implementation of Constraint::GetType().
  ConstraintType GetType() const override {
    return ConstraintType::StringLengthMin;
  }

  // Implementation of Constraint::Validate().
  bool Validate(const PropValue& value,
                chromeos::ErrorPtr* error) const override;

  // Implementation of Constraint::Clone().
  std::unique_ptr<Constraint> Clone() const override;

  // Implementation of Constraint::CloneAsInherited().
  std::unique_ptr<Constraint> CloneAsInherited() const override;
  // Implementation of Constraint::GetDictKey().
  const char* GetDictKey() const override {
    return commands::attributes::kString_MinLength;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ConstraintStringLengthMin);
};

// Implementation of Maximum string length constraint.
class ConstraintStringLengthMax : public ConstraintStringLength {
 public:
  explicit ConstraintStringLengthMax(const InheritableAttribute<int>& limit);
  explicit ConstraintStringLengthMax(int limit);

  // Implementation of Constraint::GetType().
  ConstraintType GetType() const override {
    return ConstraintType::StringLengthMax;
  }

  // Implementation of Constraint::Validate().
  bool Validate(const PropValue& value,
                chromeos::ErrorPtr* error) const override;

  // Implementation of Constraint::Clone().
  std::unique_ptr<Constraint> Clone() const override;

  // Implementation of Constraint::CloneAsInherited().
  std::unique_ptr<Constraint> CloneAsInherited() const override;

  // Implementation of Constraint::GetDictKey().
  const char* GetDictKey() const override {
    return commands::attributes::kString_MaxLength;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ConstraintStringLengthMax);
};

// Implementation of OneOf constraint for different data types.
class ConstraintOneOf : public Constraint {
 public:
  explicit ConstraintOneOf(InheritableAttribute<ValueVector> set);
  explicit ConstraintOneOf(ValueVector set);

  // Implementation of Constraint::GetType().
  ConstraintType GetType() const override { return ConstraintType::OneOf; }

  // Implementation of Constraint::HasOverriddenAttributes().
  bool HasOverriddenAttributes() const override { return !set_.is_inherited; }

  // Implementation of Constraint::Validate().
  bool Validate(const PropValue& value,
                chromeos::ErrorPtr* error) const override;

  // Implementation of Constraint::Clone().
  std::unique_ptr<Constraint> Clone() const override;

  // Implementation of Constraint::CloneAsInherited().
  std::unique_ptr<Constraint> CloneAsInherited() const override;

  // Implementation of Constraint::ToJson().
  std::unique_ptr<base::Value> ToJson() const override;

  // Implementation of Constraint::GetDictKey().
  const char* GetDictKey() const override;

  // Stores the list of acceptable values for the parameter.
  // |set_.is_inherited| indicates whether the constraint is inherited
  // from base schema or overridden.
  InheritableAttribute<ValueVector> set_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ConstraintOneOf);
};

}  // namespace weave

#endif  // LIBWEAVE_SRC_COMMANDS_PROP_CONSTRAINTS_H_
