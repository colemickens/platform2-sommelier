// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#ifndef CHAPS_PROXY_H
#define CHAPS_PROXY_H

#include <base/scoped_ptr.h>

#include "chaps/chaps_proxy_generated.h"
#include "chaps/chaps_proxy_interface.h"

namespace chaps {

// ChapsProxyImpl is the default implementation of the chaps proxy interface.
//
// All calls are forwarded to a dbus-c++ generated proxy object. All exceptions
// thrown by the dbus-c++ library are caught and handled in this class.
//
// The Connect() method must be called successfully before calling any other
// methods.
class ChapsProxyImpl : public ChapsProxyInterface {
public:
  ChapsProxyImpl();
  virtual ~ChapsProxyImpl();
  // ChapsProxyInterface methods.
  virtual bool Connect(const std::string& username);
  virtual void Disconnect();

private:
  // This class provides the link to the dbus-c++ generated proxy.
  class Proxy : public org::chromium::Chaps_proxy,
                public DBus::ObjectProxy {
  public:
    Proxy(DBus::Connection &connection,
          const char* path,
          const char* service) : ObjectProxy(connection, path, service) {}
    virtual ~Proxy() {}

  private:
    DISALLOW_COPY_AND_ASSIGN(Proxy);
  };

  scoped_ptr<Proxy> proxy_;
  uint32_t session_id_;

  DISALLOW_COPY_AND_ASSIGN(ChapsProxyImpl);
};

}  // namespace

#endif  // CHAPS_PROXY_H

