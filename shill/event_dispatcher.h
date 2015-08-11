// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_EVENT_DISPATCHER_H_
#define SHILL_EVENT_DISPATCHER_H_

#include <memory>

#include <base/callback.h>
#include <base/macros.h>
#include <base/memory/ref_counted.h>
#include <base/message_loop/message_loop.h>

#include "shill/net/io_handler_factory_container.h"

namespace base {
class MessageLoopProxy;
}  // namespace base


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
  virtual bool PostTask(const base::Closure& task);
  virtual bool PostDelayedTask(const base::Closure& task, int64_t delay_ms);

  virtual IOHandler* CreateInputHandler(
      int fd,
      const IOHandler::InputCallback& input_callback,
      const IOHandler::ErrorCallback& error_callback);

  virtual IOHandler* CreateReadyHandler(
      int fd,
      IOHandler::ReadyMode mode,
      const IOHandler::ReadyCallback& ready_callback);

 private:
  IOHandlerFactory* io_handler_factory_;

  DISALLOW_COPY_AND_ASSIGN(EventDispatcher);
};

}  // namespace shill

#endif  // SHILL_EVENT_DISPATCHER_H_
