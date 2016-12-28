// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/dbus_error_types.h"

namespace login_manager {
namespace dbus_error {
#define INTERFACE "org.chromium.SessionManagerInterface"

const char kNone[] = INTERFACE ".None";
const char kArcCpuCgroupFail[] = INTERFACE ".ArcCpuCgroupFail";
const char kArcInstanceRunning[] = INTERFACE ".ArcInstanceRunning";
const char kContainerStartupFail[] = INTERFACE ".ContainerStartupFail";
const char kContainerShutdownFail[] = INTERFACE ".ContainerShutdownFail";
const char kEmitFailed[] = INTERFACE ".EmitFailed";
const char kInitMachineInfoFail[] = INTERFACE ".InitMachineInfoFail";
const char kInvalidAccount[] = INTERFACE ".InvalidAccount";
const char kLowFreeDisk[] = INTERFACE ".LowFreeDisk";
const char kNoOwnerKey[] = INTERFACE ".NoOwnerKey";
const char kNoUserNssDb[] = INTERFACE ".NoUserNssDb";
const char kNotAvailable[] = INTERFACE ".NotAvailable";
const char kNotStarted[] = INTERFACE ".NotStarted";
const char kPolicyInitFail[] = INTERFACE ".PolicyInitFail";
const char kPubkeySetIllegal[] = INTERFACE ".PubkeySetIllegal";
const char kPolicySignatureRequired[] = INTERFACE ".PolicySignatureRequired";
const char kSessionDoesNotExist[] = INTERFACE ".SessionDoesNotExist";
const char kSessionExists[] = INTERFACE ".SessionExists";
const char kSigDecodeFail[] = INTERFACE ".SigDecodeFail";
const char kSigEncodeFail[] = INTERFACE ".SigEncodeFail";
const char kTestingChannelError[] = INTERFACE ".TestingChannelError";
const char kUnknownPid[] = INTERFACE ".UnknownPid";
const char kVerifyFail[] = INTERFACE ".VerifyFail";
const char kVpdUpdateFailed[] = INTERFACE ".VpdUpdateFailed";

}  // namespace dbus_error
}  // namespace login_manager
