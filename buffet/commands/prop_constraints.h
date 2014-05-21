// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BUFFET_COMMANDS_PROP_CONSTRAINTS_H_
#define BUFFET_COMMANDS_PROP_CONSTRAINTS_H_

#include <limits>
#include <string>
#include <type_traits>
#include <vector>

#include <base/basictypes.h>
#include <base/values.h>

#include "buffet/any.h"
#include "buffet/commands/schema_constants.h"
#include "buffet/error.h"
#include "buffet/string_utils.h"

namespace buffet {

// InheritableAttribute class is used for specifying various command parameter
// attributes that can be inherited from a base (parent) schema.
// The |value| still specifies the actual attribute values, whether it
// is inherited or overridden, while |is_inherited| can be used to identify
// if the attribute was inherited (true) or overridden (false).
template<typename T>
class InheritableAttribute {
 public:
  InheritableAttribute() = default;
  explicit InheritableAttribute(T val)
      : value(std::move(val)), is_inherited(true) {}
  InheritableAttribute(T val, bool inherited)
      : value(std::move(val)), is_inherited(inherited) {}
  T value{};
  bool is_inherited{true};
};

// A bunch of helper function to create base::Value for specific C++ classes,
// including vectors of types. These are used in template classes below
// to simplify specialization logic.
std::unique_ptr<base::Value> TypedValueToJson(bool value);
std::unique_ptr<base::Value> TypedValueToJson(int value);
std::unique_ptr<base::Value> TypedValueToJson(double value);
std::unique_ptr<base::Value> TypedValueToJson(const std::string& value);
template<typename T>
std::unique_ptr<base::Value> TypedValueToJson(const std::vector<T>& values) {
  std::unique_ptr<base::ListValue> list(new base::ListValue);
  for (const auto& v : values)
    list->Append(TypedValueToJson(v).release());
  return std::unique_ptr<base::Value>(list.release());
}

// Similarly to CreateTypedValue() function above, the following overloaded
// helper methods allow to extract specific C++ data types from base::Value.
// Also used in template classes below to simplify specialization logic.
inline bool TypedValueFromJson(const base::Value* value_in, bool* value_out) {
  return value_in->GetAsBoolean(value_out);
}
inline bool TypedValueFromJson(const base::Value* value_in, int* value_out) {
  return value_in->GetAsInteger(value_out);
}
inline bool TypedValueFromJson(const base::Value* value_in, double* value_out) {
  return value_in->GetAsDouble(value_out);
}
inline bool TypedValueFromJson(const base::Value* value_in,
                               std::string* value_out) {
  return value_in->GetAsString(value_out);
}

//////////////////////////////////////////////////////////////////////////////

enum class ConstraintType {
  Min,
  Max,
  StringLengthMin,
  StringLengthMax,
  OneOf
};

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
  virtual bool Validate(const Any& value, ErrorPtr* error) const = 0;
  // Makes a copy of the constraint object, marking all the attributes
  // as inherited from the original definition.
  virtual std::shared_ptr<Constraint> CloneAsInherited() const = 0;
  // Saves the constraint into the specified JSON |dict| object, representing
  // the object schema. If |overridden_only| is set to true, then the
  // inherited constraints will not be added to the schema object.
  virtual bool AddToJsonDict(base::DictionaryValue* dict, bool overridden_only,
                             ErrorPtr* error) const;
  // Saves the value of constraint to JSON value. E.g., if the numeric
  // constraint was defined as {"minimum":20} this will create a JSON value
  // of 20. The current design implies that each constraint has one value
  // only. If this assumption changes, this interface needs to be updated
  // accordingly.
  virtual std::unique_ptr<base::Value> ToJson(ErrorPtr* error) const = 0;
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
  static bool ReportErrorLessThan(ErrorPtr* error, const std::string& val,
                                  const std::string& limit);
  static bool ReportErrorGreaterThan(ErrorPtr* error, const std::string& val,
                                     const std::string& limit);

  static bool ReportErrorNotOneOf(ErrorPtr* error, const std::string& val,
                                  const std::vector<std::string>& values);

 private:
  DISALLOW_COPY_AND_ASSIGN(Constraint);
};

// ConstraintMinMaxBase is a base class for numeric Minimum and Maximum
// constraints.
template<typename T>
class ConstraintMinMaxBase : public Constraint {
 public:
  explicit ConstraintMinMaxBase(const InheritableAttribute<T>& limit)
      : limit_(limit) {}
  explicit ConstraintMinMaxBase(const T& limit)
      : limit_(limit) {}

  // Implementation of Constraint::HasOverriddenAttributes().
  virtual bool HasOverriddenAttributes() const override {
    return !limit_.is_inherited;
  }

  // Implementation of Constraint::ToJson().
  virtual std::unique_ptr<base::Value> ToJson(ErrorPtr* error) const override {
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
template<typename T>
class ConstraintMin : public ConstraintMinMaxBase<T> {
 public:
  explicit ConstraintMin(const InheritableAttribute<T>& limit)
      : ConstraintMinMaxBase<T>(limit) {}
  explicit ConstraintMin(const T& limit)
      : ConstraintMinMaxBase<T>(limit) {}

  // Implementation of Constraint::GetType().
  virtual ConstraintType GetType() const { return ConstraintType::Min; }

  // Implementation of Constraint::Validate().
  virtual bool Validate(const Any& value, ErrorPtr* error) const override {
    const T& v = value.Get<T>();
    if (v < this->limit_.value)
      return this->ReportErrorLessThan(
          error, string_utils::ToString(v),
          string_utils::ToString(this->limit_.value));
    return true;
  }

  // Implementation of Constraint::CloneAsInherited().
  virtual std::shared_ptr<Constraint> CloneAsInherited() const override {
    return std::make_shared<ConstraintMin>(this->limit_.value);
  }

  // Implementation of Constraint::GetDictKey().
  virtual const char* GetDictKey() const override {
    return commands::attributes::kNumeric_Min;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ConstraintMin);
};

// Implementation of Maximum value constraint for Integer/Double types.
template<typename T>
class ConstraintMax : public ConstraintMinMaxBase<T> {
 public:
  explicit ConstraintMax(const InheritableAttribute<T>& limit)
      : ConstraintMinMaxBase<T>(limit) {}
  explicit ConstraintMax(const T& limit)
      : ConstraintMinMaxBase<T>(limit) {}

  // Implementation of Constraint::GetType().
  virtual ConstraintType GetType() const { return ConstraintType::Max; }

  // Implementation of Constraint::Validate().
  virtual bool Validate(const Any& value, ErrorPtr* error) const override {
    const T& v = value.Get<T>();
    if (v > this->limit_.value)
      return this->ReportErrorGreaterThan(
          error, string_utils::ToString(v),
          string_utils::ToString(this->limit_.value));
    return true;
  }

  // Implementation of Constraint::CloneAsInherited().
  virtual std::shared_ptr<Constraint> CloneAsInherited() const override {
    return std::make_shared<ConstraintMax>(this->limit_.value);
  }

  // Implementation of Constraint::GetDictKey().
  virtual const char* GetDictKey() const override {
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
  virtual bool HasOverriddenAttributes() const override;
  // Implementation of Constraint::ToJson().
  virtual std::unique_ptr<base::Value> ToJson(ErrorPtr* error) const override;

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
  virtual ConstraintType GetType() const override {
    return ConstraintType::StringLengthMin;
  }
  // Implementation of Constraint::Validate().
  virtual bool Validate(const Any& value, ErrorPtr* error) const override;
  // Implementation of Constraint::CloneAsInherited().
  virtual std::shared_ptr<Constraint> CloneAsInherited() const override;
  // Implementation of Constraint::GetDictKey().
  virtual const char* GetDictKey() const override {
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
  virtual ConstraintType GetType() const override {
    return ConstraintType::StringLengthMax;
  }
  // Implementation of Constraint::Validate().
  virtual bool Validate(const Any& value, ErrorPtr* error) const override;
  // Implementation of Constraint::CloneAsInherited().
  virtual std::shared_ptr<Constraint> CloneAsInherited() const override;
  // Implementation of Constraint::GetDictKey().
  virtual const char* GetDictKey() const override {
    return commands::attributes::kString_MaxLength;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ConstraintStringLengthMax);
};

// CompareValue is a helper functor to help with implementing EqualsTo operator
// for various data types. For most scalar types it is using operator==(),
// however, for floating point values, rounding errors in binary representation
// of IEEE floats/doubles can cause straight == comparison to fail for seemingly
// equivalent values. For these, use approximate comparison with the error
// margin equal to the epsilon value defined for the corresponding data type.

// Compare exact types using ==.
template<typename T, bool exact = true>
struct CompareValue {
  inline bool operator()(const T& v1, const T& v2) { return v1 == v2; }
};

// Compare non-exact types (such as double) using precision margin (epsilon).
template<typename T>
struct CompareValue<T, false> {
  inline bool operator()(const T& v1, const T& v2) {
    return std::abs(v1 - v2) <= std::numeric_limits<T>::epsilon();
  }
};

// Implementation of OneOf constraint for different data types.
template<typename T>
class ConstraintOneOf : public Constraint {
 public:
  explicit ConstraintOneOf(const InheritableAttribute<std::vector<T>>& set)
      : set_(set) {}
  explicit ConstraintOneOf(const std::vector<T>& set)
      : set_(set) {}

  // Implementation of Constraint::GetType().
  virtual ConstraintType GetType() const override {
    return ConstraintType::OneOf;
  }

  // Implementation of Constraint::HasOverriddenAttributes().
  virtual bool HasOverriddenAttributes() const override {
    return !set_.is_inherited;
  }

  // Implementation of Constraint::Validate().
  virtual bool Validate(const Any& value, ErrorPtr* error) const override {
    const T& v = value.Get<T>();
    constexpr bool exact_type = std::is_same<T, std::string>::value ||
                                std::numeric_limits<T>::is_exact;
    for (const auto& item : set_.value) {
      if (CompareValue<T, exact_type>()(v, item))
        return true;
    }
    std::vector<std::string> values;
    values.reserve(set_.value.size());
    for (const auto& item : set_.value) {
      values.push_back(string_utils::ToString(item));
    }
    return ReportErrorNotOneOf(error, string_utils::ToString(v), values);
  }

  // Implementation of Constraint::CloneAsInherited().
  virtual std::shared_ptr<Constraint> CloneAsInherited() const override {
    return std::make_shared<ConstraintOneOf>(set_.value);
  }

  // Implementation of Constraint::ToJson().
  virtual std::unique_ptr<base::Value> ToJson(ErrorPtr* error) const override {
    return TypedValueToJson(set_.value);
  }

  // Implementation of Constraint::GetDictKey().
  virtual const char* GetDictKey() const override {
    return commands::attributes::kOneOf_Enum;
  }

  // Stores the list of acceptable values for the parameter.
  // |set_.is_inherited| indicates whether the constraint is inherited
  // from base schema or overridden.
  InheritableAttribute<std::vector<T>> set_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ConstraintOneOf);
};

}  // namespace buffet

#endif  // BUFFET_COMMANDS_PROP_CONSTRAINTS_H_
