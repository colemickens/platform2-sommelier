// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weave/cloud.h"
#include "weave/enum_to_string.h"

namespace weave {

namespace {

const EnumToStringMap<RegistrationStatus>::Map kMap[] = {
    {RegistrationStatus::kUnconfigured, "unconfigured"},
    {RegistrationStatus::kConnecting, "connecting"},
    {RegistrationStatus::kConnected, "connected"},
    {RegistrationStatus::kInvalidCredentials, "invalid_credentials"},
};

}  // namespace

template <>
EnumToStringMap<RegistrationStatus>::EnumToStringMap()
    : EnumToStringMap(kMap) {}

}  // namespace weave
