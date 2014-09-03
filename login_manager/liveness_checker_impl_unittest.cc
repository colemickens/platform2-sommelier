// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/liveness_checker_impl.h"

#include <base/memory/ref_counted.h>
#include <base/memory/scoped_ptr.h>
#include <base/message_loop/message_loop.h>
#include <base/message_loop/message_loop_proxy.h>
#include <base/run_loop.h>
#include <base/time/time.h>
#include <chromeos/dbus/service_constants.h>
#include <dbus/message.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "login_manager/mock_object_proxy.h"
#include "login_manager/mock_process_manager_service.h"
#include "login_manager/mock_system_utils.h"

using ::base::TimeDelta;
using ::testing::AtLeast;
using ::testing::DoAll;
using ::testing::InvokeWithoutArgs;
using ::testing::Return;
using ::testing::StrEq;
using ::testing::StrictMock;
using ::testing::_;

namespace login_manager {

class LivenessCheckerImplTest : public ::testing::Test {
 public:
  LivenessCheckerImplTest() {}
  virtual ~LivenessCheckerImplTest() {}

  virtual void SetUp() {
    manager_.reset(new StrictMock<MockProcessManagerService>);
    object_proxy_ = new MockObjectProxy;
    loop_proxy_ = base::MessageLoopProxy::current();
    checker_.reset(new LivenessCheckerImpl(manager_.get(),
                                           object_proxy_.get(),
                                           loop_proxy_,
                                           true,
                                           TimeDelta::FromSeconds(0)));
  }

  void ExpectUnAckedLivenessPing() {
    EXPECT_CALL(*object_proxy_.get(), CallMethod(_, _, _)).Times(1);
  }

  // Expect two pings, the first with a response.
  void ExpectLivenessPingResponsePing() {
    EXPECT_CALL(*object_proxy_.get(), CallMethod(_, _, _))
        .WillOnce(Invoke(this, &LivenessCheckerImplTest::Respond))
        .WillOnce(Return());
  }

  // Expect three runs through CheckAndSendLivenessPing():
  // 1) No ping has been sent before, so expect initial ping and ACK it.
  // 2) Last ping was ACK'd, so expect a no-op during this run.
  // 3) Caller should expect action during this run; Quit after it.
  void ExpectPingResponsePingCheckPingAndQuit() {
    EXPECT_CALL(*object_proxy_.get(), CallMethod(_, _, _))
        .WillOnce(Invoke(this, &LivenessCheckerImplTest::Respond))
        .WillOnce(Return())
        .WillOnce(
            InvokeWithoutArgs(base::MessageLoop::current(),
                              &base::MessageLoop::QuitNow));
  }

  base::MessageLoop loop_;
  scoped_refptr<base::MessageLoopProxy> loop_proxy_;
  scoped_refptr<MockObjectProxy> object_proxy_;
  scoped_ptr<StrictMock<MockProcessManagerService> > manager_;

  scoped_ptr<LivenessCheckerImpl> checker_;

 private:
  void Respond(dbus::MethodCall* method_call,
               int timeout_ms,
               dbus::ObjectProxy::ResponseCallback callback) {
    callback.Run(dbus::Response::CreateEmpty().get());
  }

  DISALLOW_COPY_AND_ASSIGN(LivenessCheckerImplTest);
};

TEST_F(LivenessCheckerImplTest, CheckAndSendOutstandingPing) {
  ExpectUnAckedLivenessPing();
  EXPECT_CALL(*manager_.get(), AbortBrowser(SIGFPE, _)).Times(1);
  checker_->CheckAndSendLivenessPing(TimeDelta());
  base::RunLoop().RunUntilIdle();
}

TEST_F(LivenessCheckerImplTest, CheckAndSendAckedThenOutstandingPing) {
  ExpectLivenessPingResponsePing();
  EXPECT_CALL(*manager_.get(), AbortBrowser(SIGFPE, _)).Times(1);
  checker_->CheckAndSendLivenessPing(TimeDelta());
  base::RunLoop().RunUntilIdle();
}

TEST_F(LivenessCheckerImplTest, CheckAndSendAckedThenOutstandingPingNeutered) {
  checker_.reset(new LivenessCheckerImpl(manager_.get(),
                                         object_proxy_.get(),
                                         loop_proxy_,
                                         false,  // Disable aborting
                                         TimeDelta()));
  ExpectPingResponsePingCheckPingAndQuit();
  // Expect _no_ browser abort!
  checker_->CheckAndSendLivenessPing(TimeDelta());
  base::RunLoop().RunUntilIdle();
}

TEST_F(LivenessCheckerImplTest, StartStop) {
  checker_->Start();
  EXPECT_TRUE(checker_->IsRunning());
  checker_->Stop();  // Should cancel ping, so...
  EXPECT_FALSE(checker_->IsRunning());
}

}  // namespace login_manager
