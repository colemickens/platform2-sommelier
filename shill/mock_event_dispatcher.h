// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
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
  MOCK_METHOD1(PostTask, bool(const base::Closure& task));
  MOCK_METHOD2(PostDelayedTask, bool(const base::Closure& task,
                                     int64_t delay_ms));
  MOCK_METHOD3(CreateInputHandler, IOHandler*(
      int fd,
      const IOHandler::InputCallback& input_callback,
      const IOHandler::ErrorCallback& error_callback));

  MOCK_METHOD3(CreateReadyHandler,
               IOHandler*(int fd,
                          IOHandler::ReadyMode mode,
                          const base::Callback<void(int)>& callback));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockEventDispatcher);
};

}  // namespace shill

#endif  // SHILL_MOCK_EVENT_DISPATCHER_H_
