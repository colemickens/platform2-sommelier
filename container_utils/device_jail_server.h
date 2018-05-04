// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTAINER_UTILS_DEVICE_JAIL_SERVER_H_
#define CONTAINER_UTILS_DEVICE_JAIL_SERVER_H_

#include <linux/device_jail.h>

#include <memory>
#include <string>
#include <utility>

#include <base/files/scoped_file.h>
#include <base/logging.h>
#include <base/macros.h>
#include <base/message_loop/message_loop.h>

namespace device_jail {

class DeviceJailServer : base::MessageLoopForIO::Watcher {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {}

    // Called when a jail request is received.
    virtual jail_request_result HandleRequest(const std::string& path) = 0;
  };

  static std::unique_ptr<DeviceJailServer> CreateAndListen(
      std::unique_ptr<Delegate> delegate,
      base::MessageLoopForIO* message_loop);
  virtual ~DeviceJailServer();

  // MessageLoopForIO::Watcher overrides
  void OnFileCanReadWithoutBlocking(int fd) override;
  void OnFileCanWriteWithoutBlocking(int fd) override {
    NOTREACHED();
  }

 private:
  DeviceJailServer(std::unique_ptr<Delegate> delegate, int fd);
  void Start(base::MessageLoopForIO* message_loop);

  std::unique_ptr<Delegate> delegate_;

  base::ScopedFD fd_;
  base::MessageLoopForIO::FileDescriptorWatcher watcher_;

  DISALLOW_COPY_AND_ASSIGN(DeviceJailServer);
};

}  // namespace device_jail

#endif  // CONTAINER_UTILS_DEVICE_JAIL_SERVER_H_
