// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_GLIB_IO_INPUT_HANDLER_
#define SHILL_GLIB_IO_INPUT_HANDLER_

#include <stdio.h>
#include <glib.h>

#include <base/callback.h>

#include "shill/io_handler.h"

namespace shill {

class GlibIOInputHandler : public IOHandler {
 public:
  GlibIOInputHandler(int fd, const base::Callback<void(InputData *)> &callback);
  ~GlibIOInputHandler();

  virtual void Start();
  virtual void Stop();

  const base::Callback<void(InputData *)> &callback() { return callback_; }

 private:
  GIOChannel *channel_;
  base::Callback<void(InputData *)> callback_;
  guint source_id_;
};


}  // namespace shill

#endif  // SHILL_GLIB_IO_INPUT_HANDLER_
