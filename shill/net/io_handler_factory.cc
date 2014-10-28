// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/net/io_handler_factory.h"

#include <base/message_loop/message_loop.h>

#include "shill/net/glib_io_input_handler.h"
#include "shill/net/glib_io_ready_handler.h"
#include "shill/net/io_input_handler.h"
#include "shill/net/io_ready_handler.h"

namespace shill {

namespace {

base::LazyInstance<IOHandlerFactory> g_io_handler_factory
    = LAZY_INSTANCE_INITIALIZER;

}  // namespace

IOHandlerFactory::IOHandlerFactory() {}
IOHandlerFactory::~IOHandlerFactory() {}

IOHandlerFactory *IOHandlerFactory::GetInstance() {
  return g_io_handler_factory.Pointer();
}

IOHandler *IOHandlerFactory::CreateIOInputHandler(
    int fd,
    const IOHandler::InputCallback &input_callback,
    const IOHandler::ErrorCallback &error_callback) {
  IOHandler *handler = nullptr;
  if (base::MessageLoop::current()->IsType(base::MessageLoop::TYPE_IO)) {
    handler = new IOInputHandler(fd, input_callback, error_callback);
  } else if (base::MessageLoop::current()->IsType(base::MessageLoop::TYPE_UI)) {
    handler = new GlibIOInputHandler(fd, input_callback, error_callback);
  } else {
    NOTREACHED() << "Unsupport MessageLoop type: "
                 << base::MessageLoop::current()->type();
  }

  handler->Start();
  return handler;
}

IOHandler *IOHandlerFactory::CreateIOReadyHandler(
    int fd,
    IOHandler::ReadyMode mode,
    const IOHandler::ReadyCallback &ready_callback) {
  IOHandler *handler = nullptr;
  if (base::MessageLoop::current()->IsType(base::MessageLoop::TYPE_IO)) {
    handler = new IOReadyHandler(fd, mode, ready_callback);
  } else if (base::MessageLoop::current()->IsType(base::MessageLoop::TYPE_UI)) {
    handler = new GlibIOReadyHandler(fd, mode, ready_callback);
  } else {
    NOTREACHED() << "Unsupport MessageLoop type: "
                 << base::MessageLoop::current()->type();
  }

  handler->Start();
  return handler;
}

}  // namespace shill
