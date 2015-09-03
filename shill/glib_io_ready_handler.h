//
// Copyright (C) 2011 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

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
