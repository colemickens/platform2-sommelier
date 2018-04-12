// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef METRICS_PERSISTENT_INTEGER_TEST_BASE_H_
#define METRICS_PERSISTENT_INTEGER_TEST_BASE_H_

#include <gtest/gtest.h>

#include "base/files/scoped_temp_dir.h"

namespace chromeos_metrics {

class PersistentIntegerTestBase : public testing::Test {
 public:
  void SetUp() override;
 private:
  base::ScopedTempDir test_dir_;
};

}  // namespace chromeos_metrics

#endif  // METRICS_PERSISTENT_INTEGER_TEST_BASE_H_
