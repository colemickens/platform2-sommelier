// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APMANAGER_EVENT_DISPATCHER_H_
#define APMANAGER_EVENT_DISPATCHER_H_

#include <base/callback.h>
#include <base/lazy_instance.h>

namespace apmanager {

// Singleton class for dispatching tasks to current message loop.
class EventDispatcher {
 public:
  virtual ~EventDispatcher();

  // This is a singleton. Use EventDispatcher::GetInstance()->Foo().
  static EventDispatcher* GetInstance();

  // These are thin wrappers around calls of the same name in
  // <base/message_loop_proxy.h>
  virtual bool PostTask(const base::Closure& task);
  virtual bool PostDelayedTask(const base::Closure& task,
                               int64_t delay_ms);

 protected:
  EventDispatcher();

 private:
  friend base::LazyInstanceTraitsBase<EventDispatcher>;

  DISALLOW_COPY_AND_ASSIGN(EventDispatcher);
};

}  // namespace apmanager

#endif  // APMANAGER_EVENT_DISPATCHER_H_
