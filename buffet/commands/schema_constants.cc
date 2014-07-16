// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "buffet/commands/schema_constants.h"

namespace buffet {
namespace commands {

namespace errors {
const char kDomain[] = "command_schema";

const char kOutOfRange[] = "out_of_range";
const char kTypeMismatch[] = "type_mismatch";
const char kPropTypeChanged[] = "param_type_changed";
const char kUnknownType[] = "unknown_type";
const char kInvalidPropDef[] = "invalid_parameter_definition";
const char kInvalidPropValue[] = "invalid_parameter_value";
const char kNoTypeInfo[] = "no_type_info";
const char kPropertyMissing[] = "parameter_missing";
const char kUnknownProperty[] = "unexpected_parameter";
const char kInvalidObjectSchema[] = "invalid_object_schema";
const char kDuplicateCommandDef[] = "duplicate_command_definition";
}  // namespace errors

namespace attributes {
const char kType[] = "type";
const char kDisplayName[] = "displayName";

const char kNumeric_Min[] = "minimum";
const char kNumeric_Max[] = "maximum";

const char kString_MinLength[] = "minLength";
const char kString_MaxLength[] = "maxLength";

const char kOneOf_Enum[] = "enum";
const char kOneOf_Metadata[] = "metadata";
const char kOneOf_MetaSchema[] = "schema";

const char kObject_Properties[] = "properties";

const char kCommand_Parameters[] = "parameters";
}  // namespace attributes

}  // namespace commands
}  // namespace buffet
