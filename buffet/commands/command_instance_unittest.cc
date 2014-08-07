// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#include "buffet/commands/command_instance.h"
#include "buffet/commands/prop_types.h"

TEST(CommandInstance, Test) {
  buffet::native_types::Object params;
  buffet::IntPropType int_prop;
  buffet::DoublePropType dbl_prop;
  params.insert(std::make_pair("freq", dbl_prop.CreateValue(800.5)));
  params.insert(std::make_pair("volume", int_prop.CreateValue(100)));
  buffet::CommandInstance instance("robot._beep", "robotd", params);

  EXPECT_EQ("robot._beep", instance.GetName());
  EXPECT_EQ("robotd", instance.GetCategory());
  EXPECT_EQ(params, instance.GetParameters());
  EXPECT_DOUBLE_EQ(800.5,
                   instance.FindParameter("freq")->GetDouble()->GetValue());
  EXPECT_EQ(100, instance.FindParameter("volume")->GetInt()->GetValue());
  EXPECT_EQ(nullptr, instance.FindParameter("blah").get());
}
