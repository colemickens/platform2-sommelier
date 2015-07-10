// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBWEAVE_SRC_REGISTRATION_STATUS_H_
#define LIBWEAVE_SRC_REGISTRATION_STATUS_H_

#include <string>

#include "weave/types.h"

namespace weave {

// TODO(vitalybuka): Use EnumToString.
std::string StatusToString(RegistrationStatus status);

}  // namespace weave

#endif  // LIBWEAVE_SRC_REGISTRATION_STATUS_H_
