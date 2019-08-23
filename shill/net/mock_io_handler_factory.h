// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_NET_MOCK_IO_HANDLER_FACTORY_H_
#define SHILL_NET_MOCK_IO_HANDLER_FACTORY_H_

#include <gmock/gmock.h>

#include "shill/net/io_handler_factory.h"

namespace shill {

class MockIOHandlerFactory : public IOHandlerFactory {
 public:
  MockIOHandlerFactory() = default;
  ~MockIOHandlerFactory() override = default;

  MOCK_METHOD3(CreateIOInputHandler,
               IOHandler*(int fd,
                          const IOHandler::InputCallback& input_callback,
                          const IOHandler::ErrorCallback& error_callback));

  MOCK_METHOD3(CreateIOReadyHandler,
               IOHandler*(int fd,
                          IOHandler::ReadyMode mode,
                          const IOHandler::ReadyCallback& ready_callback));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockIOHandlerFactory);
};

}  // namespace shill

#endif  // SHILL_NET_MOCK_IO_HANDLER_FACTORY_H_
