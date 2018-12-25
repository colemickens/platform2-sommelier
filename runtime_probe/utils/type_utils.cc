// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdlib>
#include <string>

#include <base/strings/string_number_conversions.h>
#include <base/strings/string_util.h>

#include "runtime_probe/utils/type_utils.h"

namespace runtime_probe {

bool StringToDouble(const std::string& input, double* output) {
  std::string trimmed_input;
  TrimWhitespaceASCII(input, base::TrimPositions::TRIM_ALL, &trimmed_input);

  if (trimmed_input.empty()) {  // empty string is never a double.
    return false;
  }
  char* endptr;
  *output = strtod(trimmed_input.c_str(), &endptr);
  // Assume the conversion success iff the entire string is consumed.
  return *endptr == '\0';
}

bool StringToInt(const std::string& input, int* output) {
  std::string trimmed_input;
  TrimWhitespaceASCII(input, base::TrimPositions::TRIM_ALL, &trimmed_input);
  return base::StringToInt(trimmed_input, output);
}

bool HexStringToInt(const std::string& input, int* output) {
  std::string trimmed_input;
  TrimWhitespaceASCII(input, base::TrimPositions::TRIM_ALL, &trimmed_input);
  return base::HexStringToInt(trimmed_input, output);
}

}  // namespace runtime_probe
