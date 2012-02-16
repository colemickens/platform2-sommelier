// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sys/socket.h>
#include <linux/netlink.h>

#include <base/bind.h>
#include <base/memory/scoped_ptr.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "shill/rtnl_handler.h"
#include "shill/rtnl_listener.h"
#include "shill/rtnl_message.h"

using base::Bind;
using base::Callback;
using base::Unretained;
using testing::_;
using testing::A;
using testing::Test;

namespace shill {

class RTNLListenerTest : public Test {
 public:
  RTNLListenerTest()
      : callback_(Bind(&RTNLListenerTest::ListenerCallback,
                       Unretained(this))) {}

  MOCK_METHOD1(ListenerCallback, void(const RTNLMessage &));

 protected:
  Callback<void(const RTNLMessage &)> callback_;
};

TEST_F(RTNLListenerTest, NoRun) {
  {
    RTNLListener listener(RTNLHandler::kRequestAddr, callback_);
    EXPECT_EQ(1, RTNLHandler::GetInstance()->listeners_.size());
    RTNLMessage message;
    EXPECT_CALL(*this, ListenerCallback(_)).Times(0);
    listener.NotifyEvent(RTNLHandler::kRequestLink, message);
  }
  EXPECT_EQ(0, RTNLHandler::GetInstance()->listeners_.size());
}

TEST_F(RTNLListenerTest, Run) {
  {
    RTNLListener listener(
        RTNLHandler::kRequestLink | RTNLHandler::kRequestAddr,
        callback_);
    EXPECT_EQ(1, RTNLHandler::GetInstance()->listeners_.size());
    RTNLMessage message;
    EXPECT_CALL(*this, ListenerCallback(A<const RTNLMessage &>())).Times(1);
    listener.NotifyEvent(RTNLHandler::kRequestLink, message);
  }
  EXPECT_EQ(0, RTNLHandler::GetInstance()->listeners_.size());
}

}  // namespace shill
