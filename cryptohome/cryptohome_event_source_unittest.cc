// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Unit tests for CryptohomeEventSource.

#include "vault_keyset.h"

#include <base/logging.h>
#include <chromeos/utility.h>
#include <gtest/gtest.h>

#include "cryptohome_event_source.h"

namespace cryptohome {
using std::string;

class CryptohomeEventSourceTest : public ::testing::Test {
 public:
  CryptohomeEventSourceTest() { }
  virtual ~CryptohomeEventSourceTest() { }

 private:
  DISALLOW_COPY_AND_ASSIGN(CryptohomeEventSourceTest);
};

class EventDestructorWatcher {
 public:
  virtual void NotifyDestroy(CryptohomeEventBase* event) = 0;
};

class MyEvent : public CryptohomeEventBase {
 public:
  MyEvent()
      : destructor_watcher_(NULL),
        id_(-1) { }

  virtual ~MyEvent() {
    if (destructor_watcher_) {
      destructor_watcher_->NotifyDestroy(this);
    }
  }

  void set_destructor_watcher(EventDestructorWatcher* watcher) {
    destructor_watcher_ = watcher;
  }

  void set_id(int id) {
    id_ = id;
  }

  int get_id() {
    return id_;
  }

  virtual const char* GetEventName() const {
    return "MyEvent";
  }

 private:
  EventDestructorWatcher* destructor_watcher_;
  int id_;
};

class EventSink : public CryptohomeEventSourceSink,
                  public EventDestructorWatcher {
 public:
  EventSink()
      : completed_events_() { }

  virtual ~EventSink() { }

  virtual void NotifyEvent(CryptohomeEventBase* event) {
    completed_events_.push_back(static_cast<MyEvent*>(event)->get_id());
  }

  virtual void NotifyDestroy(CryptohomeEventBase* event) {
    destroyed_events_.push_back(static_cast<MyEvent*>(event)->get_id());
  }

  std::vector<int> completed_events_;
  std::vector<int> destroyed_events_;
};

TEST_F(CryptohomeEventSourceTest, TestEventSink) {
  EventSink event_sink;
  CryptohomeEventSource event_source;
  event_source.Reset(&event_sink, NULL);

  static const int kEventCount = 4096;
  for (int i = 0; i < kEventCount; i++) {
    MyEvent* event = new MyEvent();
    event->set_id(i);
    event->set_destructor_watcher(&event_sink);
    event_source.AddEvent(event);
  }

  EXPECT_TRUE(event_source.EventsPending());
  event_source.HandleDispatch();
  EXPECT_FALSE(event_source.EventsPending());
  EXPECT_EQ(kEventCount, event_sink.completed_events_.size());
  EXPECT_EQ(kEventCount, event_sink.destroyed_events_.size());
}

TEST_F(CryptohomeEventSourceTest, TestEventSinkNoClear) {
  EventSink event_sink;

  static const int kEventCount = 4096;
  // Scope the event source
  {
    CryptohomeEventSource event_source;
    event_source.Reset(&event_sink, NULL);

    for (int i = 0; i < kEventCount; i++) {
      MyEvent* event = new MyEvent();
      event->set_id(i);
      event->set_destructor_watcher(&event_sink);
      event_source.AddEvent(event);
    }

    EXPECT_TRUE(event_source.EventsPending());
  }

  EXPECT_EQ(0, event_sink.completed_events_.size());
  EXPECT_EQ(kEventCount, event_sink.destroyed_events_.size());
}

}  // namespace cryptohome
