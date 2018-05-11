// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bluetooth/newblued/newblue.h"

#include <memory>
#include <utility>

#include <base/bind.h>
#include <base/message_loop/message_loop.h>
#include <gtest/gtest.h>

#include "bluetooth/newblued/mock_libnewblue.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Return;

namespace bluetooth {

class NewblueTest : public ::testing::Test {
 public:
  void SetUp() override {
    auto libnewblue = std::make_unique<MockLibNewblue>();
    libnewblue_ = libnewblue.get();
    newblue_ = std::make_unique<Newblue>(std::move(libnewblue));
  }

  bool StubHciUp(const uint8_t* address,
                 hciReadyForUpCbk callback,
                 void* callback_data) {
    callback(callback_data);
    return true;
  }

  void OnReadyForUp() { is_ready_for_up_ = true; }

 protected:
  base::MessageLoop message_loop_;
  bool is_ready_for_up_ = false;
  std::unique_ptr<Newblue> newblue_;
  MockLibNewblue* libnewblue_;
};

TEST_F(NewblueTest, ListenReadyForUp) {
  newblue_->Init();

  EXPECT_CALL(*libnewblue_, HciUp(_, _, _))
      .WillOnce(Invoke(this, &NewblueTest::StubHciUp));
  newblue_->ListenReadyForUp(
      base::Bind(&NewblueTest::OnReadyForUp, base::Unretained(this)));
  message_loop_.RunUntilIdle();
  EXPECT_TRUE(is_ready_for_up_);
}

TEST_F(NewblueTest, BringUp) {
  EXPECT_CALL(*libnewblue_, HciIsUp()).WillOnce(Return(false));
  EXPECT_FALSE(newblue_->BringUp());

  EXPECT_CALL(*libnewblue_, HciIsUp()).WillOnce(Return(true));
  EXPECT_CALL(*libnewblue_, L2cInit()).WillOnce(Return(0));
  EXPECT_CALL(*libnewblue_, AttInit()).WillOnce(Return(true));
  EXPECT_CALL(*libnewblue_, GattProfileInit()).WillOnce(Return(true));
  EXPECT_CALL(*libnewblue_, GattBuiltinInit()).WillOnce(Return(true));
  EXPECT_CALL(*libnewblue_, SmInit(HCI_DISP_CAP_NONE)).WillOnce(Return(true));
  EXPECT_TRUE(newblue_->BringUp());
}

}  // namespace bluetooth
