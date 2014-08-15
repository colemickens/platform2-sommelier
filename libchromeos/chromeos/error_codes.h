// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBCHROMEOS_CHROMEOS_ERROR_CODES_H_
#define LIBCHROMEOS_CHROMEOS_ERROR_CODES_H_

namespace chromeos {
namespace errors {

namespace json {
extern const char kDomain[];
extern const char kParseError[];
extern const char kObjectExpected[];
}  // namespace json

namespace file_system {
extern const char kDomain[];
extern const char kFileReadError[];
}  // namespace file_system

}  // namespace errors
}  // namespace chromeos

#endif  // LIBCHROMEOS_CHROMEOS_ERROR_CODES_H_
