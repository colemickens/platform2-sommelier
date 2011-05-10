// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <glib.h>

#include <base/logging.h>
#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "shill/shill_daemon.h"
#include "shill/dbus_control.h"

namespace shill {
using ::testing::Test;
using ::testing::_;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::SetArgumentPointee;
using ::testing::StrictMock;

class MockEventDispatchTester {
 public:
  explicit MockEventDispatchTester(EventDispatcher *dispatcher)
    : triggered_(false),
      int_callback_(new ClassCallback<MockEventDispatchTester, int>(this,
		    &MockEventDispatchTester::HandleInt)),
      int_callback_queue_(dispatcher) {
    int_callback_queue_.AddCallback(int_callback_);
    timer_source_ = g_timeout_add_seconds(1,
      &MockEventDispatchTester::CallbackFunc, this);
  }

  ~MockEventDispatchTester() {
    RemoveCallback();
    delete(int_callback_);
    g_source_remove(timer_source_);
  }

  void TimerFunction(int counter) {
    printf("Callback func called #%d\n", counter);
    if (counter > 1)
      int_callback_queue_.AddEvent(counter);
  }

  void HandleInt(int arg) {
    printf("MockEventDispatchTester handling int %d\n", arg);
    // Depending on the course of events, we may be called either once or
    // twice depending on timing.  Only call the mock routine once.
    if (!triggered_) {
      CallbackComplete(arg);
      triggered_ = true;
    }
  }

  bool GetTrigger() { return triggered_; }
  void ResetTrigger() { triggered_ = false; }
  void RemoveCallback() { int_callback_queue_.RemoveCallback(int_callback_); }

  MOCK_METHOD1(CallbackComplete, void(int));
 private:
  bool triggered_;
  Callback<int> *int_callback_;
  EventQueue<int>int_callback_queue_;
  int timer_source_;
  static gboolean CallbackFunc(gpointer data) {
    static int i = 0;
    MockEventDispatchTester *dispatcher_test =
      static_cast<MockEventDispatchTester *>(data);
    dispatcher_test->TimerFunction(i++);
    return true;
  }
};

class ShillDaemonTest : public Test {
 public:
  ShillDaemonTest()
    : daemon_(&config_, new DBusControl()),
      dispatcher_(&daemon_.dispatcher_),
      dispatcher_test_(dispatcher_) {}
  virtual void SetUp() {
    // Tests initialization done by the daemon's constructor
    EXPECT_NE((Config *)NULL, daemon_.config_);
    EXPECT_NE((ControlInterface *)NULL, daemon_.control_);
  }
protected:
  Config config_;
  Daemon daemon_;
  EventDispatcher *dispatcher_;
  StrictMock<MockEventDispatchTester> dispatcher_test_;
};


TEST_F(ShillDaemonTest, EventDispatcher) {
  EXPECT_CALL(dispatcher_test_, CallbackComplete(2));

  // Crank the glib main loop a few times until the event handler triggers
  for (int main_loop_count = 0;
       main_loop_count < 6 && !dispatcher_test_.GetTrigger(); ++main_loop_count)
    g_main_context_iteration(NULL, TRUE);

  EXPECT_EQ(dispatcher_test_.GetTrigger(), true);

  dispatcher_test_.ResetTrigger();
  dispatcher_test_.RemoveCallback();
  
  // Crank the glib main loop a few more times, ensuring there is no trigger
  for (int main_loop_count = 0;
       main_loop_count < 6 && !dispatcher_test_.GetTrigger(); ++main_loop_count)
    g_main_context_iteration(NULL, TRUE);
}

}
