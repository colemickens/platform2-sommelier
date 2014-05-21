// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BUFFET_COMMANDS_PROP_TYPES_H_
#define BUFFET_COMMANDS_PROP_TYPES_H_

#include <limits>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "buffet/commands/prop_constraints.h"
#include "buffet/commands/prop_values.h"
#include "buffet/error.h"

namespace buffet {

// Helper function to read in a C++ type from JSON value.
template<typename T>
bool TypedValueFromJson(const base::Value* value_in, T* value_out,
                        ErrorPtr* error);

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

  // Makes a full copy of this type definition.
  virtual std::shared_ptr<PropType> Clone() const = 0;
  // Creates an instance of associated value object, using the parameter
  // type as a factory class.
  virtual std::shared_ptr<PropValue> CreateValue() const = 0;

  // Saves the parameter type definition as a JSON object.
  // If |full_schema| is set to true, the full type definition is saved,
  // otherwise only the overridden properties and attributes from the base
  // schema is saved. That is, inherited properties and attributes are not
  // saved.
  // If it fails, returns "nullptr" and fills in the |error| with additional
  // error information.
  virtual std::unique_ptr<base::Value> ToJson(bool full_schema,
                                              ErrorPtr* error) const;
  // Parses an JSON parameter type definition. Optional |schema| may specify
  // the base schema type definition this type should be based upon.
  // If not specified (nullptr), the parameter type is assumed to be a full
  // definition and any omitted required properties are treated as an error.
  // Returns true on success, otherwise fills in the |error| with additional
  // error information.
  virtual bool FromJson(const base::DictionaryValue* value,
                        const PropType* schema, ErrorPtr* error);

  // Validates a JSON value for the parameter type to make sure it satisfies
  // the parameter type definition including any specified constraints.
  // Returns false if the |value| does not meet the requirements of the type
  // definition and returns additional information about the failure via
  // the |error| parameter.
  virtual bool ValidateValue(const base::Value* value,
                             ErrorPtr* error) const = 0;

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

 protected:
  // Validates the given value against all the constraints.
  // This is a helper method used by PropType::ValidateValue().
  bool ValidateConstraints(const Any& value, ErrorPtr* error) const;

  // Helper method to obtaining a vector of OneOf constraint values.
  template<typename T>
  std::vector<T> GetOneOfValuesHelper() const {
    auto ofc = static_cast<const ConstraintOneOf<T>*>(
        GetConstraint(ConstraintType::OneOf));
    return ofc ? ofc->set_.value : std::vector<T>();
  }
  // Helper methods to validating parameter values for various types.
  template<typename T>
  bool ValidateValueHelper(const base::Value* value, ErrorPtr* error) const {
    T val;
    return TypedValueFromJson(value, &val, error) &&
           ValidateConstraints(val, error);
  }

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

// Helper base class for Int and Double parameter types.
class NumericPropTypeBase : public PropType {
 protected:
  // Helper method for implementing AddMinMaxConstraint in derived classes.
  template<typename T>
  void AddMinMaxConstraintHelper(T min_value, T max_value) {
    InheritableAttribute<T> min_attr(min_value, false);
    InheritableAttribute<T> max_attr(max_value, false);
    AddConstraint(std::make_shared<ConstraintMin<T>>(min_attr));
    AddConstraint(std::make_shared<ConstraintMax<T>>(max_attr));
  }

  // Helper method for implementing GetMinValue in derived classes.
  template<typename T>
  T GetMinValueHelper() const {
    auto mmc = static_cast<const ConstraintMin<T>*>(
        GetConstraint(ConstraintType::Min));
    return mmc ? mmc->limit_.value : std::numeric_limits<T>::lowest();
  }

  // Helper method for implementing GetMaxValue in derived classes.
  template<typename T>
  T GetMaxValueHelper() const {
    auto mmc = static_cast<const ConstraintMax<T>*>(
        GetConstraint(ConstraintType::Max));
    return mmc ? mmc->limit_.value : (std::numeric_limits<T>::max)();
  }

  // Helper method for implementing FromJson in derived classes.
  template<typename T>
  bool FromJsonHelper(const base::DictionaryValue* value,
                      const PropType* schema, ErrorPtr* error);
};

// Property definition of Integer type.
class IntPropType : public NumericPropTypeBase {
 public:
  // Overrides from the PropType base class.
  virtual ValueType GetType() const override { return ValueType::Int; }

  virtual IntPropType* GetInt() override { return this; }
  virtual IntPropType const* GetInt() const override { return this; }

  virtual std::shared_ptr<PropType> Clone() const override;
  virtual std::shared_ptr<PropValue> CreateValue() const override;

  virtual bool FromJson(const base::DictionaryValue* value,
                        const PropType* schema, ErrorPtr* error) override {
    return FromJsonHelper<int>(value, schema, error);
  }

  virtual bool ValidateValue(const base::Value* value,
                             ErrorPtr* error) const override {
    return ValidateValueHelper<int>(value, error);
  }

  // Helper methods to add and inspect simple constraints.
  // Used mostly for unit testing.
  void AddMinMaxConstraint(int min_value, int max_value) {
    AddMinMaxConstraintHelper(min_value, max_value);
  }
  int GetMinValue() const { return GetMinValueHelper<int>(); }
  int GetMaxValue() const { return GetMaxValueHelper<int>(); }
  std::vector<int> GetOneOfValues() const {
    return GetOneOfValuesHelper<int>();
  }
};

// Property definition of Number type.
class DoublePropType : public NumericPropTypeBase {
 public:
  // Overrides from the PropType base class.
  virtual ValueType GetType() const override { return ValueType::Double; }

  virtual DoublePropType* GetDouble() override { return this; }
  virtual DoublePropType const* GetDouble() const override { return this; }

  virtual std::shared_ptr<PropType> Clone() const override;
  virtual std::shared_ptr<PropValue> CreateValue() const override;

  virtual bool FromJson(const base::DictionaryValue* value,
                        const PropType* schema, ErrorPtr* error) override {
    return FromJsonHelper<double>(value, schema, error);
  }

  virtual bool ValidateValue(const base::Value* value,
                             ErrorPtr* error) const override {
    return ValidateValueHelper<double>(value, error);
  }

  // Helper methods to add and inspect simple constraints.
  // Used mostly for unit testing.
  void AddMinMaxConstraint(double min_value, double max_value) {
    AddMinMaxConstraintHelper(min_value, max_value);
  }
  double GetMinValue() const { return GetMinValueHelper<double>(); }
  double GetMaxValue() const { return GetMaxValueHelper<double>(); }
  std::vector<double> GetOneOfValues() const {
    return GetOneOfValuesHelper<double>();
  }
};

// Property definition of String type.
class StringPropType : public PropType {
 public:
  // Overrides from the PropType base class.
  virtual ValueType GetType() const override { return ValueType::String; }

  virtual StringPropType* GetString() override { return this; }
  virtual StringPropType const* GetString() const override { return this; }

  virtual std::shared_ptr<PropType> Clone() const override;
  virtual std::shared_ptr<PropValue> CreateValue() const override;

  virtual bool FromJson(const base::DictionaryValue* value,
                        const PropType* schema, ErrorPtr* error) override;

  virtual bool ValidateValue(const base::Value* value,
                             ErrorPtr* error) const override {
    return ValidateValueHelper<std::string>(value, error);
  }

  // Helper methods to add and inspect simple constraints.
  // Used mostly for unit testing.
  void AddLengthConstraint(int min_len, int max_len);
  int GetMinLength() const;
  int GetMaxLength() const;
  std::vector<std::string> GetOneOfValues() const {
    return GetOneOfValuesHelper<std::string>();
  }
};

// Property definition of Boolean type.
class BooleanPropType : public PropType {
 public:
  // Overrides from the PropType base class.
  virtual ValueType GetType() const override { return ValueType::Boolean; }

  virtual BooleanPropType* GetBoolean() override { return this; }
  virtual BooleanPropType const* GetBoolean() const override { return this; }

  virtual std::shared_ptr<PropType> Clone() const override;
  virtual std::shared_ptr<PropValue> CreateValue() const override;

  virtual bool FromJson(const base::DictionaryValue* value,
                        const PropType* schema, ErrorPtr* error) override;

  virtual bool ValidateValue(const base::Value* value,
                             ErrorPtr* error) const override {
    return ValidateValueHelper<bool>(value, error);
  }

  // Helper methods to add and inspect simple constraints.
  // Used mostly for unit testing.
  std::vector<bool> GetOneOfValues() const {
    return GetOneOfValuesHelper<bool>();
  }
};

}  // namespace buffet

#endif  // BUFFET_COMMANDS_PROP_TYPES_H_
