// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/system/display/display_watcher_stub.h"

#include <base/logging.h>

namespace power_manager {
namespace system {

DisplayWatcherStub::DisplayWatcherStub() {}

DisplayWatcherStub::~DisplayWatcherStub() {}

const std::vector<DisplayInfo>& DisplayWatcherStub::GetDisplays() const {
  return displays_;
}

void DisplayWatcherStub::AddObserver(DisplayWatcherObserver* observer) {
  CHECK(observer);
}

void DisplayWatcherStub::RemoveObserver(DisplayWatcherObserver* observer) {
  CHECK(observer);
}

}  // namespace system
}  // namespace power_manager
