// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEBUGD_SRC_VERIFY_RO_UTILS_H_
#define DEBUGD_SRC_VERIFY_RO_UTILS_H_

#include <string>
#include <vector>

namespace debugd {

// Gets the values of |keys| from |output|. Returns lines of "key=value" pairs,
// one line per pair (a newline character is included at the end of the pair),
// or an error message if not all |keys| are found.
std::string GetKeysValuesFromProcessOutput(
    const std::string& output, const std::vector<std::string>& keys);

}  // namespace debugd

#endif  // DEBUGD_SRC_VERIFY_RO_UTILS_H_
