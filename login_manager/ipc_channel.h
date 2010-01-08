// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOGIN_MANAGER_IPC_CHANNEL_H_
#define LOGIN_MANAGER_IPC_CHANNEL_H_

#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <string>

#include <base/basictypes.h>
#include "ipc_message.h"

// Given a named pipe, this allows one to read or write one message at a time.
// Read and write operations are blocking.
// TODO(cmasone): This will be deprecated in favor of a DBus-based mechanism
// once we begin integrating Chrome with cryptohomed.

namespace login_manager {
class IpcChannel {
 public:
  IpcChannel() : pipe_(NULL) {}
  virtual ~IpcChannel() { shutdown(); }

  virtual bool init() = 0;
  virtual void shutdown() {
    if (pipe_) {
      fclose(pipe_);
      pipe_ = NULL;
    }
  }

 protected:
  bool DoInit(const char pipe_name[], const char mode[], uid_t uid);
  FILE* pipe() { return pipe_; }

 private:
  FILE* pipe_;
};

class IpcReadChannel : public IpcChannel {
 public:
  explicit IpcReadChannel(const std::string& pipe_name);
  IpcReadChannel(const std::string& pipe_name, const uid_t uid);
  virtual ~IpcReadChannel();

  bool init() { return DoInit(pipe_name_.c_str(), "r", uid_); }

  IpcMessage recv() {
    IpcMessage incoming;
    clearerr(pipe());
    size_t data_read = fread(&incoming, sizeof(IpcMessage), 1, pipe());
    return (data_read ? incoming : FAILED);
  }
  bool channel_eof() { return feof(pipe()) != 0; }
  const char* channel_error() { return strerror(ferror(pipe())); }

 private:
  const std::string pipe_name_;
  const uid_t uid_;

  DISALLOW_COPY_AND_ASSIGN(IpcReadChannel);
};

class IpcWriteChannel : public IpcChannel {
 public:
  explicit IpcWriteChannel(const std::string& pipe_name);
  IpcWriteChannel(const std::string& pipe_name, const uid_t uid);
  virtual ~IpcWriteChannel();

  bool init() { return DoInit(pipe_name_.c_str(), "w", uid_); }

  bool send(IpcMessage outgoing) {
    if (pipe()) {
      if (fwrite(&outgoing, sizeof(IpcMessage), 1, pipe())) {
        // since we're only writing 1 "member", fwrite will return 1 on success
        // and 0 on failure.  On success, we want to flush the buffer.
        return fflush(pipe()) != EOF;
      }
    }
    return false;
  }

 private:
  const std::string pipe_name_;
  const uid_t uid_;

  DISALLOW_COPY_AND_ASSIGN(IpcWriteChannel);
};
}  // namespace login_manager

#endif  // LOGIN_MANAGER_IPC_CHANNEL_H_
