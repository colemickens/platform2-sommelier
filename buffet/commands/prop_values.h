// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BUFFET_COMMANDS_PROP_VALUES_H_
#define BUFFET_COMMANDS_PROP_VALUES_H_

#include <map>
#include <memory>
#include <string>

#include "buffet/commands/prop_constraints.h"
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
  Boolean
};

// Helper methods to get the parameter type enum value for the given
// native C++ data representation.
// The generic GetValueType<T>() is undefined, however particular
// type specializations return the appropriate ValueType.
template<typename T> ValueType GetValueType();  // Undefined.
template<> ValueType GetValueType<int>();
template<> ValueType GetValueType<double>();
template<> ValueType GetValueType<std::string>();
template<> ValueType GetValueType<bool>();

class ObjectSchema;

class PropValue;
class IntPropType;
class DoublePropType;
class StringPropType;
class BooleanPropType;

class IntValue;
class DoubleValue;
class StringValue;
class BooleanValue;

// The base class for parameter values.
// Concrete value classes of various types will be derived from this base.
class PropValue {
 public:
  PropValue() = default;
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

  // Makes a full copy of this value class.
  virtual std::shared_ptr<PropValue> Clone() const = 0;

  // Saves the value as a JSON object.
  // If it fails, returns nullptr value and fills in the details for the
  // failure in the |error| parameter.
  virtual std::unique_ptr<base::Value> ToJson(ErrorPtr* error) const = 0;
  // Parses a value from JSON.
  // If it fails, it returns false and provides additional information
  // via the |error| parameter.
  virtual bool FromJson(const base::Value* value, ErrorPtr* error) = 0;
};

// A helper template base class for implementing simple (non-Object) value
// classes.
template<typename Derived, typename T>
class TypedValueBase : public PropValue {
 public:
  // Overrides from PropValue base class.
  virtual ValueType GetType() const override { return GetValueType<T>(); }
  virtual std::shared_ptr<PropValue> Clone() const override {
    return std::make_shared<Derived>(*static_cast<const Derived*>(this));
  }

  virtual std::unique_ptr<base::Value> ToJson(ErrorPtr* error) const override {
    (void)error;  // unused.
    return TypedValueToJson(value_);
  }

  virtual bool FromJson(const base::Value* value, ErrorPtr* error) override {
    return TypedValueFromJson(value, &value_, error);
  }

  // Helper method to get and set the C++ representation of the value.
  const T& GetValue() const { return value_; }
  void SetValue(T value) { value_ = std::move(value); }

 protected:
  T value_{};  // The value of the parameter in C++ data representation.
};

// Value of type Integer.
class IntValue final : public TypedValueBase<IntValue, int> {
 public:
  virtual IntValue* GetInt() override { return this; }
  virtual IntValue const* GetInt() const override { return this; }
};

// Value of type Number.
class DoubleValue final : public TypedValueBase<DoubleValue, double> {
 public:
  virtual DoubleValue* GetDouble() override { return this; }
  virtual DoubleValue const* GetDouble() const override { return this; }
};

// Value of type String.
class StringValue final : public TypedValueBase<StringValue, std::string> {
 public:
  virtual StringValue* GetString() override { return this; }
  virtual StringValue const* GetString() const override { return this; }
};

// Value of type Boolean.
class BooleanValue final : public TypedValueBase<BooleanValue, bool> {
 public:
  virtual BooleanValue* GetBoolean() override { return this; }
  virtual BooleanValue const* GetBoolean() const override { return this; }
};

}  // namespace buffet

#endif  // BUFFET_COMMANDS_PROP_VALUES_H_
