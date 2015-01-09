// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apmanager/mock_process_factory.h"

namespace apmanager {

namespace {
base::LazyInstance<MockProcessFactory> g_mock_process_factory
    = LAZY_INSTANCE_INITIALIZER;
}  // namespace

MockProcessFactory::MockProcessFactory() {}
MockProcessFactory::~MockProcessFactory() {}

MockProcessFactory* MockProcessFactory::GetInstance() {
  return g_mock_process_factory.Pointer();
}

}  // namespace apmanager
