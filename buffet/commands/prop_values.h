// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BUFFET_COMMANDS_PROP_VALUES_H_
#define BUFFET_COMMANDS_PROP_VALUES_H_

#include <map>
#include <memory>
#include <string>

#include "buffet/any.h"
#include "buffet/commands/schema_utils.h"
#include "buffet/error.h"

namespace base {
class Value;
class DictionaryValue;
}  // namespace base

namespace buffet {

// Enumeration to indicate supported command parameter types.
enum class ValueType {
  Int,
  Double,
  String,
  Boolean,
  Object
};

class PropValue;
class IntValue;
class DoubleValue;
class StringValue;
class BooleanValue;
class ObjectValue;

class PropType;

// Helper methods to get the parameter type enum value for the given
// native C++ data representation.
// The generic GetValueType<T>() is undefined, however particular
// type specializations return the appropriate ValueType.
template<typename T> ValueType GetValueType();  // Undefined.
template<>
inline ValueType GetValueType<int>() { return ValueType::Int; }
template<>
inline ValueType GetValueType<double>() { return ValueType::Double; }
template<>
inline ValueType GetValueType<std::string>() { return ValueType::String; }
template<>
inline ValueType GetValueType<bool>() { return ValueType::Boolean; }
template<>
inline ValueType GetValueType<native_types::Object>() {
  return ValueType::Object;
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
  explicit PropValue(const PropType* type)
      : type_(type) {}
  virtual ~PropValue() = default;

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

  // Makes a full copy of this value class.
  virtual std::shared_ptr<PropValue> Clone() const = 0;

  // Saves the value as a JSON object.
  // If it fails, returns nullptr value and fills in the details for the
  // failure in the |error| parameter.
  virtual std::unique_ptr<base::Value> ToJson(ErrorPtr* error) const = 0;
  // Parses a value from JSON.
  // If it fails, it returns false and provides additional information
  // via the |error| parameter.
  virtual bool FromJson(const base::Value* value,
                        ErrorPtr* error) = 0;

  // Returns the contained C++ value as Any.
  virtual Any GetValueAsAny() const = 0;

  // Return the type definition of this value.
  const PropType* GetPropType() const { return type_; }
  // Compares two values and returns true if they are equal.
  virtual bool IsEqual(const PropValue* value) const = 0;

 protected:
  const PropType* type_;  // weak pointer
};

// A helper template base class for implementing simple (non-Object) value
// classes.
template<typename Derived, typename T>
class TypedValueBase : public PropValue {
 public:
  // To help refer to this base class from derived classes, define _Base to
  // be this class.
  using _Base = TypedValueBase<Derived, T>;
  // Expose the non-default constructor of the base class.
  using PropValue::PropValue;

  // Overrides from PropValue base class.
  virtual ValueType GetType() const override { return GetValueType<T>(); }
  virtual std::shared_ptr<PropValue> Clone() const override {
    return std::make_shared<Derived>(*static_cast<const Derived*>(this));
  }

  virtual std::unique_ptr<base::Value> ToJson(ErrorPtr* error) const override {
    return TypedValueToJson(value_, error);
  }

  virtual bool FromJson(const base::Value* value,
                        ErrorPtr* error) override {
    return TypedValueFromJson(value, GetPropType()->GetObjectSchemaPtr(),
                              &value_, error);
  }

  virtual bool IsEqual(const PropValue* value) const override {
    if (GetType() != value->GetType())
      return false;
    const _Base* value_base = static_cast<const _Base*>(value);
    return CompareValue(GetValue(), value_base->GetValue());
  }

  // Helper methods to get and set the C++ representation of the value.
  virtual Any GetValueAsAny() const override { return value_; }
  const T& GetValue() const { return value_; }
  void SetValue(T value) { value_ = std::move(value); }

 protected:
  T value_{};  // The value of the parameter in C++ data representation.
};

// Value of type Integer.
class IntValue final : public TypedValueBase<IntValue, int> {
 public:
  using _Base::_Base;  // Expose the custom constructor of the base class.
  virtual IntValue* GetInt() override { return this; }
  virtual IntValue const* GetInt() const override { return this; }
};

// Value of type Number.
class DoubleValue final : public TypedValueBase<DoubleValue, double> {
 public:
  using _Base::_Base;  // Expose the custom constructor of the base class.
  virtual DoubleValue* GetDouble() override { return this; }
  virtual DoubleValue const* GetDouble() const override { return this; }
};

// Value of type String.
class StringValue final : public TypedValueBase<StringValue, std::string> {
 public:
  using _Base::_Base;  // Expose the custom constructor of the base class.
  virtual StringValue* GetString() override { return this; }
  virtual StringValue const* GetString() const override { return this; }
};

// Value of type Boolean.
class BooleanValue final : public TypedValueBase<BooleanValue, bool> {
 public:
  using _Base::_Base;  // Expose the custom constructor of the base class.
  virtual BooleanValue* GetBoolean() override { return this; }
  virtual BooleanValue const* GetBoolean() const override { return this; }
};

// Value of type Object.
class ObjectValue final : public TypedValueBase<ObjectValue,
                                                native_types::Object> {
 public:
  using _Base::_Base;  // Expose the custom constructor of the base class.
  virtual ObjectValue* GetObject() override { return this; }
  virtual ObjectValue const* GetObject() const override { return this; }
};
}  // namespace buffet

#endif  // BUFFET_COMMANDS_PROP_VALUES_H_
