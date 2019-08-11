// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINT_TOOLS_IPP_IN_JSON_H_
#define PRINT_TOOLS_IPP_IN_JSON_H_

#include <string>
#include <vector>

#include <chromeos/libipp/ipp.h>

// This function build JSON representation of the given IPP response along with
// the log from parsing it.
// TODO(pawliczek) - add const to the first param (bug chromium:994893)
bool ConvertToJson(ipp::Response& response,  // NOLINT(runtime/references)
                   const std::vector<ipp::Log>& log,
                   bool compressed_json,
                   std::string* json);

#endif  //  PRINT_TOOLS_IPP_IN_JSON_H_
