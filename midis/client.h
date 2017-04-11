// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MIDIS_CLIENT_H_
#define MIDIS_CLIENT_H_

#include <memory>

#include <base/files/scoped_file.h>
#include <base/memory/weak_ptr.h>
#include <brillo/message_loops/message_loop.h>

namespace midis {

class Client {
 public:
  ~Client();
  static std::unique_ptr<Client> Create(base::ScopedFD fd);

 private:
  explicit Client(base::ScopedFD fd);
  // Start monitoring the client socket fd for messages from the client.
  bool StartMonitoring();
  // Stop the task which was watching the client socket df.
  void StopMonitoring();
  void HandleClientMessages();

  base::ScopedFD client_fd_;
  brillo::MessageLoop::TaskId msg_taskid_;
  base::WeakPtrFactory<Client> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(Client);
};

}  // namespace midis

#endif  // MIDIS_CLIENT_H_
