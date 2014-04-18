// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_DBUS_ASYNC_CALL_HELPER_H_
#define SHILL_DBUS_ASYNC_CALL_HELPER_H_

#include "shill/logging.h"

#include <dbus-c++/error.h>

#include "shill/error.h"

namespace shill {

// The dbus-c++ async call mechanism has a funny way of handling the
// callback parameter. In particular, the caller is responsible for
// cleaning up the callback. (This is unlike the low-level D-Bus
// library, which accepts a function pointer for the cleanup
// function.)
//
// In cases where the call completes asynchronously, we delete the
// callback in the return-handling code. However, if the call
// generates a synchronous error, we need to delete the callback
// immediately.
//
// This template simply factors out that pattern, so that we don't
// need to repeat the code in every async stub.

template<
    typename TraceMsgT, typename ProxyT, typename CallT,
    typename CallbackT, typename... ArgTypes>
void BeginAsyncDBusCall(const TraceMsgT &trace_msg, ProxyT &proxy,
                        const CallT &call, const CallbackT &callback,
                        Error *error,
                        void(*error_converter)(const DBus::Error &, Error *),
                        int timeout, ArgTypes... call_args) {
  SLOG(DBus, 2) << trace_msg;
  auto cb = make_scoped_ptr(new CallbackT(callback));
  try {
    (proxy.*call)(call_args..., cb.get(), timeout);
    cb.release();
  } catch (const DBus::Error &e) {
    if (error)
      error_converter(e, error);
  }
}

}  // namespace shill

#endif  // SHILL_DBUS_ASYNC_CALL_HELPER_H_
