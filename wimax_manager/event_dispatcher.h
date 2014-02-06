// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WIMAX_MANAGER_EVENT_DISPATCHER_H_
#define WIMAX_MANAGER_EVENT_DISPATCHER_H_

#include <base/basictypes.h>
#include <base/callback.h>
#include <base/memory/ref_counted.h>
#include <base/memory/scoped_ptr.h>
#include <base/message_loop/message_loop.h>
#include <base/time/time.h>

namespace base {

class MessageLoopProxy;

}  // namespace base

namespace wimax_manager {

class EventDispatcher {
 public:
  EventDispatcher();
  ~EventDispatcher();

  void DispatchForever();
  bool PostTask(const base::Closure &task);
  bool PostDelayedTask(const base::Closure &task,
                       const base::TimeDelta &delay);
  void Stop();

 private:
  scoped_ptr<base::MessageLoop> dont_use_directly_;
  scoped_refptr<base::MessageLoopProxy> message_loop_proxy_;

  DISALLOW_COPY_AND_ASSIGN(EventDispatcher);
};

}  // namespace wimax_manager

#endif  // WIMAX_MANAGER_EVENT_DISPATCHER_H_
