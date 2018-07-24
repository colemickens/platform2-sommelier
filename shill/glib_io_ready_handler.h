// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_GLIB_IO_READY_HANDLER_H_
#define SHILL_GLIB_IO_READY_HANDLER_H_

#include <glib.h>
#include <stdio.h>

#include <base/callback.h>

#include "shill/net/io_handler.h"

namespace shill {

// This handler is different from the GlibIOInputHandler
// in that we don't read/write from the file handle and
// leave that to the caller.  This is useful in accept()ing
// sockets and effort to working with peripheral libraries.
class GlibIOReadyHandler : public IOHandler {
 public:
  GlibIOReadyHandler(int fd,
                     IOHandler::ReadyMode mode,
                     const base::Callback<void(int)>& callback);
  ~GlibIOReadyHandler();

  virtual void Start();
  virtual void Stop();

  const base::Callback<void(int)>& callback() { return callback_; }

 private:
  GIOChannel* channel_;
  GIOCondition condition_;
  const base::Callback<void(int)> callback_;
  guint source_id_;
};


}  // namespace shill

#endif  // SHILL_GLIB_IO_READY_HANDLER_H_
