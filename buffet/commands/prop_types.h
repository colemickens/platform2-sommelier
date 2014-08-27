// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BUFFET_COMMANDS_PROP_TYPES_H_
#define BUFFET_COMMANDS_PROP_TYPES_H_

#include <limits>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <chromeos/any.h>
#include <chromeos/errors/error.h>

#include "buffet/commands/prop_constraints.h"
#include "buffet/commands/prop_values.h"

namespace buffet {

class IntPropType;
class DoublePropType;
class StringPropType;
class BooleanPropType;
class ObjectPropType;

// PropType is a base class for all parameter type definition objects.
// Property definitions of a particular type will derive from this class and
// provide type-specific implementations.
class PropType {
 public:
  // ConstraintMap is a type alias for a map containing parameter
  // constraints. It is implemented as a map for fast look-ups of constraints
  // of particular type. Also it is expected to have at most one constraint
  // of each type (e.g. it makes no sense to impose two "minimum" constraints
  // onto a numeric parameter).
  using ConstraintMap = std::map<ConstraintType,
                                 std::shared_ptr<Constraint>>;

  PropType();
  virtual ~PropType();

  // Gets the parameter type as an enumeration.
  virtual ValueType GetType() const = 0;
  // Gets the parameter type as a string.
  std::string GetTypeAsString() const;
  // Returns true if this parameter definition inherits a type
  // definition from a base object schema.
  bool IsBasedOnSchema() const { return based_on_schema_; }
  // Returns a default value specified for the type, or nullptr if no default
  // is available.
  const PropValue* GetDefaultValue() const { return default_.value.get(); }
  // Gets the constraints specified for the parameter, if any.
  const ConstraintMap& GetConstraints() const {
    return constraints_;
  }
  // Checks if any of the type attributes were overridden from the base
  // schema definition. If this type does not inherit from a base schema,
  // this method returns true.
  // An attribute could be the value of any of the constraints, default
  // value of a parameter or any other data that may be specified in
  // parameter type definition in and can be inherited from the base schema.
  virtual bool HasOverriddenAttributes() const;

  // Type conversion methods. Used in lieu of RTTI and dynamic_cast<>.
  virtual IntPropType* GetInt() { return nullptr; }
  virtual IntPropType const* GetInt() const { return nullptr; }
  virtual DoublePropType* GetDouble() { return nullptr; }
  virtual DoublePropType const* GetDouble() const { return nullptr; }
  virtual StringPropType* GetString() { return nullptr; }
  virtual StringPropType const* GetString() const { return nullptr; }
  virtual BooleanPropType* GetBoolean() { return nullptr; }
  virtual BooleanPropType const* GetBoolean() const { return nullptr; }
  virtual ObjectPropType* GetObject() { return nullptr; }
  virtual ObjectPropType const* GetObject() const { return nullptr; }

  // Makes a full copy of this type definition.
  virtual std::shared_ptr<PropType> Clone() const = 0;
  // Creates an instance of associated value object, using the parameter
  // type as a factory class.
  virtual std::shared_ptr<PropValue> CreateValue() const = 0;
  virtual std::shared_ptr<PropValue> CreateValue(
      const chromeos::Any& val) const = 0;

  // Saves the parameter type definition as a JSON object.
  // If |full_schema| is set to true, the full type definition is saved,
  // otherwise only the overridden properties and attributes from the base
  // schema is saved. That is, inherited properties and attributes are not
  // saved.
  // If it fails, returns "nullptr" and fills in the |error| with additional
  // error information.
  virtual std::unique_ptr<base::Value> ToJson(bool full_schema,
                                              chromeos::ErrorPtr* error) const;
  // Parses an JSON parameter type definition. Optional |base_schema| may
  // specify the base schema type definition this type should be based upon.
  // If not specified (nullptr), the parameter type is assumed to be a full
  // definition and any omitted required properties are treated as an error.
  // Returns true on success, otherwise fills in the |error| with additional
  // error information.
  virtual bool FromJson(const base::DictionaryValue* value,
                        const PropType* base_schema, chromeos::ErrorPtr* error);
  // Helper function to load object schema from JSON.
  virtual bool ObjectSchemaFromJson(const base::DictionaryValue* value,
                                    const PropType* base_schema,
                                    std::set<std::string>* processed_keys,
                                    chromeos::ErrorPtr* error) {
    return true;
  }
  // Helper function to load type-specific constraints from JSON.
  virtual bool ConstraintsFromJson(const base::DictionaryValue* value,
                                   std::set<std::string>* processed_keys,
                                   chromeos::ErrorPtr* error) {
    return true;
  }

  // Validates a JSON value for the parameter type to make sure it satisfies
  // the parameter type definition including any specified constraints.
  // Returns false if the |value| does not meet the requirements of the type
  // definition and returns additional information about the failure via
  // the |error| parameter.
  bool ValidateValue(const base::Value* value, chromeos::ErrorPtr* error) const;

  // Similar to the above method, but uses Any as the value container.
  bool ValidateValue(const chromeos::Any& value,
                     chromeos::ErrorPtr* error) const;

  // Additional helper static methods to help with converting a type enum
  // value into a string and back.
  using TypeMap = std::vector<std::pair<ValueType, std::string>>;
  // Returns a list of value types and corresponding type names.
  static const TypeMap& GetTypeMap();
  // Gets the type name string for the given type.
  static std::string GetTypeStringFromType(ValueType type);
  // Finds the type for the given type name. Returns true on success.
  static bool GetTypeFromTypeString(const std::string& name, ValueType* type);

  // Creates an instance of PropType-derived class for the specified
  // parameter type.
  static std::unique_ptr<PropType> Create(ValueType type);

  // Adds a constraint to the type definition.
  void AddConstraint(std::shared_ptr<Constraint> constraint);
  // Removes a constraint of given type, if it exists.
  void RemoveConstraint(ConstraintType constraint_type);

  // Finds a constraint of given type. Returns nullptr if not found.
  const Constraint* GetConstraint(ConstraintType constraint_type) const;
  Constraint* GetConstraint(ConstraintType constraint_type);

  // Returns a schema for Object-type parameter. This will be nullptr for
  // every type but Object.
  const ObjectSchema* GetObjectSchemaPtr() const {
    return GetObjectSchema().get();
  }
  virtual std::shared_ptr<const ObjectSchema> GetObjectSchema() const {
    return std::shared_ptr<const ObjectSchema>();
  }

  // Validates the given value against all the constraints.
  bool ValidateConstraints(const PropValue& value,
                           chromeos::ErrorPtr* error) const;

 protected:
  // Specifies if this parameter definition is derived from a base
  // object schema.
  bool based_on_schema_ = false;
  // A list of constraints specified for the parameter.
  ConstraintMap constraints_;
  // The default value specified for the parameter, if any. If the default
  // value is present, the parameter is treated as optional and the default
  // value is used if the parameter value is omitted when sending a command.
  // Otherwise the parameter is treated as required and, if it is omitted,
  // this is treated as an error.
  InheritableAttribute<std::shared_ptr<PropValue>> default_;
};

// Base class for all the derived concrete implementations of property
// type classes. Provides implementations for common methods of PropType base.
template<class Derived, class Value, typename T>
class PropTypeBase : public PropType {
 public:
  ValueType GetType() const override { return GetValueType<T>(); }
  std::shared_ptr<PropType> Clone() const override {
    return std::make_shared<Derived>(*static_cast<const Derived*>(this));
  }
  std::shared_ptr<PropValue> CreateValue() const override {
    return std::make_shared<Value>(this);
  }
  std::shared_ptr<PropValue> CreateValue(
      const chromeos::Any& v) const override {
    auto value = std::make_shared<Value>(this);
    value->SetValue(v.Get<T>());
    return std::move(value);
  }
  bool ConstraintsFromJson(const base::DictionaryValue* value,
                           std::set<std::string>* processed_keys,
                           chromeos::ErrorPtr* error) override;

  // Helper method to obtain a vector of OneOf constraint values.
  std::vector<T> GetOneOfValues() const {
    auto ofc = static_cast<const ConstraintOneOf<T>*>(
        this->GetConstraint(ConstraintType::OneOf));
    return ofc ? ofc->set_.value : std::vector<T>();
  }
};

// Helper base class for Int and Double parameter types.
template<class Derived, class Value, typename T>
class NumericPropTypeBase : public PropTypeBase<Derived, Value, T> {
 public:
  using _Base = PropTypeBase<Derived, Value, T>;
  bool ConstraintsFromJson(const base::DictionaryValue* value,
                           std::set<std::string>* processed_keys,
                           chromeos::ErrorPtr* error) override;

  // Helper method to set and obtain a min/max constraint values.
  // Used mostly for unit testing.
  void AddMinMaxConstraint(T min_value, T max_value) {
    InheritableAttribute<T> min_attr(min_value, false);
    InheritableAttribute<T> max_attr(max_value, false);
    this->AddConstraint(std::make_shared<ConstraintMin<T>>(min_attr));
    this->AddConstraint(std::make_shared<ConstraintMax<T>>(max_attr));
  }
  T GetMinValue() const {
    auto mmc = static_cast<const ConstraintMin<T>*>(
        this->GetConstraint(ConstraintType::Min));
    return mmc ? mmc->limit_.value : std::numeric_limits<T>::lowest();
  }
  T GetMaxValue() const {
    auto mmc = static_cast<const ConstraintMax<T>*>(
        this->GetConstraint(ConstraintType::Max));
    return mmc ? mmc->limit_.value : (std::numeric_limits<T>::max)();
  }
};

// Property definition of Integer type.
class IntPropType : public NumericPropTypeBase<IntPropType, IntValue, int> {
 public:
  // Overrides from the PropType base class.
  IntPropType* GetInt() override { return this; }
  IntPropType const* GetInt() const override { return this; }
};

// Property definition of Number type.
class DoublePropType
    : public NumericPropTypeBase<DoublePropType, DoubleValue, double> {
 public:
  // Overrides from the PropType base class.
  DoublePropType* GetDouble() override { return this; }
  DoublePropType const* GetDouble() const override { return this; }
};

// Property definition of String type.
class StringPropType
    : public PropTypeBase<StringPropType, StringValue, std::string> {
 public:
  using _Base = PropTypeBase<StringPropType, StringValue, std::string>;
  // Overrides from the PropType base class.
  StringPropType* GetString() override { return this; }
  StringPropType const* GetString() const override { return this; }

  bool ConstraintsFromJson(const base::DictionaryValue* value,
                           std::set<std::string>* processed_keys,
                           chromeos::ErrorPtr* error) override;

  // Helper methods to add and inspect simple constraints.
  // Used mostly for unit testing.
  void AddLengthConstraint(int min_len, int max_len);
  int GetMinLength() const;
  int GetMaxLength() const;
};

// Property definition of Boolean type.
class BooleanPropType
    : public PropTypeBase<BooleanPropType, BooleanValue, bool> {
 public:
  // Overrides from the PropType base class.
  BooleanPropType* GetBoolean() override { return this; }
  BooleanPropType const* GetBoolean() const override { return this; }
};

// Parameter definition of Object type.
class ObjectPropType
    : public PropTypeBase<ObjectPropType, ObjectValue, native_types::Object> {
 public:
  using _Base = PropTypeBase<ObjectPropType, ObjectValue, native_types::Object>;
  ObjectPropType();

  // Overrides from the ParamType base class.
  bool HasOverriddenAttributes() const override;

  ObjectPropType* GetObject() override { return this; }
  ObjectPropType const* GetObject() const override { return this; }

  std::unique_ptr<base::Value> ToJson(bool full_schema,
                                      chromeos::ErrorPtr* error) const override;
  bool ObjectSchemaFromJson(const base::DictionaryValue* value,
                            const PropType* base_schema,
                            std::set<std::string>* processed_keys,
                            chromeos::ErrorPtr* error) override;

  std::shared_ptr<const ObjectSchema> GetObjectSchema() const override {
    return object_schema_.value;
  }
  void SetObjectSchema(const std::shared_ptr<const ObjectSchema>& schema) {
    object_schema_.value = schema;
    object_schema_.is_inherited = false;
  }

 private:
  InheritableAttribute<std::shared_ptr<const ObjectSchema>> object_schema_;
};
}  // namespace buffet

#endif  // BUFFET_COMMANDS_PROP_TYPES_H_
