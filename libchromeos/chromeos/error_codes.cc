// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/error_codes.h"

namespace chromeos {
namespace errors {

namespace json {
const char kDomain[] = "json_parser";
const char kParseError[] = "json_parse_error";
const char kObjectExpected[] = "json_object_expected";
}  // namespace json

namespace file_system {
const char kDomain[] = "file_system";
const char kFileReadError[] = "file_read_error";
}  // namespace file_system

}  // namespace errors
}  // namespace chromeos
