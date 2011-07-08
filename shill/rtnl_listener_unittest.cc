// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sys/socket.h>
#include <linux/netlink.h>

#include <base/callback_old.h>
#include <base/memory/scoped_ptr.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "shill/rtnl_handler.h"
#include "shill/rtnl_listener.h"

using testing::_;
using testing::Test;

namespace shill {

class RTNLListenerTest : public Test {
 public:
  RTNLListenerTest()
      : callback_(NewCallback(this, &RTNLListenerTest::Callback)) {}

  MOCK_METHOD1(Callback, void(struct nlmsghdr *));

 protected:
  scoped_ptr<Callback1<struct nlmsghdr *>::Type> callback_;
};

TEST_F(RTNLListenerTest, NoRun) {
  {
    RTNLListener listener(RTNLHandler::kRequestAddr, callback_.get());
    EXPECT_EQ(1, RTNLHandler::GetInstance()->listeners_.size());
    struct nlmsghdr message;
    EXPECT_CALL(*this, Callback(_)).Times(0);
    listener.NotifyEvent(RTNLHandler::kRequestLink, &message);
  }
  EXPECT_EQ(0, RTNLHandler::GetInstance()->listeners_.size());
}

TEST_F(RTNLListenerTest, Run) {
  {
    RTNLListener listener(
        RTNLHandler::kRequestLink | RTNLHandler::kRequestAddr,
        callback_.get());
    EXPECT_EQ(1, RTNLHandler::GetInstance()->listeners_.size());
    struct nlmsghdr message;
    EXPECT_CALL(*this, Callback(&message)).Times(1);
    listener.NotifyEvent(RTNLHandler::kRequestLink, &message);
  }
  EXPECT_EQ(0, RTNLHandler::GetInstance()->listeners_.size());
}

}  // namespace shill
