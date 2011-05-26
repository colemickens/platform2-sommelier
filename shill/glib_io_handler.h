// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_GLIB_IO_HANDLER_
#define SHILL_GLIB_IO_HANDLER_

#include <stdio.h>
#include <glib.h>

#include <base/callback_old.h>

#include "shill/io_handler.h"

namespace shill {

class GlibIOInputHandler : public IOInputHandler {
 public:
  GIOChannel *channel_;
  Callback1<InputData *>::Type *callback_;
  guint source_id_;

  GlibIOInputHandler(int fd, Callback1<InputData*>::Type *callback);
  ~GlibIOInputHandler();
};


}  // namespace shill

#endif  // SHILL_GLIB_IO_HANDLER_
