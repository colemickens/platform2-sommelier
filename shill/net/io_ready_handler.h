// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_NET_IO_READY_HANDLER_H_
#define SHILL_NET_IO_READY_HANDLER_H_

#include <base/message_loop/message_loop.h>

#include "shill/net/io_handler.h"

namespace shill {

// This handler is different from the IOInputHandler
// in that we don't read from the file handle and
// leave that to the caller.  This is useful in accept()ing
// sockets and effort to working with peripheral libraries.
class IOReadyHandler : public IOHandler,
                       public base::MessageLoopForIO::Watcher {
 public:
  IOReadyHandler(int fd, ReadyMode mode, const ReadyCallback& ready_callback);
  ~IOReadyHandler();

  void Start() override;
  void Stop() override;

 private:
  // base::MessageLoopForIO::Watcher methods.
  void OnFileCanReadWithoutBlocking(int fd) override;
  void OnFileCanWriteWithoutBlocking(int fd) override;

  int fd_;
  base::MessageLoopForIO::FileDescriptorWatcher fd_watcher_;
  ReadyMode ready_mode_;
  ReadyCallback ready_callback_;
};

}  // namespace shill

#endif  // SHILL_NET_IO_READY_HANDLER_H_
