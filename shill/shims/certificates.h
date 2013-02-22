// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_SHIMS_CERTIFICATES_H_
#define SHILL_SHIMS_CERTIFICATES_H_

#include "shill/byte_string.h"

namespace base {

class FilePath;

}  // namespace base

namespace shill {

namespace shims {

class Certificates {
 public:
  static ByteString ConvertDERToPEM(const ByteString &der_cert);

  static bool Write(const ByteString &cert, const base::FilePath &certfile);
};

}  // namespace shims

}  // namespace shill

#endif  // SHILL_SHIMS_CERTIFICATES_H_
