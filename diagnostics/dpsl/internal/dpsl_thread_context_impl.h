// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DIAGNOSTICS_DPSL_INTERNAL_DPSL_THREAD_CONTEXT_IMPL_H_
#define DIAGNOSTICS_DPSL_INTERNAL_DPSL_THREAD_CONTEXT_IMPL_H_

#include <memory>

#include <base/macros.h>
#include <base/message_loop/message_loop.h>
#include <base/run_loop.h>
#include <base/sequence_checker_impl.h>
#include <base/threading/platform_thread.h>

#include "diagnostics/dpsl/public/dpsl_thread_context.h"

namespace diagnostics {

// Real implementation of the DpslThreadContext interface.
class DpslThreadContextImpl final : public DpslThreadContext {
 public:
  DpslThreadContextImpl();
  ~DpslThreadContextImpl() override;

  // DpslThreadContext overrides:
  bool BelongsToCurrentThread() override;
  void RunEventLoop() override;
  bool IsEventLoopRunning() override;
  void PostTask(std::function<void()> task) override;
  void PostDelayedTask(std::function<void()> task,
                       int64_t delay_milliseconds) override;
  void QuitEventLoop() override;

 private:
  // Identifier of the thread which is associated with this instance.
  const base::PlatformThreadId thread_id_;
  // Message loop owned by this instance. Only gets created when no previously
  // created message loop was present at construction time.
  std::unique_ptr<base::MessageLoopForIO> owned_message_loop_;
  // Message loop of the thread associated with this instance.
  base::MessageLoop* message_loop_ = nullptr;
  // The run loop which is used for the current invocation of RunEventLoop(). Is
  // null when this method is not currently run.
  base::RunLoop* current_run_loop_ = nullptr;

  base::SequenceCheckerImpl sequence_checker_;

  DISALLOW_COPY_AND_ASSIGN(DpslThreadContextImpl);
};

}  // namespace diagnostics

#endif  // DIAGNOSTICS_DPSL_INTERNAL_DPSL_THREAD_CONTEXT_IMPL_H_
