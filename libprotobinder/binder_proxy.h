// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBPROTOBINDER_BINDER_PROXY_H_
#define LIBPROTOBINDER_BINDER_PROXY_H_

#include <stdint.h>

#include "libprotobinder/binder_export.h"
#include "libprotobinder/ibinder.h"

namespace protobinder {

class Parcel;

// Maintanins the client side of a binder.
class BINDER_EXPORT BinderProxy : public IBinder {
 public:
  explicit BinderProxy(uint32_t handle);
  ~BinderProxy();

  uint32_t handle() const { return handle_; }

  int Transact(uint32_t code,
               Parcel* data,
               Parcel* reply,
               uint32_t flags);

  virtual const BinderProxy* GetBinderProxy() const;

 private:
  // Binder handle
  const uint32_t handle_;
};

}  // namespace protobinder

#endif  // LIBPROTOBINDER_BINDER_PROXY_H_
