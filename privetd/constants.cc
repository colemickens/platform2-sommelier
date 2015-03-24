// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "privetd/constants.h"

namespace privetd {

namespace errors {

const char kDomain[] = "privetd";

const char kInvalidClientCommitment[] = "invalidClientCommitment";
const char kInvalidFormat[] = "invalidFormat";
const char kMissingAuthorization[] = "missingAuthorization";
const char kInvalidAuthorization[] = "invalidAuthorization";
const char kInvalidAuthorizationScope[] = "invalidAuthorizationScope";
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
const char kInvalidTicket[] = "invalidTicket";
const char kServerError[] = "serverError";
const char kInvalidSsid[] = "invalidSsid";
const char kInvalidPassphrase[] = "invalidPassphrase";
const char kUnknownCommand[] = "unknownCommand";

}  // namespace errors

}  // namespace privetd
