// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/hook_table.h"

#include <string>

#include <base/bind.h>
#include <base/callback.h>
#include <base/message_loop.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "shill/error.h"
#include "shill/event_dispatcher.h"

using base::Callback;
using base::Closure;
using base::Bind;
using base::Unretained;
using std::string;
using ::testing::_;
using ::testing::InSequence;
using ::testing::Return;
using ::testing::SaveArg;

namespace shill {

class HookTableTest : public testing::Test {
 public:
  static const char kName[];
  MOCK_METHOD0(StartAction, void());
  MOCK_METHOD0(StartAction2, void());
  MOCK_METHOD1(DoneAction, void(const Error &));

 protected:
  HookTableTest()
      : hook_table_(&event_dispatcher_) {}

  EventDispatcher event_dispatcher_;
  HookTable hook_table_;
};

const char HookTableTest::kName[] = "test";

MATCHER(IsSuccess, "") {
  return arg.IsSuccess();
}

MATCHER(IsFailure, "") {
  return arg.IsFailure();
}

TEST_F(HookTableTest, ActionCompletes) {
  EXPECT_CALL(*this, StartAction());
  EXPECT_CALL(*this, DoneAction(IsSuccess()));
  Closure start_cb = Bind(&HookTableTest::StartAction, Unretained(this));
  Callback<void(const Error &)> done_cb = Bind(&HookTableTest::DoneAction,
                                               Unretained(this));
  hook_table_.Add(kName, start_cb);
  hook_table_.Run(0, done_cb);
  hook_table_.ActionComplete(kName);

  // Ensure that the timeout callback got cancelled.  If it did not get
  // cancelled, done_cb will be run twice and make this test fail.
  event_dispatcher_.DispatchPendingEvents();
}

ACTION_P2(CompleteAction, hook_table, name) {
  hook_table->ActionComplete(name);
}

TEST_F(HookTableTest, ActionCompletesInline) {
  // StartAction completes immediately before HookTable::Run() returns.
  EXPECT_CALL(*this, StartAction())
      .WillOnce(CompleteAction(&hook_table_, kName));
  EXPECT_CALL(*this, DoneAction(IsSuccess()));
  Closure start_cb = Bind(&HookTableTest::StartAction, Unretained(this));
  Callback<void(const Error &)> done_cb = Bind(&HookTableTest::DoneAction,
                                               Unretained(this));
  hook_table_.Add(kName, start_cb);
  hook_table_.Run(0, done_cb);

  // Ensure that the timeout callback got cancelled.  If it did not get
  // cancelled, done_cb will be run twice and make this test fail.
  event_dispatcher_.DispatchPendingEvents();
}

TEST_F(HookTableTest, ActionTimesOut) {
  const int kTimeout = 1;
  EXPECT_CALL(*this, StartAction());
  EXPECT_CALL(*this, DoneAction(IsFailure()));

  Closure start_cb = Bind(&HookTableTest::StartAction, Unretained(this));
  Callback<void(const Error &)> done_cb = Bind(&HookTableTest::DoneAction,
                                               Unretained(this));

  hook_table_.Add(kName, start_cb);
  hook_table_.Run(kTimeout, done_cb);

  // Cause the event dispatcher to exit after kTimeout + 1 ms.
  event_dispatcher_.PostDelayedTask(MessageLoop::QuitClosure(),
                                    kTimeout * + 1);
  event_dispatcher_.DispatchForever();
}

TEST_F(HookTableTest, MultipleActionsAllSucceed) {
  const string kName1 = "test1";
  const string kName2 = "test2";
  const string kName3 = "test3";
  Closure pending_cb;
  const int kTimeout = 10;
  EXPECT_CALL(*this, StartAction()).Times(2);

  // StartAction2 completes immediately before HookTable::Run() returns.
  EXPECT_CALL(*this, StartAction2())
      .WillOnce(CompleteAction(&hook_table_, kName1));
  EXPECT_CALL(*this, DoneAction(IsSuccess()));

  Closure start_cb = Bind(&HookTableTest::StartAction, Unretained(this));
  Closure start2_cb = Bind(&HookTableTest::StartAction2, Unretained(this));
  Callback<void(const Error &)> done_cb = Bind(&HookTableTest::DoneAction,
                                               Unretained(this));

  hook_table_.Add(kName1, start2_cb);
  hook_table_.Add(kName2, start_cb);
  hook_table_.Add(kName3, start_cb);
  hook_table_.Run(kTimeout, done_cb);
  hook_table_.ActionComplete(kName2);
  hook_table_.ActionComplete(kName3);
}

TEST_F(HookTableTest, MultipleActionsAndOneTimesOut) {
  const string kName1 = "test1";
  const string kName2 = "test2";
  const string kName3 = "test3";
  Closure pending_cb;
  const int kTimeout = 1;
  EXPECT_CALL(*this, StartAction()).Times(3);
  EXPECT_CALL(*this, DoneAction(IsFailure()));

  Closure start_cb = Bind(&HookTableTest::StartAction, Unretained(this));
  Callback<void(const Error &)> done_cb = Bind(&HookTableTest::DoneAction,
                                               Unretained(this));

  hook_table_.Add(kName1, start_cb);
  hook_table_.Add(kName2, start_cb);
  hook_table_.Add(kName3, start_cb);
  hook_table_.Run(kTimeout, done_cb);
  hook_table_.ActionComplete(kName1);
  hook_table_.ActionComplete(kName3);
  // Cause the event dispatcher to exit after kTimeout + 1 ms.
  event_dispatcher_.PostDelayedTask(MessageLoop::QuitClosure(),
                                    kTimeout + 1);
  event_dispatcher_.DispatchForever();
}

TEST_F(HookTableTest, AddActionsWithSameName) {
  EXPECT_CALL(*this, StartAction()).Times(0);
  EXPECT_CALL(*this, StartAction2());
  EXPECT_CALL(*this, DoneAction(IsSuccess()));
  Closure start_cb = Bind(&HookTableTest::StartAction, Unretained(this));
  Closure start2_cb = Bind(&HookTableTest::StartAction2, Unretained(this));
  Callback<void(const Error &)> done_cb = Bind(&HookTableTest::DoneAction,
                                               Unretained(this));
  hook_table_.Add(kName, start_cb);

  // Adding an action with the same name kName.  New callbacks should replace
  // old ones.
  hook_table_.Add(kName, start2_cb);
  hook_table_.Run(0, done_cb);
  hook_table_.ActionComplete(kName);

  // Ensure that the timeout callback got cancelled.  If it did not get
  // cancelled, done_cb will be run twice and make this test fail.
  event_dispatcher_.DispatchPendingEvents();
}

TEST_F(HookTableTest, RemoveAction) {
  EXPECT_CALL(*this, StartAction()).Times(0);
  EXPECT_CALL(*this, DoneAction(IsSuccess()));
  Closure start_cb = Bind(&HookTableTest::StartAction, Unretained(this));
  Callback<void(const Error &)> done_cb = Bind(&HookTableTest::DoneAction,
                                               Unretained(this));
  hook_table_.Add(kName, start_cb);
  hook_table_.Remove(kName);
  hook_table_.Run(0, done_cb);
}

TEST_F(HookTableTest, ActionCompleteFollowedByRemove) {
  EXPECT_CALL(*this, StartAction()).Times(0);
  Closure start_cb = Bind(&HookTableTest::StartAction, Unretained(this));
  hook_table_.Add(kName, start_cb);
  hook_table_.ActionComplete(kName);
  hook_table_.Remove(kName);
}

class SomeClass : public base::RefCounted<SomeClass> {
 public:
  SomeClass() {}
  void StartAction() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(SomeClass);
};

// This test verifies that a class that removes itself from a hook table upon
// destruction does not crash if the hook table is destroyed first.
TEST_F(HookTableTest, RefcountedObject) {
  scoped_ptr<HookTable> ht(new HookTable(&event_dispatcher_));
  {
    scoped_refptr<SomeClass> ref_counted_object = new SomeClass();
    Closure start_cb = Bind(&SomeClass::StartAction, ref_counted_object);
    ht->Add(kName, start_cb);
  }
}

TEST_F(HookTableTest, ActionAddedBeforePreviousActionCompletes) {
  EXPECT_CALL(*this, StartAction());
  EXPECT_CALL(*this, StartAction2()).Times(0);
  EXPECT_CALL(*this, DoneAction(IsSuccess()));
  Closure start_cb = Bind(&HookTableTest::StartAction, Unretained(this));
  Closure start2_cb = Bind(&HookTableTest::StartAction2, Unretained(this));
  Callback<void(const Error &)> done_cb = Bind(&HookTableTest::DoneAction,
                                               Unretained(this));
  hook_table_.Add(kName, start_cb);
  hook_table_.Run(0, done_cb);

  // An action with the same name is added before the previous actions complete.
  // It should not be run.
  hook_table_.Add(kName, start2_cb);
  hook_table_.ActionComplete(kName);
}


}  // namespace shill
