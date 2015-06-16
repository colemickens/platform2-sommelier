// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/glib_io_handler_factory.h"

#include "shill/glib_io_input_handler.h"
#include "shill/glib_io_ready_handler.h"

namespace shill {

GlibIOHandlerFactory::GlibIOHandlerFactory() {}
GlibIOHandlerFactory::~GlibIOHandlerFactory() {}

IOHandler* GlibIOHandlerFactory::CreateIOInputHandler(
    int fd,
    const IOHandler::InputCallback& input_callback,
    const IOHandler::ErrorCallback& error_callback) {
  IOHandler* handler = new GlibIOInputHandler(
          fd, input_callback, error_callback);
  handler->Start();
  return handler;
}

IOHandler* GlibIOHandlerFactory::CreateIOReadyHandler(
    int fd,
    IOHandler::ReadyMode mode,
    const IOHandler::ReadyCallback& ready_callback) {
  IOHandler* handler = new GlibIOReadyHandler(fd, mode, ready_callback);
  handler->Start();
  return handler;
}

}  // namespace shill
