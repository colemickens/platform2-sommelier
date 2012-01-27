// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tests for the MockCallback class.  They ensures that MockCallback can be used
// to replace each Callback type in base/callbacks_old.h.

#include "shill/mock_callback.h"

#include <base/logging.h>
#include <base/scoped_ptr.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

using std::string;
using ::testing::_;
using ::testing::Return;
using ::testing::Test;

namespace shill {
class MockCallbackTest : public Test {
 public:
  // RunCallback simulates how a real callback may be invoked.  It calls a
  // callback's Run method with the correct number of arguments.  There is a
  // version for each type of callback.
  void RunCallback(Callback0::Type *cb) {
    cb->Run();
  }

  template <typename R>
  R RunCallback(typename CallbackWithReturnValue<R>::Type *cb) {
    return cb->Run();
  }

  template <typename Arg1>
  void RunCallback(typename Callback1<Arg1>::Type *cb, Arg1 arg1) {
    cb->Run(arg1);
  }

  template <typename Arg1, typename Arg2>
  void RunCallback(typename Callback2<Arg1, Arg2>::Type *cb,
                   Arg1 arg1, Arg2 arg2) {
    cb->Run(arg1, arg2);
  }

  template <typename Arg1, typename Arg2, typename Arg3>
  void RunCallback(typename Callback3<Arg1, Arg2, Arg3>::Type *cb,
                   Arg1 arg1, Arg2 arg2, Arg3 arg3) {
    cb->Run(arg1, arg2, arg3);
  }

  template <typename Arg1, typename Arg2, typename Arg3, typename Arg4>
  void RunCallback(typename Callback4<Arg1, Arg2, Arg3, Arg4>::Type *cb,
                   Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4) {
    cb->Run(arg1, arg2, arg3, arg4);
  }

  template <typename Arg1, typename Arg2, typename Arg3, typename Arg4,
            typename Arg5>
  void RunCallback(typename Callback5<Arg1, Arg2, Arg3, Arg4, Arg5>::Type *cb,
                   Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4, Arg5 arg5) {
    cb->Run(arg1, arg2, arg3, arg4, arg5);
  }
};

TEST_F(MockCallbackTest, Callback0) {
  scoped_ptr<MockCallback<void()> > mock_callback(NewMockCallback());
  EXPECT_CALL(*mock_callback, OnRun());
  RunCallback(mock_callback.get());
}

TEST_F(MockCallbackTest, CallbackWithReturnValue) {
  scoped_ptr<MockCallback<int()> > mock_callback(NewMockCallback());
  const int kReturn = 99;
  EXPECT_CALL(*mock_callback, OnRun()).WillOnce(Return(kReturn));
  EXPECT_EQ(kReturn, RunCallback<int>(mock_callback.get()));
}

TEST_F(MockCallbackTest, Callback1) {
    scoped_ptr<MockCallback<void(int)> > mock_callback(NewMockCallback());
  const int kArg = 99;
  EXPECT_CALL(*mock_callback, OnRun(kArg));
  RunCallback(mock_callback.get(), kArg);
}

TEST_F(MockCallbackTest, Callback2) {
  scoped_ptr<MockCallback<void(int, string)> > mock_callback(
      NewMockCallback());
  const int kArg1 = 99;
  const string kArg2 = "Beeblebrox";
  EXPECT_CALL(*mock_callback, OnRun(kArg1, kArg2));
  RunCallback(mock_callback.get(), kArg1, kArg2);
}

TEST_F(MockCallbackTest, Callback3) {
  scoped_ptr<MockCallback<void(int, string, int)> > mock_callback(
      NewMockCallback());
  const int kArg1 = 99;
  const string kArg2 = "Beeblebrox";
  const int kArg3 = 42;
  EXPECT_CALL(*mock_callback, OnRun(kArg1, kArg2, kArg3));
  RunCallback(mock_callback.get(), kArg1, kArg2, kArg3);
}

TEST_F(MockCallbackTest, Callback4) {
  scoped_ptr<MockCallback<void(int, string, int, string)> > mock_callback(
      NewMockCallback());
  const int kArg1 = 99;
  const string kArg2 = "Beeblebrox";
  const int kArg3 = 42;
  const string kArg4 = "Zaphod";
  EXPECT_CALL(*mock_callback, OnRun(kArg1, kArg2, kArg3, kArg4));
  RunCallback(mock_callback.get(), kArg1, kArg2, kArg3, kArg4);
}

TEST_F(MockCallbackTest, Callback5) {
  scoped_ptr<MockCallback<void(int, string, int, string, int)> > mock_callback(
      NewMockCallback());
  const int kArg1 = 99;
  const string kArg2 = "Beeblebrox";
  const int kArg3 = 42;
  const string kArg4 = "Zaphod";
  const int kArg5 = 101;
  EXPECT_CALL(*mock_callback, OnRun(kArg1, kArg2, kArg3, kArg4, kArg5));
  RunCallback(mock_callback.get(), kArg1, kArg2, kArg3, kArg4, kArg5);
}

}  // namespace shill
