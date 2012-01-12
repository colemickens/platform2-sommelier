// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <base/callback_old.h>
#include <base/logging.h>
#include <base/memory/ref_counted.h>
#include <base/message_loop_proxy.h>
#include <base/stringprintf.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "shill/io_handler.h"
#include "shill/mock_control.h"
#include "shill/shill_daemon.h"
#include "shill/shill_test_config.h"

namespace shill {
using ::testing::Test;
using ::testing::_;
using ::testing::Gt;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::StrictMock;

class MockEventDispatchTester {
 public:
  explicit MockEventDispatchTester(EventDispatcher *dispatcher)
      : dispatcher_(dispatcher),
        triggered_(false),
        callback_count_(0),
        got_data_(false),
        got_ready_(false),
        data_callback_(NULL),
        input_handler_(NULL),
        tester_factory_(this) {
  }

  void ScheduleFailSafe() {
    // Set up a failsafe, so the test still exits even if something goes
    // wrong.  The Factory owns the RunnableMethod, but we get a pointer to it.
    failsafe_ = tester_factory_.NewRunnableMethod(
        &MockEventDispatchTester::StopDispatcher);
    dispatcher_->PostDelayedTask(failsafe_, 100);
  }

  void ScheduleTimedTasks() {
    dispatcher_->PostDelayedTask(
        tester_factory_.NewRunnableMethod(&MockEventDispatchTester::Trigger),
        10);
  }

  void RescheduleUnlessTriggered() {
    ++callback_count_;
    if (!triggered_) {
      dispatcher_->PostTask(
          tester_factory_.NewRunnableMethod(
              &MockEventDispatchTester::RescheduleUnlessTriggered));
    } else {
      failsafe_->Cancel();
      StopDispatcher();
    }
  }

  void StopDispatcher() {
    dispatcher_->PostTask(new MessageLoop::QuitTask);
  }

  void Trigger() {
    LOG(INFO) << "MockEventDispatchTester handling " << callback_count_;
    CallbackComplete(callback_count_);
    triggered_ = true;
  }

  void HandleData(InputData *inputData) {
    LOG(INFO) << "MockEventDispatchTester handling data len "
              << base::StringPrintf("%zd %.*s", inputData->len,
                                    static_cast<int>(inputData->len),
                                    inputData->buf);
    got_data_ = true;
    IOComplete(inputData->len);
    StopDispatcher();
  }

  bool GetData() { return got_data_; }

  void ListenIO(int fd) {
    data_callback_.reset(
        NewCallback(this, &MockEventDispatchTester::HandleData));
    input_handler_.reset(
        dispatcher_->CreateInputHandler(fd, data_callback_.get()));
  }

  void StopListenIO() {
    got_data_ = false;
    input_handler_.reset(NULL);
  }

  void HandleReady(int fd) {
    // Stop event handling after we receive in input-ready event.  We should
    // no longer be called until events are re-enabled.
    input_handler_->Stop();

    if (got_ready_) {
      // If we're still getting events after we have stopped them, something
      // is really wrong, and we cannot just depend on ASSERT_FALSE() to get
      // us out of it.  Make sure the dispatcher is also stopped, or else we
      // could end up never exiting.
      StopDispatcher();
      ASSERT_FALSE(got_ready_) << "failed to stop Input Ready events";
    }
    got_ready_ = true;

    LOG(INFO) << "MockEventDispatchTester handling ready for fd " << fd;
    IOComplete(callback_count_);

    if (callback_count_) {
      StopDispatcher();
    } else {
      // Restart Ready events after 10 millisecond delay.
      callback_count_++;
      dispatcher_->PostDelayedTask(tester_factory_.NewRunnableMethod(
          &MockEventDispatchTester::RestartReady), 10);
    }
  }

  void RestartReady() {
    got_ready_ = false;
    input_handler_->Start();
  }

  void ListenReady(int fd) {
    ready_callback_.reset(
        NewCallback(this, &MockEventDispatchTester::HandleReady));
    input_handler_.reset(
        dispatcher_->CreateReadyHandler(fd, IOHandler::kModeInput,
                                        ready_callback_.get()));
  }

  void StopListenReady() {
    got_ready_ = false;
    input_handler_.reset(NULL);
  }

  MOCK_METHOD1(CallbackComplete, void(int));
  MOCK_METHOD1(IOComplete, void(int));

 private:
  EventDispatcher *dispatcher_;
  bool triggered_;
  int callback_count_;
  bool got_data_;
  bool got_ready_;
  scoped_ptr<Callback1<InputData*>::Type> data_callback_;
  scoped_ptr<Callback1<int>::Type> ready_callback_;
  scoped_ptr<IOHandler> input_handler_;
  ScopedRunnableMethodFactory<MockEventDispatchTester> tester_factory_;
  CancelableTask* failsafe_;
};

class ShillDaemonTest : public Test {
 public:
  ShillDaemonTest()
      : daemon_(&config_, new MockControl()),
        device_info_(daemon_.control_, dispatcher_, &daemon_.metrics_,
                     &daemon_.manager_),
        dispatcher_(&daemon_.dispatcher_),
        dispatcher_test_(dispatcher_),
        factory_(this) {
  }
  virtual ~ShillDaemonTest() {}
  virtual void SetUp() {
    // Tests initialization done by the daemon's constructor
    ASSERT_NE(reinterpret_cast<Config*>(NULL), daemon_.config_);
    ASSERT_NE(reinterpret_cast<ControlInterface*>(NULL), daemon_.control_);
    dispatcher_test_.ScheduleFailSafe();
  }
 protected:
  TestConfig config_;
  Daemon daemon_;
  DeviceInfo device_info_;
  EventDispatcher *dispatcher_;
  StrictMock<MockEventDispatchTester> dispatcher_test_;
  ScopedRunnableMethodFactory<ShillDaemonTest> factory_;
};


TEST_F(ShillDaemonTest, EventDispatcherTimer) {
  EXPECT_CALL(dispatcher_test_, CallbackComplete(Gt(0)));
  dispatcher_test_.ScheduleTimedTasks();
  dispatcher_test_.RescheduleUnlessTriggered();
  dispatcher_->DispatchForever();
}

TEST_F(ShillDaemonTest, EventDispatcherIO) {
  EXPECT_CALL(dispatcher_test_, IOComplete(16));
  int pipefd[2];
  ASSERT_EQ(pipe(pipefd), 0);

  dispatcher_test_.ListenIO(pipefd[0]);
  ASSERT_EQ(write(pipefd[1], "This is a test?!", 16), 16);

  dispatcher_->DispatchForever();
  dispatcher_test_.StopListenIO();
}

TEST_F(ShillDaemonTest, EventDispatcherReady) {
  EXPECT_CALL(dispatcher_test_, IOComplete(0))
      .Times(1);
  EXPECT_CALL(dispatcher_test_, IOComplete(1))
      .Times(1);

  int pipefd[2];
  ASSERT_EQ(pipe(pipefd), 0);

  dispatcher_test_.ListenReady(pipefd[0]);
  ASSERT_EQ(write(pipefd[1], "This is a test?!", 16), 16);

  dispatcher_->DispatchForever();
  dispatcher_test_.StopListenReady();
}

}  // namespace shill
