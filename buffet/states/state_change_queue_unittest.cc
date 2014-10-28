// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include <gtest/gtest.h>

#include "buffet/states/state_change_queue.h"

namespace buffet {

class StateChangeQueueTest : public ::testing::Test {
 public:
  void SetUp() override {
    queue_.reset(new StateChangeQueue(100));
  }

  void TearDown() override {
    queue_.reset();
  }

  std::unique_ptr<StateChangeQueue> queue_;
};

TEST_F(StateChangeQueueTest, Empty) {
  EXPECT_TRUE(queue_->IsEmpty());
  EXPECT_TRUE(queue_->GetAndClearRecordedStateChanges().empty());
}

TEST_F(StateChangeQueueTest, UpdateOne) {
  StateChange change;
  change.timestamp = base::Time::Now();
  change.property_set.emplace("prop.name", int{23});
  ASSERT_TRUE(queue_->NotifyPropertiesUpdated(change));
  EXPECT_FALSE(queue_->IsEmpty());
  auto changes = queue_->GetAndClearRecordedStateChanges();
  ASSERT_EQ(1, changes.size());
  EXPECT_EQ(change.timestamp, changes.front().timestamp);
  EXPECT_EQ(change.property_set, changes.front().property_set);
  EXPECT_TRUE(queue_->IsEmpty());
  EXPECT_TRUE(queue_->GetAndClearRecordedStateChanges().empty());
}

TEST_F(StateChangeQueueTest, UpdateMany) {
  StateChange change1;
  change1.timestamp = base::Time::Now();
  change1.property_set.emplace("prop.name1", int{23});
  ASSERT_TRUE(queue_->NotifyPropertiesUpdated(change1));
  StateChange change2;
  change2.timestamp = base::Time::Now();
  change2.property_set.emplace("prop.name1", int{17});
  change2.property_set.emplace("prop.name2", double{1.0});
  change2.property_set.emplace("prop.name3", bool{false});
  ASSERT_TRUE(queue_->NotifyPropertiesUpdated(change2));
  EXPECT_FALSE(queue_->IsEmpty());
  auto changes = queue_->GetAndClearRecordedStateChanges();
  ASSERT_EQ(2, changes.size());
  EXPECT_EQ(change1.timestamp, changes.front().timestamp);
  EXPECT_EQ(change1.property_set, changes.front().property_set);
  EXPECT_EQ(change2.timestamp, changes.back().timestamp);
  EXPECT_EQ(change2.property_set, changes.back().property_set);
  EXPECT_TRUE(queue_->IsEmpty());
  EXPECT_TRUE(queue_->GetAndClearRecordedStateChanges().empty());
}

TEST_F(StateChangeQueueTest, MaxQueueSize) {
  queue_.reset(new StateChangeQueue(2));
  base::Time start_time = base::Time::Now();
  base::TimeDelta time_delta = base::TimeDelta::FromMinutes(1);

  StateChange change;
  change.timestamp = start_time;
  change.property_set.emplace("prop.name1", int{1});
  change.property_set.emplace("prop.name2", int{2});
  ASSERT_TRUE(queue_->NotifyPropertiesUpdated(change));

  change.timestamp += time_delta;
  change.property_set.clear();
  change.property_set.emplace("prop.name1", int{3});
  change.property_set.emplace("prop.name3", int{4});
  ASSERT_TRUE(queue_->NotifyPropertiesUpdated(change));

  change.timestamp += time_delta;
  change.property_set.clear();
  change.property_set.emplace("prop.name10", int{10});
  change.property_set.emplace("prop.name11", int{11});
  ASSERT_TRUE(queue_->NotifyPropertiesUpdated(change));

  auto changes = queue_->GetAndClearRecordedStateChanges();
  ASSERT_EQ(2, changes.size());

  chromeos::VariantDictionary expected1{
    {"prop.name1", int{3}},
    {"prop.name2", int{2}},
    {"prop.name3", int{4}},
  };
  EXPECT_EQ(start_time + time_delta, changes.front().timestamp);
  EXPECT_EQ(expected1, changes.front().property_set);

  chromeos::VariantDictionary expected2{
    {"prop.name10", int{10}},
    {"prop.name11", int{11}},
  };
  EXPECT_EQ(start_time + 2 * time_delta, changes.back().timestamp);
  EXPECT_EQ(expected2, changes.back().property_set);
}

}  // namespace buffet
