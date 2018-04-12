// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "metrics/persistent_integer.h"

#include "metrics/persistent_integer_test_base.h"

namespace chromeos_metrics {

void PersistentIntegerTestBase::SetUp() {
  CHECK(test_dir_.CreateUniqueTempDir());
  std::string dirname = test_dir_.path().MaybeAsASCII() + "/";
  CHECK(!dirname.empty());
  PersistentInteger::SetTestingMode(dirname);
}

}  // namespace chromeos_metrics
