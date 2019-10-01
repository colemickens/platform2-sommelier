// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef U2FD_ALLOWLISTING_UTIL_H_
#define U2FD_ALLOWLISTING_UTIL_H_

#include <functional>
#include <vector>

#include <attestation/proto_bindings/interface.pb.h>
#include <base/optional.h>

namespace u2f {

// Utility to append allowlisting data to a U2F_REGISTER response.
class AllowlistingUtil {
 public:
  // Creates a new utility, which will make use of the specified function to
  // retrieve a certified copy of the G2F certificate.
  AllowlistingUtil(
      std::function<base::Optional<attestation::GetCertifiedNvIndexReply>(int)>
          get_certified_g2f_cert);

  virtual ~AllowlistingUtil() = default;

  // Appends allowlisting data to the specified certificate. Returns true on
  // success. On failure, returns false, and does not modify |cert|.
  virtual bool AppendDataToCert(std::vector<uint8_t>* cert);

 private:
  std::function<base::Optional<attestation::GetCertifiedNvIndexReply>(int)>
      get_certified_g2f_cert_;
};

}  // namespace u2f

#endif  // U2FD_ALLOWLISTING_UTIL_H_
