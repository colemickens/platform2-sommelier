// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_EVENT_DISPATCHER_H_
#define SHILL_MOCK_EVENT_DISPATCHER_H_

#include <base/basictypes.h>
#include <gmock/gmock.h>

#include "shill/event_dispatcher.h"

namespace shill {

class MockEventDispatcher : public EventDispatcher {
 public:
  MockEventDispatcher();
  virtual ~MockEventDispatcher();

  MOCK_METHOD0(DispatchForever, void());
  MOCK_METHOD0(DispatchPendingEvents, void());
  MOCK_METHOD1(PostTask, bool(Task *task));
  MOCK_METHOD2(PostDelayedTask, bool(Task *task, int64 delay_ms));
  MOCK_METHOD2(CreateInputHandler,
               IOHandler *(int fd, Callback1<InputData *>::Type *callback));

  MOCK_METHOD3(CreateReadyHandler,
               IOHandler *(int fd,
                           IOHandler::ReadyMode mode,
                           Callback1<int>::Type *callback));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockEventDispatcher);
};

}  // namespace shill

#endif  // SHILL_MOCK_EVENT_DISPATCHER_H_
