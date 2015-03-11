// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>
#include <string>

#include <base/logging.h>

#include "libprotobinder/binder_manager.h"
#include "libprotobinder/binder_proxy.h"
#include "libprotobinder/iinterface.h"
#include "libprotobinder/iservice_manager.h"
#include "libprotobinder/parcel.h"

namespace protobinder {

class ITest : public IInterface {
 public:
  enum { ALERT = IBinder::FIRST_CALL_TRANSACTION };
  // Sends a user-provided value to the service
  virtual void alert() = 0;

  DECLARE_META_INTERFACE(Test)
};

class ITestProxy : public BinderProxyInterface<ITest> {
 public:
  explicit ITestProxy(IBinder* impl) : BinderProxyInterface<ITest>(impl) {}

  void alert() override {
    Parcel data, reply;
    data.WriteInt32(200);
    int ret = Remote()->Transact(ALERT, data, &reply, 0);
    LOG(INFO) << "ret " << ret;
  }
};

IMPLEMENT_META_INTERFACE(Test, "Test")

int TestBinder() {
  LOG(INFO) << "Ping client";

  IBinder* proxy = GetServiceManager()->GetService("ping");
  ITest* test = BinderToInterface<ITest>(proxy);
  test->alert();
  return 0;
}

}  // namespace protobinder

int main() {
  protobinder::TestBinder();
  return 0;
}
