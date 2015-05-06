// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBPROTOBINDER_BINDER_HOST_H_
#define LIBPROTOBINDER_BINDER_HOST_H_

#include <stdint.h>

// binder.h requires types.h to be included first.
#include <sys/types.h>
#include <linux/android/binder.h>  // NOLINT(build/include_alpha)

#include "binder_export.h"  // NOLINT(build/include)
#include "ibinder.h"        // NOLINT(build/include)
#include "status.h"         // NOLINT(build/include)

namespace protobinder {

class Parcel;

// Maintains the server side of a binder connection.
// Callbacks will be received on Transact when transactions
// arrive for this binder.
class BINDER_EXPORT BinderHost : public IBinder {
 public:
  BinderHost();

  binder_uintptr_t cookie() const { return cookie_; }

  // IBinder overrides:
  void CopyToProtocolBuffer(StrongBinder* proto) const override;
  Status Transact(uint32_t code,
                  Parcel* data,
                  Parcel* reply,
                  bool one_way) override;
  const BinderHost* GetBinderHost() const override;
  BinderHost* GetBinderHost() override;

 protected:
  ~BinderHost() override;

  // Implemented by generated code and called by Transact().
  virtual Status OnTransact(uint32_t code,
                            Parcel* data,
                            Parcel* reply,
                            bool one_way);

 private:
  // Cookie used to identify this host in transactions.
  binder_uintptr_t cookie_;
};

}  // namespace protobinder

#endif  // LIBPROTOBINDER_BINDER_HOST_H_
