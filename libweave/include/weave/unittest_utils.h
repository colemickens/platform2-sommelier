// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBWEAVE_INCLUDE_WEAVE_UNITTEST_UTILS_H_
#define LIBWEAVE_INCLUDE_WEAVE_UNITTEST_UTILS_H_

#include <memory>
#include <string>

#include <base/values.h>
#include <gtest/gtest.h>

namespace weave {
namespace unittests {

// Helper method to create base::Value from a string as a smart pointer.
// For ease of definition in C++ code, double-quotes in the source definition
// are replaced with apostrophes.
std::unique_ptr<base::Value> CreateValue(const std::string& json);

// Helper method to create a JSON dictionary object from a string.
std::unique_ptr<base::DictionaryValue> CreateDictionaryValue(
    const std::string& json);

inline bool IsEqualValue(const base::Value& val1, const base::Value& val2) {
  return val1.Equals(&val2);
}

}  // namespace unittests
}  // namespace weave

#define EXPECT_JSON_EQ(expected, actual)       \
  EXPECT_PRED2(weave::unittests::IsEqualValue, \
               *weave::unittests::CreateValue(expected), actual)

#endif  // LIBWEAVE_INCLUDE_WEAVE_UNITTEST_UTILS_H_
