// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "u2fd/allowlisting_util.h"

#include <functional>
#include <vector>

#include <attestation/proto_bindings/interface.pb.h>
#include <base/optional.h>
#include <base/strings/string_number_conversions.h>

namespace u2f {

AllowlistingUtil::AllowlistingUtil(
    std::function<base::Optional<attestation::GetCertifiedNvIndexReply>(int)>
        get_certified_g2f_cert)
    : get_certified_g2f_cert_(get_certified_g2f_cert) {}

bool AllowlistingUtil::AppendDataToCert(std::vector<uint8_t>* cert) {
  base::Optional<attestation::GetCertifiedNvIndexReply> reply =
      get_certified_g2f_cert_(cert->size());

  if (reply.has_value()) {
    // TODO(louiscollard): Replace with implementation.
    LOG(INFO) << "Certified Data: "
              << base::HexEncode(reply->certified_data().data(),
                                 reply->certified_data().size());
    LOG(INFO) << "Signature: "
              << base::HexEncode(reply->signature().data(),
                                 reply->signature().size());
  }

  return true;
}

}  // namespace u2f
