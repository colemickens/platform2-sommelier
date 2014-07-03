// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_IO_HANDLER_H_
#define SHILL_IO_HANDLER_H_

namespace shill {

class Error;

struct InputData {
  InputData() : buf(NULL), len(0) {}
  InputData(unsigned char *in_buf, size_t in_len) : buf(in_buf), len(in_len) {}

  unsigned char *buf;
  size_t len;
};

class IOHandler {
 public:
  enum ReadyMode {
    kModeInput,
    kModeOutput
  };

  typedef base::Callback<void(const Error &)> ErrorCallback;
  typedef base::Callback<void(InputData *)> InputCallback;
  typedef base::Callback<void(int)> ReadyCallback;

  IOHandler() {}
  virtual ~IOHandler() {}

  virtual void Start() {}
  virtual void Stop() {}
};

}  // namespace shill

#endif  // SHILL_IO_HANDLER_H_
