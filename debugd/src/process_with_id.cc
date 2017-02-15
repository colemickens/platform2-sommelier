// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "debugd/src/process_with_id.h"

#include <base/rand_util.h>
#include <base/strings/string_number_conversions.h>

namespace debugd {

namespace {

constexpr int kNumRandomBytesInId = 16;

}  // namespace

bool ProcessWithId::Init() {
  if (SandboxedProcess::Init()) {
    GenerateId();
    return true;
  }
  return false;
}

void ProcessWithId::GenerateId() {
  std::string random_bytes = base::RandBytesAsString(kNumRandomBytesInId);
  id_ = base::HexEncode(random_bytes.data(), random_bytes.size());
}

}  // namespace debugd
