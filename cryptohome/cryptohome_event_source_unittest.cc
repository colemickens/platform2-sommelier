// Copyright (c) 2009-2010 The Chromium OS Authors. All rights reserved.
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

class EventSink : public CryptohomeEventSourceSink {
 public:
  EventSink()
      : completed_events_() { }

  virtual ~EventSink() { }

  void NotifyComplete(const MountTaskResult& task_result) {
    completed_events_.push_back(task_result.sequence_id());
  }

  std::vector<int> completed_events_;
};

TEST_F(CryptohomeEventSourceTest, TestEventSink) {
  EventSink event_sink;
  CryptohomeEventSource event_source;
  event_source.Reset(&event_sink, NULL);

  for (int i = 0; i < 4096; i++) {
    MountTaskResult result;
    result.set_sequence_id(i);
    result.set_return_status(true);
    result.set_return_code(Mount::MOUNT_ERROR_NONE);
    event_source.AddEvent(result);
  }

  EXPECT_TRUE(event_source.EventsPending());
  event_source.HandleDispatch();
  EXPECT_FALSE(event_source.EventsPending());
  EXPECT_EQ(4096, event_sink.completed_events_.size());
}

} // namespace cryptohome
