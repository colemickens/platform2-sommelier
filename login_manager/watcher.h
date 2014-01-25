// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOGIN_MANAGER_WATCHER_H_
#define LOGIN_MANAGER_WATCHER_H_

namespace login_manager {

// Copied from base/watcher.h
// Once migrating to MessageLoopForIO is complete, this interface will be used
// with WatchFileDescriptor to asynchronously monitor the I/O readiness
// of a file descriptor.
class Watcher {
 public:
  // Called when an FD can be read from/written to without blocking.
  virtual void OnFileCanReadWithoutBlocking(int fd) = 0;
  virtual void OnFileCanWriteWithoutBlocking(int fd) = 0;

 protected:
  virtual ~Watcher() {}
};

}  // namespace login_manager

#endif  // LOGIN_MANAGER_WATCHER_H_
