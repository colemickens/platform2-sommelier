// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BUFFET_UTILS_H_
#define BUFFET_UTILS_H_

#include <memory>

#include <base/values.h>
#include <base/files/file_path.h>
#include <chromeos/errors/error.h>

namespace buffet {

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
std::unique_ptr<const base::DictionaryValue> LoadJsonDict(
    const base::FilePath& json_file_path, chromeos::ErrorPtr* error);

}  // namespace buffet

#endif  // BUFFET_UTILS_H_
