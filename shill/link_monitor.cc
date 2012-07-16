// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/link_monitor.h"

#include <base/logging.h>

#include "shill/connection.h"
#include "shill/event_dispatcher.h"
#include "shill/sockets.h"

namespace shill {

LinkMonitor::LinkMonitor(const ConnectionRefPtr &connection,
                         EventDispatcher *dispatcher,
                         const FailureCallback &failure_callback)
    : connection_(connection),
      dispatcher_(dispatcher),
      failure_callback_(failure_callback),
      sockets_(new Sockets()) {}

LinkMonitor::~LinkMonitor() {}

bool LinkMonitor::Start() {
  NOTIMPLEMENTED();
  return false;
}

void LinkMonitor::Stop() {
  NOTIMPLEMENTED();
}

}  // namespace shill
