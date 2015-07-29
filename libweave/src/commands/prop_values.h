// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBWEAVE_SRC_COMMANDS_PROP_VALUES_H_
#define LIBWEAVE_SRC_COMMANDS_PROP_VALUES_H_

#include <map>
#include <memory>
#include <string>

#include <chromeos/errors/error.h>

#include "libweave/src/commands/schema_utils.h"

namespace base {
class Value;
class DictionaryValue;
}  // namespace base

namespace weave {

// Enumeration to indicate supported command parameter types.
enum class ValueType {
  Int,
  Double,
  String,
  Boolean,
  Object,
  Array,
};

class PropValue;
class IntValue;
class DoubleValue;
class StringValue;
class BooleanValue;
class ObjectValue;
class ArrayValue;

class PropType;

// Helper methods to get the parameter type enum value for the given
// native C++ data representation.
// The generic GetValueType<T>() is undefined, however particular
// type specializations return the appropriate ValueType.
template <typename T>
ValueType GetValueType();  // Undefined.
template <>
inline ValueType GetValueType<int>() {
  return ValueType::Int;
}
template <>
inline ValueType GetValueType<double>() {
  return ValueType::Double;
}
template <>
inline ValueType GetValueType<std::string>() {
  return ValueType::String;
}
template <>
inline ValueType GetValueType<bool>() {
  return ValueType::Boolean;
}
template <>
inline ValueType GetValueType<ValueMap>() {
  return ValueType::Object;
}
template <>
inline ValueType GetValueType<ValueVector>() {
  return ValueType::Array;
}

// The base class for property values.
// Concrete value classes of various types will be derived from this base.
// A property value is the actual command parameter value (or a concrete value
// that can be used in constraints and presets). The PropValue is mostly
// just parsed content of base::Value when a command is dispatched, however
// it does have some additional functionality:
//   - it has a reference to the type definition (PropType) which is used
//     when validating the value, especially for "object" types.
//   - it can be compared with another instances of values of the same type.
//     This is used to validate the values against "enum"/"one of" constraints.
class PropValue {
 public:
  explicit PropValue(std::unique_ptr<const PropType> type);
  virtual ~PropValue();

  // Gets the type of the value.
  virtual ValueType GetType() const = 0;

  // Type conversion methods. Used in lieu of RTTI and dynamic_cast<>.
  virtual IntValue* GetInt() { return nullptr; }
  virtual IntValue const* GetInt() const { return nullptr; }
  virtual DoubleValue* GetDouble() { return nullptr; }
  virtual DoubleValue const* GetDouble() const { return nullptr; }
  virtual StringValue* GetString() { return nullptr; }
  virtual StringValue const* GetString() const { return nullptr; }
  virtual BooleanValue* GetBoolean() { return nullptr; }
  virtual BooleanValue const* GetBoolean() const { return nullptr; }
  virtual ObjectValue* GetObject() { return nullptr; }
  virtual ObjectValue const* GetObject() const { return nullptr; }
  virtual ArrayValue* GetArray() { return nullptr; }
  virtual ArrayValue const* GetArray() const { return nullptr; }

  // Makes a full copy of this value class.
  virtual std::unique_ptr<PropValue> Clone() const = 0;

  // Saves the value as a JSON object. Never fails.
  virtual std::unique_ptr<base::Value> ToJson() const = 0;
  // Parses a value from JSON.
  // If it fails, it returns false and provides additional information
  // via the |error| parameter.
  virtual bool FromJson(const base::Value* value,
                        chromeos::ErrorPtr* error) = 0;

  // Return the type definition of this value.
  const PropType* GetPropType() const { return type_.get(); }
  // Compares two values and returns true if they are equal.
  virtual bool IsEqual(const PropValue* value) const = 0;

 protected:
  // Special out-of-line constructor to help implement PropValue::Clone().
  // That method needs to clone the underlying type but can't do this in this
  // header file since PropType is just forward-declared (it needs PropValue
  // fully defined in its own inner workings).
  explicit PropValue(const PropValue& other);

 private:
  std::unique_ptr<const PropType> type_;
};

// A helper template base class for implementing value classes.
template <typename T>
class TypedValueBase : public PropValue {
 public:
  // To help refer to this base class from derived classes, define Base to
  // be this class.
  using Base = TypedValueBase<T>;
  using PropValue::PropValue;

  // Overrides from PropValue base class.
  ValueType GetType() const override { return GetValueType<T>(); }

  std::unique_ptr<base::Value> ToJson() const override {
    return TypedValueToJson(value_);
  }

  bool FromJson(const base::Value* value, chromeos::ErrorPtr* error) override {
    T tmp_value;
    if (!TypedValueFromJson(value, GetPropType(), &tmp_value, error))
      return false;
    return SetValue(tmp_value, error);
  }

  bool IsEqual(const PropValue* value) const override {
    if (GetType() != value->GetType())
      return false;
    const Base* value_base = static_cast<const Base*>(value);
    return CompareValue(GetValue(), value_base->GetValue());
  }

  // Helper methods to get and set the C++ representation of the value.
  const T& GetValue() const { return value_; }
  bool SetValue(T value, chromeos::ErrorPtr* error) {
    std::swap(value_, value);  // Backup.
    if (GetPropType()->ValidateConstraints(*this, error))
      return true;
    std::swap(value_, value);  // Restore.
    return false;
  }

 protected:
  explicit TypedValueBase(const TypedValueBase& other)
      : PropValue(other), value_(other.value_) {}

 private:
  T value_{};  // The value of the parameter in C++ data representation.
};

// A helper template base class for implementing value classes.
template <typename Derived, typename T>
class TypedValueWithClone : public TypedValueBase<T> {
 public:
  using Base = TypedValueWithClone<Derived, T>;

  // Expose the custom constructor of the base class.
  using TypedValueBase<T>::TypedValueBase;
  using PropValue::GetPropType;

  std::unique_ptr<PropValue> Clone() const override {
    return std::unique_ptr<PropValue>{
        new Derived{*static_cast<const Derived*>(this)}};
  }
};

// Value of type Integer.
class IntValue final : public TypedValueWithClone<IntValue, int> {
 public:
  using Base::Base;  // Expose the custom constructor of the base class.
  IntValue* GetInt() override { return this; }
  IntValue const* GetInt() const override { return this; }
};

// Value of type Number.
class DoubleValue final : public TypedValueWithClone<DoubleValue, double> {
 public:
  using Base::Base;  // Expose the custom constructor of the base class.
  DoubleValue* GetDouble() override { return this; }
  DoubleValue const* GetDouble() const override { return this; }
};

// Value of type String.
class StringValue final : public TypedValueWithClone<StringValue, std::string> {
 public:
  using Base::Base;  // Expose the custom constructor of the base class.
  StringValue* GetString() override { return this; }
  StringValue const* GetString() const override { return this; }
};

// Value of type Boolean.
class BooleanValue final : public TypedValueWithClone<BooleanValue, bool> {
 public:
  using Base::Base;  // Expose the custom constructor of the base class.
  BooleanValue* GetBoolean() override { return this; }
  BooleanValue const* GetBoolean() const override { return this; }
};

// Value of type Object.
class ObjectValue final : public TypedValueWithClone<ObjectValue, ValueMap> {
 public:
  using Base::Base;  // Expose the custom constructor of the base class.
  ObjectValue* GetObject() override { return this; }
  ObjectValue const* GetObject() const override { return this; }
};

// Value of type Array.
class ArrayValue final : public TypedValueWithClone<ArrayValue, ValueVector> {
 public:
  using Base::Base;  // Expose the custom constructor of the base class.
  ArrayValue* GetArray() override { return this; }
  ArrayValue const* GetArray() const override { return this; }
};

}  // namespace weave

#endif  // LIBWEAVE_SRC_COMMANDS_PROP_VALUES_H_
