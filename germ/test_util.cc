// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "germ/test_util.h"

#include <unistd.h>

namespace germ {

ScopedAlarm::ScopedAlarm(unsigned int seconds) { alarm(seconds); }
ScopedAlarm::~ScopedAlarm() { alarm(0); }

}  // namespace germ
