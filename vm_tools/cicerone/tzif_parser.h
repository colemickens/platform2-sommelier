// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VM_TOOLS_CICERONE_TZIF_PARSER_H_
#define VM_TOOLS_CICERONE_TZIF_PARSER_H_

#include <string>

#include <base/files/file_path.h>

// Parses TZif format timezone files. See 'man tzfile' for more info on the
// format.
class TzifParser {
 public:
  static bool GetPosixTimezone(const base::FilePath& tzif_path,
                               std::string* timezone_string_out);
};

#endif  // VM_TOOLS_CICERONE_TZIF_PARSER_H_
