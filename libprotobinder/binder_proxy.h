// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBPROTOBINDER_BINDER_PROXY_H_
#define LIBPROTOBINDER_BINDER_PROXY_H_

#include <stdint.h>

#include <base/callback.h>

#include "binder_export.h"  // NOLINT(build/include)
#include "ibinder.h"        // NOLINT(build/include)
#include "status.h"         // NOLINT(build/include)

namespace protobinder {

class Parcel;

// Maintains the client side of a binder.
class BINDER_EXPORT BinderProxy : public IBinder {
 public:
  explicit BinderProxy(uint32_t handle);
  ~BinderProxy() override;

  uint32_t handle() const { return handle_; }

  // Set a callback to be invoked when the remote (host/server) side of the
  // connection is closed.
  void SetDeathCallback(const base::Closure& closure);

  // Called by BinderManager() in response to notification that the remote side
  // of the connection has been closed.
  void HandleDeathNotification();

  // IBinder overrides:
  void CopyToProtocolBuffer(StrongBinder* proto) const override;
  Status Transact(uint32_t code,
                  Parcel* data,
                  Parcel* reply,
                  bool one_way) override;
  const BinderProxy* GetBinderProxy() const override;
  BinderProxy* GetBinderProxy() override;

 private:
  // Binder handle.
  const uint32_t handle_;

  // Called when the remote side of the connection is closed.
  base::Closure death_callback_;
};

}  // namespace protobinder

#endif  // LIBPROTOBINDER_BINDER_PROXY_H_
