// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "buffet/commands/command_definition.h"

#include <gtest/gtest.h>

using buffet::ObjectSchema;

TEST(CommandDefinition, Test) {
  std::unique_ptr<const ObjectSchema> params{ObjectSchema::Create()};
  std::unique_ptr<const ObjectSchema> results{ObjectSchema::Create()};
  const ObjectSchema* param_ptr = params.get();
  const ObjectSchema* results_ptr = results.get();
  buffet::CommandDefinition def{"powerd", std::move(params),
                                std::move(results)};
  EXPECT_EQ("powerd", def.GetCategory());
  EXPECT_EQ(param_ptr, def.GetParameters());
  EXPECT_EQ(results_ptr, def.GetResults());
}
