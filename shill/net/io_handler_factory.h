// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_NET_IO_HANDLER_FACTORY_H_
#define SHILL_NET_IO_HANDLER_FACTORY_H_

#include "shill/net/io_handler.h"
#include "shill/net/shill_export.h"

namespace shill {

class SHILL_EXPORT IOHandlerFactory {
 public:
  IOHandlerFactory();
  virtual ~IOHandlerFactory();

  virtual IOHandler* CreateIOInputHandler(
      int fd,
      const IOHandler::InputCallback& input_callback,
      const IOHandler::ErrorCallback& error_callback);

  virtual IOHandler* CreateIOReadyHandler(
      int fd,
      IOHandler::ReadyMode mode,
      const IOHandler::ReadyCallback& input_callback);

 private:
  DISALLOW_COPY_AND_ASSIGN(IOHandlerFactory);
};

}  // namespace shill

#endif  // SHILL_NET_IO_HANDLER_FACTORY_H_
