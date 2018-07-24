// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_NET_IO_INPUT_HANDLER_H_
#define SHILL_NET_IO_INPUT_HANDLER_H_

#include <base/message_loop/message_loop.h>

#include "shill/net/io_handler.h"

namespace shill {

// Monitor file descriptor for reading.
class IOInputHandler : public IOHandler,
                       public base::MessageLoopForIO::Watcher {
 public:
  IOInputHandler(int fd,
                 const InputCallback& input_callback,
                 const ErrorCallback& error_callback);
  ~IOInputHandler();

  void Start() override;
  void Stop() override;

 private:
  // base::MessageLoopForIO::Watcher methods.
  void OnFileCanReadWithoutBlocking(int fd) override;
  void OnFileCanWriteWithoutBlocking(int fd) override;

  int fd_;
  base::MessageLoopForIO::FileDescriptorWatcher fd_watcher_;
  InputCallback input_callback_;
  ErrorCallback error_callback_;
};

}  // namespace shill

#endif  // SHILL_NET_IO_INPUT_HANDLER_H_
