// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include <gtest/gtest.h>

#include <base/compiler_specific.h>

#include "metrics/persistent_integer.h"
#include "metrics/persistent_integer_test_base.h"

using chromeos_metrics::PersistentInteger;

class PersistentIntegerTest :
    public chromeos_metrics::PersistentIntegerTestBase {
};

TEST_F(PersistentIntegerTest, BasicChecks) {
  const std::string pi_name("xyz");
  std::unique_ptr<PersistentInteger> pi(new PersistentInteger(pi_name));

  // Test initialization.
  EXPECT_EQ(0, pi->Get());
  EXPECT_EQ(pi_name, pi->Name());  // not too useful really

  // Test set and add.
  pi->Set(2);
  pi->Add(3);
  EXPECT_EQ(5, pi->Get());

  // Test persistence.
  pi.reset(new PersistentInteger(pi_name));
  EXPECT_EQ(5, pi->Get());

  // Test GetAndClear.
  EXPECT_EQ(5, pi->GetAndClear());
  EXPECT_EQ(pi->Get(), 0);

  // Another persistence test.
  pi.reset(new PersistentInteger(pi_name));
  EXPECT_EQ(0, pi->Get());
}
