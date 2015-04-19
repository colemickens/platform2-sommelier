// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBPROTOBINDER_IBINDER_H_
#define LIBPROTOBINDER_IBINDER_H_

#include <stdint.h>
#include <sys/types.h>

#include <linux/android/binder.h>

#include "binder_export.h"  // NOLINT(build/include)

namespace protobinder {

class BinderHost;
class BinderProxy;
class Parcel;
class Status;

// Wraps a binder endpoint.
// Can be the local or remote side.
class BINDER_EXPORT IBinder {
 public:
  IBinder();
  virtual ~IBinder();

  virtual Status Transact(uint32_t code,
                          Parcel* data,
                          Parcel* reply,
                          bool one_way) = 0;

  virtual const BinderHost* GetBinderHost() const;
  virtual const BinderProxy* GetBinderProxy() const;
};

}  // namespace protobinder

#endif  // LIBPROTOBINDER_IBINDER_H_
