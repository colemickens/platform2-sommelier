// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_NET_MOCK_IO_HANDLER_FACTORY_H_
#define SHILL_NET_MOCK_IO_HANDLER_FACTORY_H_

#include <base/lazy_instance.h>
#include <gmock/gmock.h>

#include "shill/net/io_handler_factory.h"

namespace shill {

class MockIOHandlerFactory : public IOHandlerFactory {
 public:
  ~MockIOHandlerFactory() override;

  // This is a singleton. Use MockIOHandlerFactory::GetInstance()->Foo().
  static MockIOHandlerFactory* GetInstance();

  MOCK_METHOD3(CreateIOInputHandler,
               IOHandler* (
                   int fd,
                   const IOHandler::InputCallback& input_callback,
                   const IOHandler::ErrorCallback& error_callback));

  MOCK_METHOD3(CreateIOReadyHandler,
               IOHandler* (
                   int fd,
                   IOHandler::ReadyMode mode,
                   const IOHandler::ReadyCallback& ready_callback));

 protected:
  MockIOHandlerFactory();

 private:
  friend struct base::DefaultLazyInstanceTraits<MockIOHandlerFactory>;

  DISALLOW_COPY_AND_ASSIGN(MockIOHandlerFactory);
};

}  // namespace shill

#endif  // SHILL_NET_MOCK_IO_HANDLER_FACTORY_H_
