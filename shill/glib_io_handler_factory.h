//
// Copyright (C) 2014 The Android Open Source Project
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

#ifndef SHILL_GLIB_IO_HANDLER_FACTORY_H_
#define SHILL_GLIB_IO_HANDLER_FACTORY_H_

#include "shill/net/io_handler_factory.h"

namespace shill {

class GlibIOHandlerFactory : public IOHandlerFactory {
 public:
  GlibIOHandlerFactory();
  virtual ~GlibIOHandlerFactory();

  IOHandler* CreateIOInputHandler(
      int fd,
      const IOHandler::InputCallback& input_callback,
      const IOHandler::ErrorCallback& error_callback) override;

  IOHandler* CreateIOReadyHandler(
      int fd,
      IOHandler::ReadyMode mode,
      const IOHandler::ReadyCallback& input_callback) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(GlibIOHandlerFactory);
};

}  // namespace shill

#endif  // SHILL_GLIB_IO_HANDLER_FACTORY_H_
