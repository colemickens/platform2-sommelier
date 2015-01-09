// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apmanager/mock_event_dispatcher.h"

namespace apmanager {

namespace {
base::LazyInstance<MockEventDispatcher> g_mock_event_dispatcher
    = LAZY_INSTANCE_INITIALIZER;
}  // namespace

MockEventDispatcher::MockEventDispatcher() {}
MockEventDispatcher::~MockEventDispatcher() {}

MockEventDispatcher* MockEventDispatcher::GetInstance() {
  return g_mock_event_dispatcher.Pointer();
}

}  // namespace apmanager
