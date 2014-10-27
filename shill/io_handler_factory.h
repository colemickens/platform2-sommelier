// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_IO_HANDLER_FACTORY_H_
#define SHILL_IO_HANDLER_FACTORY_H_

#include <base/lazy_instance.h>

#include "shill/io_handler.h"

namespace shill {

class IOHandlerFactory {
 public:
  virtual ~IOHandlerFactory();

  // This is a singleton. Use IOHandlerFactory::GetInstance()->Foo().
  static IOHandlerFactory *GetInstance();

  virtual IOHandler *CreateIOInputHandler(
      int fd,
      const IOHandler::InputCallback &input_callback,
      const IOHandler::ErrorCallback &error_callback);

  virtual IOHandler *CreateIOReadyHandler(
      int fd,
      IOHandler::ReadyMode mode,
      const IOHandler::ReadyCallback &input_callback);

 protected:
  IOHandlerFactory();

 private:
  friend struct base::DefaultLazyInstanceTraits<IOHandlerFactory>;

  DISALLOW_COPY_AND_ASSIGN(IOHandlerFactory);
};

}  // namespace shill

#endif  // SHILL_IO_HANDLER_FACTORY_H_
