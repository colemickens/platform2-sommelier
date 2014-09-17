// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBCHROMEOS_CHROMEOS_ERRORS_ERROR_CODES_H_
#define LIBCHROMEOS_CHROMEOS_ERRORS_ERROR_CODES_H_

#include <string>

#include <chromeos/chromeos_export.h>
#include <chromeos/errors/error.h>

namespace chromeos {
namespace errors {

namespace dbus {
CHROMEOS_EXPORT extern const char kDomain[];
}  // namespace dbus

namespace json {
CHROMEOS_EXPORT extern const char kDomain[];
CHROMEOS_EXPORT extern const char kParseError[];
CHROMEOS_EXPORT extern const char kObjectExpected[];
}  // namespace json

namespace system {
CHROMEOS_EXPORT extern const char kDomain[];

// Adds an Error object to the error chain identified by |error|, using
// the system error code (see "errno").
CHROMEOS_EXPORT void AddSystemError(ErrorPtr* error, int errnum);
}  // namespace system

}  // namespace errors
}  // namespace chromeos

#endif  // LIBCHROMEOS_CHROMEOS_ERRORS_ERROR_CODES_H_
