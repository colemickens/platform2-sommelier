// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>
#include <string>
#include <vector>

#include <chromeos/string_utils.h>
#include <gtest/gtest.h>

#include "buffet/commands/command_dispatch_interface.h"
#include "buffet/commands/command_queue.h"

namespace {

std::unique_ptr<const buffet::CommandInstance> CreateDummyCommandInstance(
    const std::string& name = "base.reboot") {
  return std::unique_ptr<const buffet::CommandInstance>(
      new buffet::CommandInstance(name, "powerd", {}));
}

// Fake implementation of CommandDispachInterface.
// Just keeps track of commands being added to and removed from the queue.
// Aborts if duplicate commands are added or non-existent commands are removed.
class FakeDispatchInterface : public buffet::CommandDispachInterface {
 public:
  void OnCommandAdded(
      const std::string& command_id,
      const buffet::CommandInstance* command_instance) override {
    CHECK(ids_.insert(command_id).second)
        << "Command ID already exists: " << command_id;
    CHECK(commands_.insert(command_instance).second)
        << "Command instance already exists";
  }

  void OnCommandRemoved(
      const std::string& command_id,
      const buffet::CommandInstance* command_instance) override {
    CHECK_EQ(1, ids_.erase(command_id))
        << "Command ID not found: " << command_id;
    CHECK_EQ(1, commands_.erase(command_instance))
        << "Command instance not found";
  }

  // Get the comma-separated list of command IDs currently accumulated in the
  // command queue.
  std::string GetIDs() const {
    using chromeos::string_utils::Join;
    return Join(',', std::vector<std::string>(ids_.begin(), ids_.end()));
  }

 private:
  std::set<std::string> ids_;
  std::set<const buffet::CommandInstance*> commands_;
};

}  // anonymous namespace

TEST(CommandQueue, Empty) {
  buffet::CommandQueue queue;
  EXPECT_TRUE(queue.IsEmpty());
  EXPECT_EQ(0, queue.GetCount());
}

TEST(CommandQueue, Add) {
  buffet::CommandQueue queue;
  std::set<std::string> ids;  // Using set<> to check that IDs are unique.
  ids.insert(queue.Add(CreateDummyCommandInstance()));
  ids.insert(queue.Add(CreateDummyCommandInstance()));
  ids.insert(queue.Add(CreateDummyCommandInstance()));
  EXPECT_EQ(3, queue.GetCount());
  EXPECT_EQ(3, ids.size());
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

TEST(CommandQueue, Dispatch) {
  FakeDispatchInterface dispatch;
  buffet::CommandQueue queue;
  queue.SetCommandDispachInterface(&dispatch);
  std::string id1 = queue.Add(CreateDummyCommandInstance());
  std::string id2 = queue.Add(CreateDummyCommandInstance());
  std::set<std::string> ids{id1, id2};  // Make sure they are sorted properly.
  std::string expected_set = chromeos::string_utils::Join(
      ',', std::vector<std::string>(ids.begin(), ids.end()));
  EXPECT_EQ(expected_set, dispatch.GetIDs());
  queue.Remove(id1);
  EXPECT_EQ(id2, dispatch.GetIDs());
  queue.Remove(id2);
  EXPECT_EQ("", dispatch.GetIDs());
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
