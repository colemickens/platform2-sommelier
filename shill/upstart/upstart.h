// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_UPSTART_UPSTART_H_
#define SHILL_UPSTART_UPSTART_H_

#include <memory>

#include <base/macros.h>

#include "shill/upstart/upstart_proxy_interface.h"

namespace shill {

class ProxyFactory;

class Upstart {
 public:
  // |proxy_factory| creates the UpstartProxy.  Usually this is
  // ProxyFactory::GetInstance().  Use a fake for testing.
  explicit Upstart(ProxyFactory *proxy_factory);
  virtual ~Upstart();

  // Report an event to upstart indicating that the system has disconnected.
  virtual void NotifyDisconnected();

 private:
  // Event string to be provided to upstart to indicate we have disconnected.
  static const char kShillDisconnectEvent[];

  // The upstart proxy created by this class.
  const std::unique_ptr<UpstartProxyInterface> upstart_proxy_;

  DISALLOW_COPY_AND_ASSIGN(Upstart);
};

}  // namespace shill

#endif  // SHILL_UPSTART_UPSTART_H_
