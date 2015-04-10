// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libprotobinder/binder_watcher.h"

#include <base/logging.h>

#include "libprotobinder/binder_manager.h"

namespace protobinder {

BinderWatcher::BinderWatcher() {
  int binder_fd = -1;
  PCHECK(BinderManagerInterface::Get()->GetFdForPolling(&binder_fd))
      << "Failed to get binder FD";

  base::MessageLoopForIO* loop = base::MessageLoopForIO::current();
  CHECK(loop) << "base::MessageLoopForIO must be instantiated first";
  CHECK(loop->WatchFileDescriptor(binder_fd, true /* persistent */,
                                  base::MessageLoopForIO::WATCH_READ,
                                  &fd_watcher_, this))
      << "Failed to add binder FD to message loop";
}

BinderWatcher::~BinderWatcher() = default;

void BinderWatcher::OnFileCanReadWithoutBlocking(int file_descriptor) {
  BinderManagerInterface::Get()->HandleEvent();
}

void BinderWatcher::OnFileCanWriteWithoutBlocking(int file_descriptor) {
  NOTREACHED() << "Unexpected write notification for FD " << file_descriptor;
}

}  // namespace protobinder
