// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBSERVER_WEBSERVD_ERROR_CODES_H_
#define WEBSERVER_WEBSERVD_ERROR_CODES_H_

namespace webservd {
namespace errors {

// Error domain for the webserver.
extern const char kDomain[];

// Invalid server configuration.
extern const char kInvalidConfig[];

}  // namespace errors
}  // namespace webservd

#endif  // WEBSERVER_WEBSERVD_ERROR_CODES_H_
