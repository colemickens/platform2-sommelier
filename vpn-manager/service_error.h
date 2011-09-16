// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef _VPN_MANAGER_SERVICE_ERROR_H_
#define _VPN_MANAGER_SERVICE_ERROR_H_

// A enumeration of service errors. These error identifiers are ordered in a way
// that a more specific error has a higher value than a less specific one, and
// an error from an inner service has a higher value than an error from an outer
// service.
enum ServiceError {
  kServiceErrorNoError = 0,

  // Common errors
  kServiceErrorInternal = 1,
  kServiceErrorInvalidArgument = 2,
  kServiceErrorResolveHostnameFailed = 3,

  // IPsec specific errors
  kServiceErrorIpsecConnectionFailed = 32,
  kServiceErrorIpsecPresharedKeyAuthenticationFailed = 33,
  kServiceErrorIpsecCertificateAuthenticationFailed = 34,

  // LT2P specific errors
  kServiceErrorL2tpConnectionFailed = 64,

  // PPP specific errors
  kServiceErrorPppConnectionFailed = 128,
  kServiceErrorPppAuthenticationFailed = 129,
};

#endif  // _VPN_MANAGER_SERVICE_ERROR_H_
