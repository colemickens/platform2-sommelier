// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_EVENT_DISPATCHER_
#define SHILL_EVENT_DISPATCHER_

#include <base/basictypes.h>
#include <base/callback_old.h>
#include <base/memory/ref_counted.h>
#include <base/memory/scoped_ptr.h>
#include <base/message_loop.h>

#include "shill/io_handler.h"

namespace base {
class MessageLoopProxy;
}  // namespace base

class Task;

namespace shill {

// This is the main event dispatcher.  It contains a central instance, and is
// the entity responsible for dispatching events out of all queues to their
// listeners during the idle loop.
class EventDispatcher {
 public:
  EventDispatcher();
  virtual ~EventDispatcher();

  virtual void DispatchForever();

  // Processes all pending events that can run and returns.
  virtual void DispatchPendingEvents();

  // These are thin wrappers around calls of the same name in
  // <base/message_loop_proxy.h>
  virtual bool PostTask(Task *task);
  virtual bool PostDelayedTask(Task *task, int64 delay_ms);

  virtual IOHandler *CreateInputHandler(
      int fd,
      Callback1<InputData *>::Type *callback);

  virtual IOHandler *CreateReadyHandler(
      int fd,
      IOHandler::ReadyMode mode,
      Callback1<int>::Type *callback);

 private:
  scoped_ptr<MessageLoop> dont_use_directly_;
  scoped_refptr<base::MessageLoopProxy> message_loop_proxy_;

  DISALLOW_COPY_AND_ASSIGN(EventDispatcher);
};

}  // namespace shill

#endif  // SHILL_EVENT_DISPATCHER_
