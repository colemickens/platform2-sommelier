// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/net/mock_io_handler_factory.h"

namespace shill {

namespace {
base::LazyInstance<MockIOHandlerFactory> g_mock_io_handler_factory
    = LAZY_INSTANCE_INITIALIZER;
}  // namespace

MockIOHandlerFactory::MockIOHandlerFactory() {}
MockIOHandlerFactory::~MockIOHandlerFactory() {}

MockIOHandlerFactory* MockIOHandlerFactory::GetInstance() {
  return g_mock_io_handler_factory.Pointer();
}

}  // namespace shill
