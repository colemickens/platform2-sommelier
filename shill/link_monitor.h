// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_LINK_MONITOR_
#define SHILL_LINK_MONITOR_

#include <base/callback.h>
#include <base/memory/scoped_ptr.h>

#include "shill/refptr_types.h"

namespace shill {

class EventDispatcher;
class Sockets;

// LinkMonitor tracks the status of a connection by sending ARP
// messages to the default gateway for a connection.  It keeps
// a log of response times which can show a trend in link quality.
// It signals to caller that the link has failed if too many
// requests go unanswered.
class LinkMonitor {
 public:
  typedef base::Callback<void()> FailureCallback;

  LinkMonitor(const ConnectionRefPtr &connection,
              EventDispatcher *dispatcher,
              const FailureCallback &failure_callback);
  virtual ~LinkMonitor();

  bool Start();
  void Stop();

 private:
  ConnectionRefPtr connection_;
  EventDispatcher *dispatcher_;
  FailureCallback failure_callback_;
  scoped_ptr<Sockets> sockets_;

  DISALLOW_COPY_AND_ASSIGN(LinkMonitor);
};

}  // namespace shill

#endif  // SHILL_LINK_MONITOR_
