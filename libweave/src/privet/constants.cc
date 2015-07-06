// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libweave/src/privet/constants.h"

namespace privetd {

namespace errors {

const char kDomain[] = "privetd";

const char kInvalidClientCommitment[] = "invalidClientCommitment";
const char kInvalidFormat[] = "invalidFormat";
const char kMissingAuthorization[] = "missingAuthorization";
const char kInvalidAuthorization[] = "invalidAuthorization";
const char kInvalidAuthorizationScope[] = "invalidAuthorizationScope";
const char kAuthorizationExpired[] = "authorizationExpired";
const char kCommitmentMismatch[] = "commitmentMismatch";
const char kUnknownSession[] = "unknownSession";
const char kInvalidAuthCode[] = "invalidAuthCode";
const char kInvalidAuthMode[] = "invalidAuthMode";
const char kInvalidRequestedScope[] = "invalidRequestedScope";
const char kAccessDenied[] = "accessDenied";
const char kInvalidParams[] = "invalidParams";
const char kSetupUnavailable[] = "setupUnavailable";
const char kDeviceBusy[] = "deviceBusy";
const char kInvalidState[] = "invalidState";
const char kInvalidSsid[] = "invalidSsid";
const char kInvalidPassphrase[] = "invalidPassphrase";
const char kNotFound[] = "notFound";
const char kNotImplemented[] = "notImplemented";

}  // namespace errors

}  // namespace privetd
