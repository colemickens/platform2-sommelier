// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_IO_HANDLER_
#define SHILL_IO_HANDLER_

namespace shill {

struct InputData {
  InputData() : buf(NULL), len(0) {}
  InputData(unsigned char *in_buf, size_t in_len) : buf(in_buf), len(in_len) {}

  unsigned char *buf;
  size_t len;
};

class IOHandler {
 public:
  IOHandler() {}
  virtual ~IOHandler() {}
};

}  // namespace shill

#endif  // SHILL_IO_HANDLER_
