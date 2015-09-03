//
// Copyright (C) 2012 The Android Open Source Project
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
