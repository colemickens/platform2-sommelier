// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "thd/source/fake_source.h"

namespace thd {

FakeSource::FakeSource(int64_t value, bool success)
    : value_(value), success_(success) {}

bool FakeSource::ReadValue(int64_t* value_out) {
  if (success_)
    *value_out = value_;
  return success_;
}

}  // namespace thd
