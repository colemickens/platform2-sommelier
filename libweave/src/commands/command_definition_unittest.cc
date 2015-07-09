// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libweave/src/commands/command_definition.h"

#include <gtest/gtest.h>

namespace weave {

TEST(CommandVisibility, DefaultConstructor) {
  CommandDefinition::Visibility visibility;
  EXPECT_FALSE(visibility.local);
  EXPECT_FALSE(visibility.cloud);
}

TEST(CommandVisibility, InitialState) {
  auto visibility = CommandDefinition::Visibility::GetAll();
  EXPECT_TRUE(visibility.local);
  EXPECT_TRUE(visibility.cloud);

  visibility = CommandDefinition::Visibility::GetLocal();
  EXPECT_TRUE(visibility.local);
  EXPECT_FALSE(visibility.cloud);

  visibility = CommandDefinition::Visibility::GetCloud();
  EXPECT_FALSE(visibility.local);
  EXPECT_TRUE(visibility.cloud);

  visibility = CommandDefinition::Visibility::GetNone();
  EXPECT_FALSE(visibility.local);
  EXPECT_FALSE(visibility.cloud);
}

TEST(CommandVisibility, FromString) {
  CommandDefinition::Visibility visibility;

  ASSERT_TRUE(visibility.FromString("local", nullptr));
  EXPECT_TRUE(visibility.local);
  EXPECT_FALSE(visibility.cloud);

  ASSERT_TRUE(visibility.FromString("cloud", nullptr));
  EXPECT_FALSE(visibility.local);
  EXPECT_TRUE(visibility.cloud);

  ASSERT_TRUE(visibility.FromString("cloud,local", nullptr));
  EXPECT_TRUE(visibility.local);
  EXPECT_TRUE(visibility.cloud);

  ASSERT_TRUE(visibility.FromString("none", nullptr));
  EXPECT_FALSE(visibility.local);
  EXPECT_FALSE(visibility.cloud);

  ASSERT_TRUE(visibility.FromString("all", nullptr));
  EXPECT_TRUE(visibility.local);
  EXPECT_TRUE(visibility.cloud);

  ASSERT_TRUE(visibility.FromString("", nullptr));
  EXPECT_FALSE(visibility.local);
  EXPECT_FALSE(visibility.cloud);

  chromeos::ErrorPtr error;
  ASSERT_FALSE(visibility.FromString("cloud,all", &error));
  EXPECT_EQ("invalid_parameter_value", error->GetCode());
  EXPECT_EQ("Invalid command visibility value 'all'", error->GetMessage());
}

TEST(CommandVisibility, ToString) {
  EXPECT_EQ("none", CommandDefinition::Visibility::GetNone().ToString());
  EXPECT_EQ("local", CommandDefinition::Visibility::GetLocal().ToString());
  EXPECT_EQ("cloud", CommandDefinition::Visibility::GetCloud().ToString());
  EXPECT_EQ("all", CommandDefinition::Visibility::GetAll().ToString());
}

TEST(CommandDefinition, Test) {
  std::unique_ptr<const ObjectSchema> params{ObjectSchema::Create()};
  std::unique_ptr<const ObjectSchema> progress{ObjectSchema::Create()};
  std::unique_ptr<const ObjectSchema> results{ObjectSchema::Create()};
  const ObjectSchema* param_ptr = params.get();
  const ObjectSchema* progress_ptr = progress.get();
  const ObjectSchema* results_ptr = results.get();
  CommandDefinition def{
      "powerd", std::move(params), std::move(progress), std::move(results)};
  EXPECT_EQ("powerd", def.GetCategory());
  EXPECT_EQ(param_ptr, def.GetParameters());
  EXPECT_EQ(progress_ptr, def.GetProgress());
  EXPECT_EQ(results_ptr, def.GetResults());
  EXPECT_EQ("all", def.GetVisibility().ToString());

  def.SetVisibility(CommandDefinition::Visibility::GetLocal());
  EXPECT_EQ("local", def.GetVisibility().ToString());
}

}  // namespace weave
