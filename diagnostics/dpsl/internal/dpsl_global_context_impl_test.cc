// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#include "diagnostics/dpsl/internal/dpsl_global_context_impl.h"
#include "diagnostics/dpsl/public/dpsl_global_context.h"

namespace diagnostics {

class DpslGlobalContextImplTest : public testing::Test {
 public:
  ~DpslGlobalContextImplTest() override {
    DpslGlobalContextImpl::CleanGlobalCounterForTesting();
  }
};

TEST_F(DpslGlobalContextImplTest, CreateAndForget) {
  ASSERT_TRUE(DpslGlobalContext::Create());

  ASSERT_DEATH(DpslGlobalContext::Create(),
               "Duplicate DpslGlobalContext instances");
}

TEST_F(DpslGlobalContextImplTest, CreateAndSave) {
  auto context = DpslGlobalContext::Create();
  ASSERT_TRUE(context);

  ASSERT_DEATH(DpslGlobalContext::Create(),
               "Duplicate DpslGlobalContext instances");
}

}  // namespace diagnostics
