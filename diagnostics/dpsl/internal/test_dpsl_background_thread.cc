// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "diagnostics/dpsl/internal/test_dpsl_background_thread.h"

#include <base/bind.h>
#include <base/location.h>
#include <base/logging.h>
#include "base/memory/scoped_refptr.h"
#include <base/run_loop.h>
#include <base/task_runner.h>
#include <base/threading/thread_task_runner_handle.h>

#include "diagnostics/dpsl/public/dpsl_global_context.h"
#include "diagnostics/dpsl/public/dpsl_thread_context.h"

namespace diagnostics {

// static
std::function<void()> TestDpslBackgroundThread::WrapTaskToReplyOnMainThread(
    const base::Closure& background_callback,
    DpslThreadContext* main_thread_context,
    const base::Closure& main_thread_callback) {
  DCHECK(main_thread_context);
  DCHECK(!main_thread_callback.is_null());

  return [background_callback, main_thread_context, main_thread_callback]() {
    if (!background_callback.is_null()) {
      background_callback.Run();
    }
    main_thread_context->PostTask(
        [main_thread_callback]() { main_thread_callback.Run(); });
  };
}

TestDpslBackgroundThread::TestDpslBackgroundThread(
    const std::string& name,
    DpslGlobalContext* global_context,
    DpslThreadContext* main_thread_context)
    : global_context_(global_context),
      main_thread_context_(main_thread_context),
      run_event_loop_event_(base::WaitableEvent::ResetPolicy::MANUAL,
                            base::WaitableEvent::InitialState::NOT_SIGNALED),
      thread_(this, name) {
  DCHECK(global_context);
  DCHECK(main_thread_context_);
  DCHECK(main_thread_context_->BelongsToCurrentThread());

  base::RunLoop run_loop;

  on_thread_context_ready_ = WrapTaskToReplyOnMainThread(
      base::Closure(), main_thread_context_, run_loop.QuitClosure());

  thread_.Start();

  run_loop.Run();
}

TestDpslBackgroundThread::~TestDpslBackgroundThread() {
  DCHECK(main_thread_context_->BelongsToCurrentThread());

  run_event_loop_event_.Signal();

  DoSync(base::Bind(
      [](DpslThreadContext* background_thread_context) {
        background_thread_context->QuitEventLoop();
      },
      thread_context_.get()));
  thread_.Join();
}

void TestDpslBackgroundThread::StartEventLoop() {
  DCHECK(!run_event_loop_event_.IsSignaled());
  run_event_loop_event_.Signal();
}

void TestDpslBackgroundThread::DoSync(
    const base::Closure& background_callback) {
  DCHECK(run_event_loop_event_.IsSignaled());
  DCHECK(main_thread_context_->BelongsToCurrentThread());
  DCHECK(thread_context_);

  base::RunLoop run_loop;
  thread_context_->PostTask(WrapTaskToReplyOnMainThread(
      background_callback, main_thread_context_, run_loop.QuitClosure()));
  run_loop.Run();
}

DpslThreadContext* TestDpslBackgroundThread::thread_context() {
  DCHECK(main_thread_context_->BelongsToCurrentThread());
  return thread_context_.get();
}

void TestDpslBackgroundThread::Run() {
  thread_context_ = DpslThreadContext::Create(global_context_);
  DCHECK(thread_context_);

  on_thread_context_ready_();

  run_event_loop_event_.Wait();

  thread_context_->RunEventLoop();

  thread_context_.reset();
}

}  // namespace diagnostics
