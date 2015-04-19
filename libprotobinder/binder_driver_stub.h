// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBPROTOBINDER_BINDER_DRIVER_STUB_H_
#define LIBPROTOBINDER_BINDER_DRIVER_STUB_H_

#include <base/macros.h>
#include <sys/types.h>
// Out of order due to this bad header requiring sys/types.h
#include <linux/android/binder.h>

#include <map>
#include <memory>

#include "binder_driver.h"  // NOLINT(build/include)
#include "binder_export.h"  // NOLINT(build/include)
#include "parcel.h"         // NOLINT(build/include)

namespace protobinder {

// Stub class that emulates the binder driver. This is used
// when unit testing libprotobinder.
class BINDER_EXPORT BinderDriverStub : public BinderDriverInterface {
 public:
  enum EndPoints {
    // Provide a valid reply.
    GOOD_ENDPOINT = 1,
    // Provides a dead end point.
    BAD_ENDPOINT = 2,
    // Returns a Status reply.
    STATUS_ENDPOINT = 3
  };
  BinderDriverStub();
  ~BinderDriverStub() override;

  int GetFdForPolling() override;
  int ReadWrite(binder_write_read* buffers) override;
  void SetMaxThreads(int max_threads) override;

  // The following methods are used by unit tests.

  // Provides access to the raw transaction data from
  // the last Transaction on the driver.
  struct binder_transaction_data* LastTransactionData();
  int GetRefCount(uint32_t handle);
  bool IsDeathRegistered(uintptr_t cookie, uint32_t handle);
  void InjectDeathNotification(uintptr_t cookie);
  void InjectTransaction(uintptr_t cookie,
                         uint32_t code,
                         const Parcel& data,
                         bool one_way);

  static const int kReplyVal;
  static const char kReplyString[];

 private:
  void ProcessTransaction(struct binder_transaction_data* tr);

  struct binder_transaction_data last_transaction_data_;
  Parcel return_cmds_;
  std::map<void*, std::unique_ptr<Parcel>> user_buffers_;
  std::map<uint32_t, int> handle_refs_;
  std::map<uintptr_t, uint32_t> death_notifications_;
  int max_threads_;

  DISALLOW_COPY_AND_ASSIGN(BinderDriverStub);
};

}  // namespace protobinder

#endif  // LIBPROTOBINDER_BINDER_DRIVER_STUB_H_
