// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOGIN_MANAGER_DBUS_ERROR_TYPES_H_
#define LOGIN_MANAGER_DBUS_ERROR_TYPES_H_

namespace login_manager {
namespace dbus_error {

extern const char kNone[];
extern const char kArcCpuCgroupFail[];
extern const char kArcInstanceRunning[];
extern const char kContainerStartupFail[];
extern const char kContainerShutdownFail[];
extern const char kEmitFailed[];
extern const char kInitMachineInfoFail[];
extern const char kInvalidAccount[];
extern const char kLowFreeDisk[];
extern const char kNoOwnerKey[];
extern const char kNoUserNssDb[];
extern const char kNotAvailable[];
extern const char kNotStarted[];
extern const char kPolicyInitFail[];
extern const char kPubkeySetIllegal[];
extern const char kPolicySignatureRequired[];
extern const char kSessionDoesNotExist[];
extern const char kSessionExists[];
extern const char kSigDecodeFail[];
extern const char kSigEncodeFail[];
extern const char kTestingChannelError[];
extern const char kUnknownPid[];
extern const char kVerifyFail[];
extern const char kVpdUpdateFailed[];

}  // namespace dbus_error
}  // namespace login_manager

#endif  // LOGIN_MANAGER_DBUS_ERROR_TYPES_H_
