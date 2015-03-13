// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBPROTOBINDER_IINTERFACE_H_
#define LIBPROTOBINDER_IINTERFACE_H_

#include "libprotobinder/binder_export.h"
#include "libprotobinder/binder_host.h"
#include "libprotobinder/binder_proxy_interface_base.h"
#include "libprotobinder/ibinder.h"

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
  virtual IBinder* onAsBinder() { return Remote(); }
};

template <typename INTERFACE>
inline INTERFACE* BinderToInterface(IBinder* obj) {
  return INTERFACE::asInterface(obj);
}

#define DECLARE_META_INTERFACE(INTERFACE)         \
  static I##INTERFACE* asInterface(IBinder* obj); \
  I##INTERFACE();                                 \
  virtual ~I##INTERFACE();

#define IMPLEMENT_META_INTERFACE(INTERFACE, NAME)         \
  I##INTERFACE* I##INTERFACE::asInterface(IBinder* obj) { \
    I##INTERFACE* intr = NULL;                            \
    if (obj != NULL) {                                    \
      intr = new I##INTERFACE##Proxy(obj);                \
    }                                                     \
    return intr;                                          \
  }                                                       \
  I##INTERFACE::I##INTERFACE() {}                         \
  I##INTERFACE::~I##INTERFACE() {}

}  // namespace protobinder

#endif  // LIBPROTOBINDER_IINTERFACE_H_
