// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "diagnostics/routines/routine_test_utils.h"

#include <gtest/gtest.h>

namespace diagnostics {

void VerifyNonInteractiveUpdate(
    const chromeos::cros_healthd::mojom::RoutineUpdateUnionPtr& update_union,
    chromeos::cros_healthd::mojom::DiagnosticRoutineStatusEnum expected_status,
    const std::string& expected_status_message) {
  ASSERT_FALSE(update_union.is_null());
  ASSERT_TRUE(update_union->is_noninteractive_update());
  const auto& noninteractive_update = update_union->get_noninteractive_update();
  EXPECT_EQ(noninteractive_update->status_message, expected_status_message);
  EXPECT_EQ(noninteractive_update->status, expected_status);
}

}  // namespace diagnostics
