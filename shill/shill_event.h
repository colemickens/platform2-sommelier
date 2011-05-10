// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_EVENT_
#define SHILL_EVENT_

#include <vector>

#include <base/basictypes.h>
#include <base/callback.h>
#include <base/message_loop.h>
#include <base/ref_counted.h>
#include <base/scoped_ptr.h>

namespace base {
class MessageLoopProxy;
}  // namespace base
class Task;

namespace shill {

struct InputData {
  unsigned char *buf;
  size_t len;
};

class IOInputHandler {
 public:
  IOInputHandler() {}
  virtual ~IOInputHandler() {}
};

// This is the main event dispatcher.  It contains a central instance, and
// is the entity responsible for dispatching events out of all queues to
// their listeners during the idle loop.
class EventDispatcher {
 public:
  EventDispatcher();
  virtual ~EventDispatcher();
  void DispatchForever();

  // These are thin wrappers around calls of the same name in
  // <base/message_loop_proxy.h>
  bool PostTask(Task* task);
  bool PostDelayedTask(Task* task, int64 delay_ms);

  IOInputHandler *CreateInputHandler(int fd,
                                     Callback1<InputData*>::Type *callback);
 private:
  scoped_ptr<MessageLoop> dont_use_directly_;
  scoped_refptr<base::MessageLoopProxy> message_loop_proxy_;
};

}  // namespace shill

#endif  // SHILL_EVENT_
