// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RUNTIME_PROBE_UTILS_FILE_UTILS_H_
#define RUNTIME_PROBE_UTILS_FILE_UTILS_H_

#include <string>
#include <vector>

#include <base/files/file_path.h>
#include <base/values.h>

namespace runtime_probe {
// Maps files listed in |keys| and |optional_keys| under |dir_path| into key
// value pairs.
//
// |keys| represents the set of must have, if any |keys| is missed in the
// |dir_path|, an empty dictionary will be returned.
base::DictionaryValue MapFilesToDict(
    const base::FilePath& dir_path,
    const std::vector<std::string> keys,
    const std::vector<std::string> optional_keys);

}  // namespace runtime_probe

#endif  // RUNTIME_PROBE_UTILS_FILE_UTILS_H_
