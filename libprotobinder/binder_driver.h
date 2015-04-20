// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBPROTOBINDER_BINDER_DRIVER_H_
#define LIBPROTOBINDER_BINDER_DRIVER_H_

#include <stdint.h>

#include <base/macros.h>

#include "binder_export.h"  // NOLINT(build/include)

struct binder_transaction_data;
struct binder_write_read;

namespace protobinder {

class Parcel;

class BINDER_EXPORT BinderDriverInterface {
 public:
  virtual ~BinderDriverInterface() = default;

  // Get a file descriptor that can used with epoll.
  virtual int GetFdForPolling() = 0;

  // Do binder BINDER_WRITE_READ command.
  virtual int ReadWrite(binder_write_read* buffers) = 0;

  // Do binder BINDER_SET_MAX_THREADS command.
  virtual void SetMaxThreads(int max_threads) = 0;
};

class BINDER_EXPORT BinderDriver : public BinderDriverInterface {
 public:
  BinderDriver();
  ~BinderDriver() override;

  int GetFdForPolling() override;
  int ReadWrite(binder_write_read* buffers) override;
  void SetMaxThreads(int max_threads) override;

  void Init();

 private:
  int binder_fd_;
  void* binder_mapped_address_;

  DISALLOW_COPY_AND_ASSIGN(BinderDriver);
};

}  // namespace protobinder

#endif  // LIBPROTOBINDER_BINDER_DRIVER_H_
