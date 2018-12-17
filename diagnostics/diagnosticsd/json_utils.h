// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DIAGNOSTICS_DIAGNOSTICSD_JSON_UTILS_H_
#define DIAGNOSTICS_DIAGNOSTICSD_JSON_UTILS_H_

#include <string>

#include <base/strings/string_piece.h>

namespace diagnostics {

// Validates |json| and copies error message to |json_error_message| if |json|
// is not valid.
//
// |json_error_message| must be non-NULL.
//
// Returns true if |json| is valid, otherwise false.
bool IsJsonValid(base::StringPiece json, std::string* json_error_message);

}  // namespace diagnostics

#endif  // DIAGNOSTICS_DIAGNOSTICSD_JSON_UTILS_H_
