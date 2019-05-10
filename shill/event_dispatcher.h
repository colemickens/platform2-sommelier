// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_EVENT_DISPATCHER_H_
#define SHILL_EVENT_DISPATCHER_H_

#include <base/callback.h>
#include <base/macros.h>
#include <base/memory/ref_counted.h>
#include <base/message_loop/message_loop.h>
#include <base/tracked_objects.h>

#include "shill/net/io_handler_factory_container.h"

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
  virtual void PostTask(const tracked_objects::Location& location,
                        const base::Closure& task);
  virtual void PostDelayedTask(const tracked_objects::Location& location,
                               const base::Closure& task, int64_t delay_ms);

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
