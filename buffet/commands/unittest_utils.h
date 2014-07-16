// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BUFFET_COMMANDS_UNITTEST_UTILS_H_
#define BUFFET_COMMANDS_UNITTEST_UTILS_H_

#include <memory>
#include <string>

#include <base/values.h>

namespace buffet {
namespace unittests {

// Helper method to create base::Value from a string as a smart pointer.
// For ease of definition in C++ code, double-quotes in the source definition
// are replaced with apostrophes.
std::unique_ptr<base::Value> CreateValue(const char* json);

// Helper method to create a JSON dictionary object from a string.
std::unique_ptr<base::DictionaryValue> CreateDictionaryValue(const char* json);

// Converts a JSON value to a string. It also converts double-quotes to
// apostrophes for easy comparisons in C++ source code.
std::string ValueToString(const base::Value* value);

}  // namespace unittests
}  // namespace buffet

#endif  // BUFFET_COMMANDS_UNITTEST_UTILS_H_
