// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_NET_IO_HANDLER_H_
#define SHILL_NET_IO_HANDLER_H_

#include <string>

#include <base/callback.h>

#include "shill/net/shill_export.h"

namespace shill {

struct SHILL_EXPORT InputData {
  InputData() : buf(nullptr), len(0) {}
  InputData(unsigned char* in_buf, size_t in_len) : buf(in_buf), len(in_len) {}

  unsigned char* buf;
  size_t len;
};

class SHILL_EXPORT IOHandler {
 public:
  enum ReadyMode {
    kModeInput,
    kModeOutput
  };

  typedef base::Callback<void(const std::string&)> ErrorCallback;
  typedef base::Callback<void(InputData*)> InputCallback;
  typedef base::Callback<void(int)> ReadyCallback;

  // Data buffer size in bytes.
  static const int kDataBufferSize = 4096;

  IOHandler() {}
  virtual ~IOHandler() {}

  virtual void Start() {}
  virtual void Stop() {}
};

}  // namespace shill

#endif  // SHILL_NET_IO_HANDLER_H_
