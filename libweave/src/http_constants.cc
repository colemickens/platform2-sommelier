// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libweave/src/http_constants.h"

namespace weave {
namespace http {

const char kGet[] = "GET";
const char kPatch[] = "PATCH";
const char kPost[] = "POST";
const char kPut[] = "PUT";

const char kAuthorization[] = "Authorization";
const char kContentType[] = "Content-Type";

const char kJson[] = "application/json";
const char kJsonUtf8[] = "application/json; charset=utf-8";
const char kPlain[] = "text/plain";
const char kWwwFormUrlEncoded[] = "application/x-www-form-urlencoded";

}  // namespace http
}  // namespace weave
