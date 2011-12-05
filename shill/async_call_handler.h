// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_ASYNC_CALL_HANDLER_
#define SHILL_ASYNC_CALL_HANDLER_

#include <base/basictypes.h>

namespace shill {

class Error;
class ReturnerInterface;

// TODO(ers): An AsyncCallHandler object for an in-flight operation can leak if
// an ObjectProxy disappears before the object is deleted. This can probably
// be handled by changing dbus-c++ to allow registering a user-supplied
// 'free' function for the 'data' argument passed to proxy methods. In that
// case, the objects will be freed when pending dbus calls are canceled.
//
// The same issue appears to be relevant to Returner objects as well.


// This class implements reply handling for code that makes asynchronous
// client calls. The default behavior, embodied in this class, is to
// return a result or error to the pending adaptor method invocation,
// if any. This behavior may be overridden or extended by specializations
// of this class.
//
// <external-client> --- [method call] ---> shill adaptor
class AsyncCallHandler {
 public:
  AsyncCallHandler();
  explicit AsyncCallHandler(ReturnerInterface *returner);
  virtual ~AsyncCallHandler() { }

  // Signal successful completion of the handling of a reply to a
  // single asynchronous client operation. Returns true if a terminal
  // state has been reached, i.e., the AsyncCallHandler is no longer
  // needed. The base class always returns true.
  bool Complete();
  // Signal completion of the handling of a reply to a single
  // asynchronous client operation which resulted in an error.
  // Returns true if a terminal state has been reached, i.e.,
  // the AsyncCallHandler is no longer needed. The base class always
  // returns true.
  bool Complete(const Error &error);

 protected:
  ReturnerInterface *returner() const { return returner_; }

  virtual bool CompleteOperation();
  virtual bool CompleteOperationWithError(const Error &error);

  void DoReturn();

 private:
  // scoped_ptr not used here because Returners delete themselves
  ReturnerInterface *returner_;

  DISALLOW_COPY_AND_ASSIGN(AsyncCallHandler);
};

}  // namespace shill

#endif  // SHILL_ASYNC_CALL_HANDLER_
