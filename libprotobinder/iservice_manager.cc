// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libprotobinder/iservice_manager.h"

#include "libprotobinder/binder_proxy.h"
#include "libprotobinder/parcel.h"
#include "libprotobinder/protobinder.h"

namespace protobinder {

IServiceManager* g_service_manager = nullptr;

IServiceManager* GetServiceManager() {
  if (g_service_manager)
    return g_service_manager;

  g_service_manager = BinderToInterface<IServiceManager>(new BinderProxy(0));

  return g_service_manager;
}

class IServiceManagerProxy : public BinderProxyInterface<IServiceManager> {
 public:
  explicit IServiceManagerProxy(IBinder* impl)
      : BinderProxyInterface<IServiceManager>(impl) {}

  virtual int AddService(const char* name, IBinder* binder) {
    Parcel data, reply;
    data.WriteInt32(0);
    data.WriteString16("android.os.IServiceManager");
    data.WriteString16(name);
    data.WriteStrongBinder(binder);
    return Remote()->Transact(ADD_SERVICE_TRANSACTION, &data, &reply, 0);
  }

  virtual IBinder* GetService(const char* name) {
    Parcel data, reply;
    data.WriteInt32(0);
    data.WriteString16("android.os.IServiceManager");
    data.WriteString16(name);
    if (Remote()->Transact(CHECK_SERVICE_TRANSACTION, &data, &reply, 0) !=
        SUCCESS)
      return nullptr;

    IBinder* binder = nullptr;
    return reply.ReadStrongBinder(&binder) ? binder : nullptr;
  }
};

IMPLEMENT_META_INTERFACE(ServiceManager, "Test")

}  // namespace protobinder
