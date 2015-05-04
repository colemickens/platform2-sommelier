// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBPROTOBINDER_BINDER_HOST_H_
#define LIBPROTOBINDER_BINDER_HOST_H_

#include <stdint.h>

#include "binder_export.h"  // NOLINT(build/include)
#include "ibinder.h"  // NOLINT(build/include)

namespace protobinder {

class Parcel;

// Maintains the server side of a binder connection.
// Callbacks will be received on Transact when transactions
// arrive for this binder.
class BINDER_EXPORT BinderHost : public IBinder {
 public:
  BinderHost();

  // IBinder overrides:
  int Transact(uint32_t code,
               Parcel* data,
               Parcel* reply,
               bool one_way) override;
  const BinderHost* GetBinderHost() const override;

 protected:
  ~BinderHost() override;

  // Implemented by generated code and called by Transact().
  virtual int OnTransact(uint32_t code,
                         Parcel* data,
                         Parcel* reply,
                         bool one_way);
};

}  // namespace protobinder

#endif  // LIBPROTOBINDER_BINDER_HOST_H_
