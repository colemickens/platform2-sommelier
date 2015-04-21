// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBPROTOBINDER_IINTERFACE_H_
#define LIBPROTOBINDER_IINTERFACE_H_

#include "binder_export.h"  // NOLINT(build/include)
#include "binder_host.h"  // NOLINT(build/include)
#include "binder_manager.h"  // NOLINT(build/include)
#include "binder_proxy_interface_base.h"  // NOLINT(build/include)
#include "ibinder.h"  // NOLINT(build/include)

namespace protobinder {

// AIDL class inherits from this.
// Just need some basic stuff to return a binder.
// Mainly interface holder to force both sides to implement the methods.
class BINDER_EXPORT IInterface {
 public:
  IInterface() {}
  ~IInterface() {}
};

template <typename INTERFACE>
class BINDER_EXPORT BinderHostInterface : public INTERFACE, public BinderHost {
 public:
 protected:
  virtual ~BinderHostInterface() {}
};

template <typename INTERFACE>
class BINDER_EXPORT BinderProxyInterface : public INTERFACE,
                                           public BinderProxyInterfaceBase {
 public:
  explicit BinderProxyInterface(IBinder* remote)
      : BinderProxyInterfaceBase(remote) {}

 protected:
  virtual ~BinderProxyInterface() {}
};

template <typename INTERFACE>
inline INTERFACE* BinderToInterface(IBinder* obj) {
  IInterface* test_interface =
      BinderManagerInterface::Get()->CreateTestInterface(obj);
  if (test_interface)
    return static_cast<INTERFACE*>(test_interface);
  return INTERFACE::CreateInterface(obj);
}

#define DECLARE_META_INTERFACE(INTERFACE)                                      \
  static I##INTERFACE* CreateInterface(IBinder* obj);                          \
  I##INTERFACE();                                                              \
  virtual ~I##INTERFACE();

#define IMPLEMENT_META_INTERFACE(INTERFACE, NAME)                              \
  I##INTERFACE* I##INTERFACE::CreateInterface(IBinder* obj) {                  \
    return obj ? new I##INTERFACE##Proxy(obj) : nullptr;                       \
  }                                                                            \
  I##INTERFACE::I##INTERFACE() {}                                              \
  I##INTERFACE::~I##INTERFACE() {}

}  // namespace protobinder

#endif  // LIBPROTOBINDER_IINTERFACE_H_
