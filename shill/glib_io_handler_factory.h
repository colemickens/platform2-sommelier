// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
