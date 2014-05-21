// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BUFFET_COMMANDS_SCHEMA_CONSTANTS_H_
#define BUFFET_COMMANDS_SCHEMA_CONSTANTS_H_

namespace buffet {
namespace commands {

namespace errors {
// Error domain for command schema description.
extern const char kDomain[];

// Common command definition error codes.
extern const char kOutOfRange[];
extern const char kTypeMismatch[];
extern const char kPropTypeChanged[];
extern const char kUnknownType[];
extern const char kInvalidPropDef[];
extern const char kNoTypeInfo[];
extern const char kPropertyMissing[];
}  // namespace errors

namespace attributes {
// Command description JSON schema attributes.
extern const char kType[];
extern const char kDisplayName[];

extern const char kNumeric_Min[];
extern const char kNumeric_Max[];

extern const char kString_MinLength[];
extern const char kString_MaxLength[];

extern const char kOneOf_Enum[];
}  // namespace attributes

}  // namespace commands
}  // namespace buffet

#endif  // BUFFET_COMMANDS_SCHEMA_CONSTANTS_H_
