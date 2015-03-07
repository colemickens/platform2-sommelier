// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBBRILLOBINDER_BINDER_HOST_H_
#define LIBBRILLOBINDER_BINDER_HOST_H_

#include <stdint.h>

#include "ibinder.h"

#define BINDER_EXPORT __attribute__((visibility("default")))

namespace brillobinder {

class Parcel;

// Maintains the server side of a binder connection.
// Callbacks will be received on Transact when transactions
// arrive for this binder.
class BINDER_EXPORT BinderHost : public IBinder {
 public:
  BinderHost();

  virtual int Transact(uint32_t code,
                       Parcel& data,
                       Parcel* reply,
                       uint32_t flags);
  virtual BinderHost* GetBinderHost();

 protected:
  virtual ~BinderHost();
  virtual int OnTransact(uint32_t code,
                         Parcel& data,
                         Parcel* reply,
                         uint32_t flags);
};

}  // namespace brillobinder

#endif