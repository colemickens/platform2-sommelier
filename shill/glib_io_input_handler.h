// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_GLIB_IO_INPUT_HANDLER_H_
#define SHILL_GLIB_IO_INPUT_HANDLER_H_

#include <glib.h>
#include <stdio.h>

#include <base/callback.h>

#include "shill/net/io_handler.h"

namespace shill {

class GlibIOInputHandler : public IOHandler {
 public:
  GlibIOInputHandler(int fd,
                     const InputCallback& input_callback,
                     const ErrorCallback& error_callback);
  ~GlibIOInputHandler();

  virtual void Start();
  virtual void Stop();

  const InputCallback& input_callback() { return input_callback_; }
  const ErrorCallback& error_callback() { return error_callback_; }

 private:
  GIOChannel* channel_;
  InputCallback input_callback_;
  ErrorCallback error_callback_;
  guint source_id_;
};


}  // namespace shill

#endif  // SHILL_GLIB_IO_INPUT_HANDLER_H_
