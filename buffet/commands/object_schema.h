// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BUFFET_COMMANDS_OBJECT_SCHEMA_H_
#define BUFFET_COMMANDS_OBJECT_SCHEMA_H_

#include <map>
#include <memory>
#include <string>

#include <base/basictypes.h>

#include "buffet/error.h"

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
  // Properties is a string-to-PropType map representing a list of
  // properties defined for a command/object. The key is the parameter
  // name and the value is the parameter type definition object.
  using Properties = std::map<std::string, std::shared_ptr<PropType>>;

  ObjectSchema() = default;

  // Add a new parameter definition.
  void AddProp(const std::string& name, std::shared_ptr<PropType> prop);
  // Finds parameter type definition by name. Returns nullptr if not found.
  const PropType* GetProp(const std::string& name) const;
  // Gets the list of all the properties defined.
  const Properties& GetProps() const { return properties_; }

  // Saves the object schema to JSON. When |full_schema| is set to true,
  // then all properties and constraints are saved, otherwise, only
  // the overridden (not inherited) ones are saved.
  std::unique_ptr<base::DictionaryValue> ToJson(bool full_schema,
                                                ErrorPtr* error) const;
  // Loads the object schema from JSON. If |schema| is not nullptr, it is
  // used as a base schema to inherit omitted properties and constraints from.
  bool FromJson(const base::DictionaryValue* value, const ObjectSchema* schema,
                ErrorPtr* error);

 private:
  // Internal helper method to load individual parameter type definitions.
  bool PropFromJson(const std::string& prop_name,
                    const base::Value& value,
                    const PropType* schemaProp,
                    Properties* properties, ErrorPtr* error) const;
  // Helper function in case the parameter is defined as JSON string like this:
  //   "prop":"..."
  bool PropFromJsonString(const std::string& prop_name,
                          const base::Value& value,
                          const PropType* schemaProp,
                          Properties* properties, ErrorPtr* error) const;
  // Helper function in case the parameter is defined as JSON array like this:
  //   "prop":[...]
  bool PropFromJsonArray(const std::string& prop_name,
                         const base::Value& value,
                         const PropType* schemaProp,
                         Properties* properties, ErrorPtr* error) const;
  // Helper function in case the parameter is defined as JSON object like this:
  //   "prop":{...}
  bool PropFromJsonObject(const std::string& prop_name,
                          const base::Value& value,
                          const PropType* schemaProp,
                          Properties* properties, ErrorPtr* error) const;

  // Internal parameter type definition map.
  Properties properties_;
  DISALLOW_COPY_AND_ASSIGN(ObjectSchema);
};

}  // namespace buffet

#endif  // BUFFET_COMMANDS_OBJECT_SCHEMA_H_
