// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arc/vm/vsock_proxy/client_proxy_service.h"

#include <utility>

#include <base/bind.h>
#include <base/logging.h>
#include <base/synchronization/waitable_event.h>
#include <base/threading/thread.h>

#include "arc/vm/vsock_proxy/client_proxy.h"

namespace arc {

ClientProxyService::ClientProxyService() = default;

ClientProxyService::~ClientProxyService() {
  // This is safe, although base::Unretained(this) is used in the method,
  // because stop() is blocked until the thread where the pointer is posted
  // is joined.
  Stop();
}

bool ClientProxyService::Start() {
  LOG(INFO) << "Starting ClientProxyService...";
  base::Thread::Options options;
  options.message_loop_type = base::MessageLoop::TYPE_IO;
  auto thread = std::make_unique<base::Thread>("ClientProxy");
  if (!thread->StartWithOptions(options)) {
    LOG(ERROR) << "Failed to start a ClientProxy thread.";
    return false;
  }
  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::MANUAL,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);
  thread->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&ClientProxyService::Initialize, base::Unretained(this),
                     base::Unretained(&event)));
  event.Wait();
  thread_ = std::move(thread);
  LOG(INFO) << "ClientProxy thread is ready";
  return true;
}

void ClientProxyService::Stop() {
  LOG(INFO) << "Stopping ClientProxyService...";
  thread_->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&ClientProxyService::ShutDown, base::Unretained(this)));
  thread_.reset();
  LOG(INFO) << "ClientProxyService has been stopped.";
}

void ClientProxyService::Initialize(base::WaitableEvent* event) {
  proxy_ = std::make_unique<ClientProxy>();
  proxy_->Initialize();
  event->Signal();
}

void ClientProxyService::ShutDown() {
  proxy_.reset();
}

}  // namespace arc
