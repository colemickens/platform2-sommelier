// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_ASYNC_FILE_READER_H_
#define POWER_MANAGER_ASYNC_FILE_READER_H_

#include <aio.h>
#include <unistd.h>
#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "power_manager/signal_callback.h"

typedef int gboolean;

namespace power_manager {

class AsyncFileReader {
 public:
  explicit AsyncFileReader();
  ~AsyncFileReader();

  // Read file asynchronously, passing its contents to |read_cb| when done.
  // Invokes |error_cb| on failure. If a read is already in progress, abort it
  // first.
  void StartRead(base::Callback<void(const std::string&)>* read_cb,
                 base::Callback<void()>* error_cb);

  // The file reader will open a file handle and keep it open even over
  // repeated reads.
  bool Init(const std::string& filename);

  // Indicates whether a file handle has been opened.
  bool HasOpenedFile() const;

 private:
  friend class AsyncFileReaderTest;

  // Callback used for AIO notification.
  static void SigeventCallback(union sigval);

  // Updates the state based on whether there is an ongoing file I/O.
  SIGNAL_CALLBACK_0(AsyncFileReader, gboolean, UpdateState);

  // Goes back to the idle state, cleans up allocated resouces.
  void Reset();

  // Initiates an AIO read operation.  This is a helper function for
  // StartRead().  Returns true if the AIO read was successfully enqueued.
  bool AsyncRead(int size, int offset);

  // Flag indicating whether there is an active AIO read.
  bool read_in_progress_;

  // AIO control object.
  aiocb aio_control_;

  // Name of file from which to read.
  std::string filename_;

  // File for AIO reads.
  int fd_;

  // Buffer for AIO reads.
  char* aio_buffer_;

  // Number of bytes to be read for the first chunk.  This is a variable instead
  // of a constant so unit tests can modify it.
  int initial_read_size_;

  // Accumulator for data read by AIO.
  std::string stored_data_;

  // Callbacks invoked when the read completes or encounters an error.
  base::Callback<void(const std::string&)>* read_cb_;
  base::Callback<void()>* error_cb_;

  DISALLOW_COPY_AND_ASSIGN(AsyncFileReader);
};

}  // namespace power_manager

#endif // POWER_MANAGER_ASYNC_FILE_READER_H_
