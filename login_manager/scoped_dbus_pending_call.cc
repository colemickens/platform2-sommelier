// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/scoped_dbus_pending_call.h"

#include <base/memory/scoped_ptr.h>
#include <dbus/dbus.h>


namespace login_manager {

ScopedDBusPendingCall::ScopedDBusPendingCall(DBusPendingCall* call,
                                             bool for_test)
    : call_(call),
      skip_unref_for_test_(for_test) {
}

// static
scoped_ptr<ScopedDBusPendingCall> ScopedDBusPendingCall::Create(
    DBusPendingCall* call) {
  return scoped_ptr<ScopedDBusPendingCall>(new ScopedDBusPendingCall(call,
                                                                     false));
}

// static
scoped_ptr<ScopedDBusPendingCall> ScopedDBusPendingCall::CreateForTesting() {
  static int test_instance_counter = 0;
  ++test_instance_counter;
  DBusPendingCall* fake_ptr_value =
      reinterpret_cast<DBusPendingCall*>(0xbeefcafe + test_instance_counter);
  return scoped_ptr<ScopedDBusPendingCall>(
      new ScopedDBusPendingCall(fake_ptr_value, true));
}

ScopedDBusPendingCall::~ScopedDBusPendingCall() {
  UnrefAndClear();
}

DBusPendingCall* ScopedDBusPendingCall::Get() {
  return call_;
}

void ScopedDBusPendingCall::UnrefAndClear() {
  if (call_ && !skip_unref_for_test_)
    dbus_pending_call_unref(call_);
  call_ = NULL;
}

}  // namespace login_manager
