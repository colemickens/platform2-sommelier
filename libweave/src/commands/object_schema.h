// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBWEAVE_SRC_COMMANDS_OBJECT_SCHEMA_H_
#define LIBWEAVE_SRC_COMMANDS_OBJECT_SCHEMA_H_

#include <map>
#include <memory>
#include <string>

#include <chromeos/errors/error.h>

namespace base {
class Value;
class DictionaryValue;
}  // namespace base

namespace buffet {

class PropType;

// ObjectSchema is a class representing an object definition in GCD command
// schema. This could represent a GCD command definition, but also it can be
// used when defining custom object types for command properties such as
// output media type (paper) for print command. The schema definition for
// these type of object description is the same.
class ObjectSchema final {
 public:
  // Do not inline the constructor/destructor to allow forward-declared type
  // PropType to be part of |properties_| member.
  ObjectSchema();
  ~ObjectSchema();

  // Properties is a string-to-PropType map representing a list of
  // properties defined for a command/object. The key is the parameter
  // name and the value is the parameter type definition object.
  using Properties = std::map<std::string, std::unique_ptr<PropType>>;

  // Makes a full copy of this object.
  virtual std::unique_ptr<ObjectSchema> Clone() const;

  // Add a new parameter definition.
  void AddProp(const std::string& name, std::unique_ptr<PropType> prop);

  // Finds parameter type definition by name. Returns nullptr if not found.
  const PropType* GetProp(const std::string& name) const;

  // Gets the list of all the properties defined.
  const Properties& GetProps() const { return properties_; }

  // Marks the property with given name as "required". If |name| specifies
  // an unknown property, false is returned and |error| is set with detailed
  // error message for the failure.
  bool MarkPropRequired(const std::string& name, chromeos::ErrorPtr* error);

  // Specify whether extra properties are allowed on objects described by
  // this schema. When validating a value of an object type, we can
  // make sure that the value has only the properties explicitly defined by
  // the schema and no other (custom) properties are allowed.
  // This is to support JSON Schema's "additionalProperties" specification.
  bool GetExtraPropertiesAllowed() const { return extra_properties_allowed_; }
  void SetExtraPropertiesAllowed(bool allowed) {
    extra_properties_allowed_ = allowed;
  }

  // Saves the object schema to JSON. When |full_schema| is set to true,
  // then all properties and constraints are saved, otherwise, only
  // the overridden (not inherited) ones are saved.
  std::unique_ptr<base::DictionaryValue> ToJson(
      bool full_schema,
      bool in_command_def,
      chromeos::ErrorPtr* error) const;

  // Loads the object schema from JSON. If |object_schema| is not nullptr, it is
  // used as a base schema to inherit omitted properties and constraints from.
  bool FromJson(const base::DictionaryValue* value,
                const ObjectSchema* object_schema,
                chromeos::ErrorPtr* error);

  // Helper factory method to create a new instance of ObjectSchema object.
  static std::unique_ptr<ObjectSchema> Create();

  // Helper method to load property type definitions from JSON.
  static std::unique_ptr<PropType> PropFromJson(const base::Value& value,
                                                const PropType* base_schema,
                                                chromeos::ErrorPtr* error);

 private:
  // Internal parameter type definition map.
  Properties properties_;
  bool extra_properties_allowed_{false};
};

}  // namespace buffet

#endif  // LIBWEAVE_SRC_COMMANDS_OBJECT_SCHEMA_H_
