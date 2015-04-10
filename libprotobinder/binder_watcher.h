// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBPROTOBINDER_BINDER_WATCHER_H_
#define LIBPROTOBINDER_BINDER_WATCHER_H_

#include <base/macros.h>
#include <base/message_loop/message_loop.h>

#include "binder_export.h"  // NOLINT(build/include)

namespace protobinder {

// Adds the binder FD to the current base::MessageLoopForIO and notifies
// BinderManager when new events are available.
class BINDER_EXPORT BinderWatcher : public base::MessageLoopForIO::Watcher {
 public:
  // Crashes if binder initialization fails.
  BinderWatcher();
  ~BinderWatcher() override;

 private:
  // MessageLoopForIO::Watcher:
  void OnFileCanReadWithoutBlocking(int file_descriptor) override;
  void OnFileCanWriteWithoutBlocking(int file_descriptor) override;

  base::MessageLoopForIO::FileDescriptorWatcher fd_watcher_;

  DISALLOW_COPY_AND_ASSIGN(BinderWatcher);
};

}  // namespace protobinder

#endif  // LIBPROTOBINDER_BINDER_WATCHER_H_
