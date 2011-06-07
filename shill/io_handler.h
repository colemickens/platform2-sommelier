// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_IO_HANDLER_
#define SHILL_IO_HANDLER_

namespace shill {

struct InputData {
  unsigned char *buf;
  size_t len;
};

class IOInputHandler {
 public:
  IOInputHandler() {}
  virtual ~IOInputHandler() {}
};

}  // namespace shill

#endif  // SHILL_IO_HANDLER_
