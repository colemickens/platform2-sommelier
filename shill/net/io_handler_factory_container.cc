// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/net/io_handler_factory_container.h"

namespace shill {

namespace {

base::LazyInstance<IOHandlerFactoryContainer> g_io_handler_factory_container
    = LAZY_INSTANCE_INITIALIZER;

}  // namespace

IOHandlerFactoryContainer::IOHandlerFactoryContainer()
    : factory_(new IOHandlerFactory()) {}

IOHandlerFactoryContainer::~IOHandlerFactoryContainer() {}

IOHandlerFactoryContainer* IOHandlerFactoryContainer::GetInstance() {
  return g_io_handler_factory_container.Pointer();
}

void IOHandlerFactoryContainer::SetIOHandlerFactory(IOHandlerFactory* factory) {
  CHECK(factory);
  factory_.reset(factory);
}

IOHandlerFactory* IOHandlerFactoryContainer::GetIOHandlerFactory() {
  return factory_.get();
}

}  // namespace shill
