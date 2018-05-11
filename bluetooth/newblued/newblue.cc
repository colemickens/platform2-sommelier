// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bluetooth/newblued/newblue.h"

#include <memory>
#include <utility>

namespace bluetooth {

Newblue::Newblue(std::unique_ptr<LibNewblue> libnewblue)
    : libnewblue_(std::move(libnewblue)) {}

bool Newblue::Init() {
  return true;
}

}  // namespace bluetooth
