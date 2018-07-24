// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_SHIMS_NET_DIAGS_UPLOAD_H_
#define SHILL_SHIMS_NET_DIAGS_UPLOAD_H_

class FilePath;

namespace shill {

namespace shims {

static const char kStashedNetLog[] = "/var/log/net-diags.net.log";

}  // namespace shims

}  // namespace shill

#endif  // SHILL_SHIMS_NET_DIAGS_UPLOAD_H_
