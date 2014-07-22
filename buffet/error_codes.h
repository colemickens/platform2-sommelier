// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BUFFET_ERROR_CODES_H_
#define BUFFET_ERROR_CODES_H_

namespace buffet {
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
}  // namespace buffet

#endif  // BUFFET_ERROR_CODES_H_
