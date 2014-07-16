// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#include "buffet/commands/command_definition.h"

TEST(CommandDefinition, Test) {
  auto params = std::make_shared<buffet::ObjectSchema>();
  buffet::CommandDefinition def("powerd", params);
  EXPECT_EQ("powerd", def.GetCategory());
  EXPECT_EQ(params, def.GetParameters());
}
