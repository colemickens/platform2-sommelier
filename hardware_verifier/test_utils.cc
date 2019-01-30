/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "hardware_verifier/test_utils.h"

#include <cstdlib>

#include <base/logging.h>

namespace hardware_verifier {

base::FilePath GetTestDataPath() {
  char* src_env = std::getenv("SRC");
  CHECK_NE(src_env, nullptr)
      << "Expect to have the envvar |SRC| set when testing.";
  return base::FilePath(src_env).Append("testdata");
}

}  // namespace hardware_verifier
