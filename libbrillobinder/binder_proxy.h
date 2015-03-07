// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBBRILLOBINDER_BINDER_PROXY_H_
#define LIBBRILLOBINDER_BINDER_PROXY_H_

#include <stdint.h>

#include "ibinder.h"

#define BINDER_EXPORT __attribute__((visibility("default")))

namespace brillobinder {

class Parcel;

// Maintanins the client side of a binder.
class BINDER_EXPORT BinderProxy : public IBinder {
 public:
  BinderProxy(uint32_t handle);
  ~BinderProxy();

  int Transact(uint32_t code, Parcel& data, Parcel* reply, uint32_t flags);

  uint32_t Handle() { return handle_; }

  virtual BinderProxy* GetBinderProxy();

 private:
  // Binder handle
  const uint32_t handle_;
};
}

#endif