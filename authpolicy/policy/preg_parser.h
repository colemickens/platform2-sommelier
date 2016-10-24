// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file provides a parser for PReg files which are used for storing group
// policy settings in the file system. The file format is documented here:
//
// http://msdn.microsoft.com/en-us/library/windows/desktop/aa374407(v=vs.85).aspx
//
// Code was copied from Chromium and slightly modified. Will be refactored to
// simplify maintainance, see crbug.com/659645.

#ifndef AUTHPOLICY_POLICY_PREG_PARSER_H_
#define AUTHPOLICY_POLICY_PREG_PARSER_H_

#include "base/strings/string16.h"
#include "brillo/errors/error.h"

namespace base {
class FilePath;
}

namespace policy {

class RegistryDict;

namespace preg_parser {

// Reads the PReg file at |file_path| and writes the registry data to |dict|.
// |root| specifies the registry subtree the caller is interested in,
// everything else gets ignored.
bool ReadFile(const base::FilePath& file_path,
              const base::string16& root,
              RegistryDict* dict,
              brillo::ErrorPtr* error);

}  // namespace preg_parser
}  // namespace policy

#endif  // AUTHPOLICY_POLICY_PREG_PARSER_H_
