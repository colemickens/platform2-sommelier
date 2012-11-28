// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOGIN_MANAGER_SCOPED_DBUS_PENDING_CALL_H_
#define LOGIN_MANAGER_SCOPED_DBUS_PENDING_CALL_H_

#include <base/basictypes.h>
#include <base/memory/scoped_ptr.h>

struct DBusPendingCall;

namespace login_manager {

// RAII wrapper around DBusPendingCall.
// dbus_pending_call_unref()s the wrapped struct on destruction.
class ScopedDBusPendingCall {
 public:
  virtual ~ScopedDBusPendingCall();

  // Takes ownership of |call| upon creation.
  static scoped_ptr<ScopedDBusPendingCall> Create(DBusPendingCall* call);

  // DBusPendingCall* wrapped in objects created for testing contain bogus
  // pointers.
  static scoped_ptr<ScopedDBusPendingCall> CreateForTesting();

  DBusPendingCall* Get();

 private:
  ScopedDBusPendingCall(DBusPendingCall* call, bool for_test);

  void UnrefAndClear();

  DBusPendingCall* call_;
  const bool skip_unref_for_test_;
  DISALLOW_COPY_AND_ASSIGN(ScopedDBusPendingCall);
};

}  // namespace login_manager

#endif  // LOGIN_MANAGER_SCOPED_DBUS_PENDING_CALL_H_
