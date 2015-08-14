// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBWEAVE_INCLUDE_WEAVE_STREAM_H_
#define LIBWEAVE_INCLUDE_WEAVE_STREAM_H_

#include <string>

#include <base/callback.h>
#include <weave/error.h>

namespace weave {

class Stream {
 public:
  virtual ~Stream() = default;

  virtual bool ReadAsync(
      void* buffer,
      size_t size_to_read,
      const base::Callback<void(size_t)>& success_callback,
      const base::Callback<void(const Error*)>& error_callback,
      ErrorPtr* error) = 0;

  virtual bool WriteAllAsync(
      const void* buffer,
      size_t size_to_write,
      const base::Closure& success_callback,
      const base::Callback<void(const Error*)>& error_callback,
      ErrorPtr* error) = 0;

  virtual bool FlushBlocking(ErrorPtr* error) = 0;

  virtual bool CloseBlocking(ErrorPtr* error) = 0;

  virtual void CancelPendingAsyncOperations() = 0;
};

}  // namespace weave

#endif  // LIBWEAVE_INCLUDE_WEAVE_STREAM_H_
