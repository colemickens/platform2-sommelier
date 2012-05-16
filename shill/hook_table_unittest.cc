// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/hook_table.h"

#include <base/bind.h>
#include <base/callback.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "shill/error.h"
#include "shill/mock_event_dispatcher.h"

using base::Callback;
using base::Closure;
using base::Bind;
using base::Unretained;
using ::testing::_;
using ::testing::InSequence;
using ::testing::Return;
using ::testing::SaveArg;

namespace shill {

class HookTableTest : public testing::Test {
 public:
  MOCK_METHOD0(StartAction, void());
  MOCK_METHOD0(PollAction, bool());
  MOCK_METHOD1(DoneAction, void(const Error &));

 protected:
  HookTableTest()
      : hook_table_(&event_dispatcher_) {}

  MockEventDispatcher event_dispatcher_;
  HookTable hook_table_;
};

MATCHER(IsSuccess, "") {
  return arg.IsSuccess();
}

MATCHER(IsFailure, "") {
  return arg.IsFailure();
}

TEST_F(HookTableTest, ActionCompletesImmediately) {
  EXPECT_CALL(*this, StartAction());
  EXPECT_CALL(*this, PollAction()).WillOnce(Return(true));
  EXPECT_CALL(*this, DoneAction(IsSuccess()));
  EXPECT_CALL(event_dispatcher_, PostDelayedTask(_, _)).Times(0);
  Closure start_cb = Bind(&HookTableTest::StartAction, Unretained(this));
  Callback<bool()> poll_cb = Bind(&HookTableTest::PollAction, Unretained(this));
  Callback<void(const Error &)> done_cb = Bind(&HookTableTest::DoneAction,
                                               Unretained(this));
  hook_table_.Add("test", start_cb, poll_cb);
  hook_table_.Run(0, done_cb);
}

TEST_F(HookTableTest, DelayedAction) {
  Closure pending_cb;
  const int kTimeout = 10;
  const int64 kExpectedDelay = kTimeout * 1000 / HookTable::kPollIterations;
  EXPECT_CALL(*this, StartAction());
  {
    InSequence s;
    EXPECT_CALL(*this, PollAction()).WillOnce(Return(false));
    EXPECT_CALL(event_dispatcher_, PostDelayedTask(_, kExpectedDelay))
        .WillOnce(SaveArg<0>(&pending_cb));
    EXPECT_CALL(*this, PollAction()).WillOnce(Return(true));
    EXPECT_CALL(*this, DoneAction(IsSuccess()));
  }
  Closure start_cb = Bind(&HookTableTest::StartAction, Unretained(this));
  Callback<bool()> poll_cb = Bind(&HookTableTest::PollAction, Unretained(this));
  Callback<void(const Error &)> done_cb = Bind(&HookTableTest::DoneAction,
                                               Unretained(this));

  hook_table_.Add("test", start_cb, poll_cb);
  hook_table_.Run(kTimeout, done_cb);
  ASSERT_FALSE(pending_cb.is_null());
  pending_cb.Run();
}

TEST_F(HookTableTest, ActionTimesOut) {
  Closure pending_cb;
  const int kTimeout = 10;
  const int64 kExpectedDelay = kTimeout * 1000 / HookTable::kPollIterations;
  EXPECT_CALL(*this, StartAction());
  EXPECT_CALL(*this, PollAction()).WillRepeatedly(Return(false));
  EXPECT_CALL(event_dispatcher_, PostDelayedTask(_, kExpectedDelay))
      .Times(HookTable::kPollIterations)
      .WillRepeatedly(SaveArg<0>(&pending_cb));
  EXPECT_CALL(*this, DoneAction(IsFailure()));

  Closure start_cb = Bind(&HookTableTest::StartAction, Unretained(this));
  Callback<bool()> poll_cb = Bind(&HookTableTest::PollAction, Unretained(this));
  Callback<void(const Error &)> done_cb = Bind(&HookTableTest::DoneAction,
                                               Unretained(this));

  hook_table_.Add("test", start_cb, poll_cb);
  hook_table_.Run(kTimeout, done_cb);
  for (int i = 0; i < HookTable::kPollIterations; ++i) {
    ASSERT_FALSE(pending_cb.is_null());
    pending_cb.Run();
  }
}

TEST_F(HookTableTest, MultipleActionsAllSucceed) {
  Closure pending_cb;
  const int kTimeout = 10;
  EXPECT_CALL(*this, StartAction()).Times(3);
  EXPECT_CALL(*this, PollAction())
      .Times(3)
      .WillRepeatedly(Return(true));
  EXPECT_CALL(event_dispatcher_, PostDelayedTask(_, _)).Times(0);
  EXPECT_CALL(*this, DoneAction(IsSuccess()));

  Closure start_cb = Bind(&HookTableTest::StartAction, Unretained(this));
  Callback<bool()> poll_cb = Bind(&HookTableTest::PollAction, Unretained(this));
  Callback<void(const Error &)> done_cb = Bind(&HookTableTest::DoneAction,
                                               Unretained(this));

  hook_table_.Add("test1", start_cb, poll_cb);
  hook_table_.Add("test2", start_cb, poll_cb);
  hook_table_.Add("test3", start_cb, poll_cb);
  hook_table_.Run(kTimeout, done_cb);
}

TEST_F(HookTableTest, MultipleActionsAndOneTimesOut) {
  Closure pending_cb;
  const int kTimeout = 10;
  const int64 kExpectedDelay = kTimeout * 1000 / HookTable::kPollIterations;
  EXPECT_CALL(*this, StartAction()).Times(3);
  EXPECT_CALL(*this, PollAction())
      .Times((HookTable::kPollIterations + 1) * 3)
      .WillOnce(Return(true))
      .WillOnce(Return(true))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(event_dispatcher_, PostDelayedTask(_, kExpectedDelay))
      .Times(HookTable::kPollIterations)
      .WillRepeatedly(SaveArg<0>(&pending_cb));
  EXPECT_CALL(*this, DoneAction(IsFailure()));

  Closure start_cb = Bind(&HookTableTest::StartAction, Unretained(this));
  Callback<bool()> poll_cb = Bind(&HookTableTest::PollAction, Unretained(this));
  Callback<void(const Error &)> done_cb = Bind(&HookTableTest::DoneAction,
                                               Unretained(this));

  hook_table_.Add("test1", start_cb, poll_cb);
  hook_table_.Add("test2", start_cb, poll_cb);
  hook_table_.Add("test3", start_cb, poll_cb);
  hook_table_.Run(kTimeout, done_cb);
  for (int i = 0; i < HookTable::kPollIterations; ++i) {
    ASSERT_FALSE(pending_cb.is_null());
    pending_cb.Run();
  }
}

}  // namespace shill
