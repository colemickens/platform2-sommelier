// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "settingsd/test_helpers.h"

namespace settingsd {

std::unique_ptr<base::Value> MakeIntValue(int i) {
  return std::unique_ptr<base::Value>(new base::FundamentalValue(i));
}

std::unique_ptr<base::Value> MakeNullValue() {
  return std::unique_ptr<base::Value>(base::Value::CreateNullValue().release());
}

std::unique_ptr<base::Value> MakeStringValue(const std::string& str) {
  return std::unique_ptr<base::Value>(new base::StringValue(str));
}


}  // namespace settingsd
