// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APMANAGER_MOCK_EVENT_DISPATCHER_H_
#define APMANAGER_MOCK_EVENT_DISPATCHER_H_

#include <base/lazy_instance.h>
#include <gmock/gmock.h>

#include "apmanager/event_dispatcher.h"

namespace apmanager {

class MockEventDispatcher : public EventDispatcher {
 public:
  ~MockEventDispatcher() override;

  // This is a singleton. Use MockEventDispatcher::GetInstance()->Foo().
  static MockEventDispatcher* GetInstance();

  MOCK_METHOD1(PostTask, bool(const base::Closure& task));
  MOCK_METHOD2(PostDelayedTask, bool(const base::Closure& task,
                                     int64_t delay_ms));

 protected:
  MockEventDispatcher();

 private:
  friend struct base::DefaultLazyInstanceTraits<MockEventDispatcher>;

  DISALLOW_COPY_AND_ASSIGN(MockEventDispatcher);
};

}  // namespace apmanager

#endif  // APMANAGER_MOCK_EVENT_DISPATCHER_H_
