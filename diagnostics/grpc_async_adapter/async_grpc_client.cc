// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "diagnostics/grpc_async_adapter/async_grpc_client.h"

namespace diagnostics {
namespace internal {

AsyncGrpcClientBase::AsyncGrpcClientBase(
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : dispatcher_(&completion_queue_, task_runner) {
  dispatcher_.Start();
}

AsyncGrpcClientBase::~AsyncGrpcClientBase() = default;

void AsyncGrpcClientBase::Shutdown(const base::Closure& on_shutdown) {
  dispatcher_.Shutdown(on_shutdown);
}

}  // namespace internal
}  // namespace diagnostics
