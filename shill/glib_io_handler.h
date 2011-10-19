// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_GLIB_IO_HANDLER_
#define SHILL_GLIB_IO_HANDLER_

#include <stdio.h>
#include <glib.h>

#include <base/callback_old.h>

#include "shill/io_handler.h"

namespace shill {

class GlibIOInputHandler : public IOHandler {
 public:
  GlibIOInputHandler(int fd, Callback1<InputData*>::Type *callback);
  ~GlibIOInputHandler();
  Callback1<InputData *>::Type *callback() { return callback_; }

 private:
  GIOChannel *channel_;
  Callback1<InputData *>::Type *callback_;
  guint source_id_;

};


}  // namespace shill

#endif  // SHILL_GLIB_IO_HANDLER_
