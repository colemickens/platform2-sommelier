// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/crossystem.h"

#include <vboot/crossystem.h>

static_assert(Crossystem::kVbMaxStringProperty == VB_MAX_STRING_PROPERTY,
              "VB_MAX_STRING_PROPERTY got out of sync!");
