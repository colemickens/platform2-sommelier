// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_EVENT_DISPATCHER_H_
#define SHILL_MOCK_EVENT_DISPATCHER_H_

#include <base/macros.h>
#include <gmock/gmock.h>

#include "shill/event_dispatcher.h"

namespace shill {

class MockEventDispatcher : public EventDispatcher {
 public:
  MockEventDispatcher();
  ~MockEventDispatcher() override;

  MOCK_METHOD0(DispatchForever, void());
  MOCK_METHOD0(DispatchPendingEvents, void());
  MOCK_METHOD2(PostTask, void(const tracked_objects::Location& location,
                              const base::Closure& task));
  MOCK_METHOD3(PostDelayedTask, void(const tracked_objects::Location& location,
                                     const base::Closure& task,
                                     int64_t delay_ms));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockEventDispatcher);
};

}  // namespace shill

#endif  // SHILL_MOCK_EVENT_DISPATCHER_H_
