// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBWEAVE_SRC_UTILS_H_
#define LIBWEAVE_SRC_UTILS_H_

#include <memory>
#include <string>

#include <base/values.h>
#include <base/files/file_path.h>
#include <chromeos/errors/error.h>

namespace weave {

// Buffet-wide errors.
// TODO(avakulenko): This should be consolidated into errors::<domain> namespace
// See crbug.com/417274
extern const char kErrorDomainBuffet[];
extern const char kFileReadError[];
extern const char kInvalidCategoryError[];
extern const char kInvalidPackageError[];

// kDefaultCategory represents a default state property category for standard
// properties from "base" package which are provided by buffet and not any of
// the daemons running on the device.
const char kDefaultCategory[] = "";

// Helper function to load a JSON file that is expected to be
// an object/dictionary. In case of error, returns empty unique ptr and fills
// in error details in |error|.
std::unique_ptr<base::DictionaryValue> LoadJsonDict(
    const base::FilePath& json_file_path,
    chromeos::ErrorPtr* error);

// Helper function to load a JSON dictionary from a string.
std::unique_ptr<base::DictionaryValue> LoadJsonDict(
    const std::string& json_string,
    chromeos::ErrorPtr* error);

// Synchronously resolves the |host| and connects a socket to the resolved
// address/port.
// Returns the connected socket file descriptor or -1 if failed.
int ConnectSocket(const std::string& host, uint16_t port);

}  // namespace weave

#endif  // LIBWEAVE_SRC_UTILS_H_
