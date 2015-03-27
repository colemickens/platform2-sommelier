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

// Wraps a binder endpoint.
// Can be the local or remote side.
class BINDER_EXPORT IBinder {
 public:
  enum {
    FIRST_CALL_TRANSACTION = 0x00000001,
    LAST_CALL_TRANSACTION = 0x00ffffff,

    // These are used by Android
    PING_TRANSACTION = B_PACK_CHARS('_', 'P', 'N', 'G'),
    DUMP_TRANSACTION = B_PACK_CHARS('_', 'D', 'M', 'P'),
    INTERFACE_TRANSACTION = B_PACK_CHARS('_', 'N', 'T', 'F'),
    SYSPROPS_TRANSACTION = B_PACK_CHARS('_', 'S', 'P', 'R'),

    FLAG_ONEWAY = 0x00000001
  };

  IBinder();
  virtual ~IBinder();

  virtual int Transact(uint32_t code,
                       Parcel* data,
                       Parcel* reply,
                       uint32_t flags) = 0;

  virtual const BinderHost* GetBinderHost() const;
  virtual const BinderProxy* GetBinderProxy() const;
};

}  // namespace protobinder

#endif  // LIBPROTOBINDER_IBINDER_H_
