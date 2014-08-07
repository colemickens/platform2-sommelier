// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include <gtest/gtest.h>

#include "buffet/commands/command_queue.h"

namespace {

std::unique_ptr<const buffet::CommandInstance> CreateDummyCommandInstance(
    const std::string& name = "base.reboot") {
  return std::unique_ptr<const buffet::CommandInstance>(
      new buffet::CommandInstance(name, "powerd", {}));
}

}  // anonymous namespace

TEST(CommandQueue, Empty) {
  buffet::CommandQueue queue;
  EXPECT_TRUE(queue.IsEmpty());
  EXPECT_EQ(0, queue.GetCount());
}

TEST(CommandQueue, Add) {
  buffet::CommandQueue queue;
  EXPECT_EQ("1", queue.Add(CreateDummyCommandInstance()));
  EXPECT_EQ("2", queue.Add(CreateDummyCommandInstance()));
  EXPECT_EQ("3", queue.Add(CreateDummyCommandInstance()));
  EXPECT_EQ(3, queue.GetCount());
  EXPECT_FALSE(queue.IsEmpty());
}

TEST(CommandQueue, Remove) {
  buffet::CommandQueue queue;
  std::string id1 = queue.Add(CreateDummyCommandInstance());
  std::string id2 = queue.Add(CreateDummyCommandInstance());
  EXPECT_FALSE(queue.IsEmpty());
  EXPECT_EQ(nullptr, queue.Remove("dummy").get());
  EXPECT_EQ(2, queue.GetCount());
  EXPECT_NE(nullptr, queue.Remove(id1).get());
  EXPECT_EQ(1, queue.GetCount());
  EXPECT_EQ(nullptr, queue.Remove(id1).get());
  EXPECT_EQ(1, queue.GetCount());
  EXPECT_NE(nullptr, queue.Remove(id2).get());
  EXPECT_EQ(0, queue.GetCount());
  EXPECT_EQ(nullptr, queue.Remove(id2).get());
  EXPECT_EQ(0, queue.GetCount());
  EXPECT_TRUE(queue.IsEmpty());
}

TEST(CommandQueue, Find) {
  buffet::CommandQueue queue;
  std::string id1 = queue.Add(CreateDummyCommandInstance("base.reboot"));
  std::string id2 = queue.Add(CreateDummyCommandInstance("base.shutdown"));
  EXPECT_EQ(nullptr, queue.Find("dummy"));
  auto cmd1 = queue.Find(id1);
  EXPECT_NE(nullptr, cmd1);
  EXPECT_EQ("base.reboot", cmd1->GetName());
  auto cmd2 = queue.Find(id2);
  EXPECT_NE(nullptr, cmd2);
  EXPECT_EQ("base.shutdown", cmd2->GetName());
}
