// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOGIN_MANAGER_DBUS_ERROR_TYPES_H_
#define LOGIN_MANAGER_DBUS_ERROR_TYPES_H_

namespace login_manager {
namespace dbus_error {
#define INTERFACE "org.chromium.SessionManagerInterface"

static const char kEmitFailed[] = INTERFACE ".EmitFailed";
static const char kInvalidAccount[] = INTERFACE ".InvalidAccount";
static const char kNoOwnerKey[] = INTERFACE ".NoOwnerKey";
static const char kNoUserNssDb[] = INTERFACE ".NoUserNssDb";
static const char kPolicyInitFail[] = INTERFACE ".PolicyInitFail";
static const char kPubkeySetIllegal[] = INTERFACE ".PubkeySetIllegal";
static const char kSessionDoesNotExist[] = INTERFACE ".SessionDoesNotExist";
static const char kSessionExists[] = INTERFACE ".SessionExists";
static const char kSigEncodeFail[] = INTERFACE ".SigEncodeFail";
static const char kSigDecodeFail[] = INTERFACE ".SigDecodeFail";
static const char kTestingChannelError[] = INTERFACE ".TestingChannelError";
static const char kUnknownPid[] = INTERFACE ".UnknownPid";
static const char kVerifyFail[] = INTERFACE ".VerifyFail";

#undef INTERFACE
}  // dbus_error
}  // namespace login_manager

#endif  // LOGIN_MANAGER_DBUS_ERROR_TYPES_H_
