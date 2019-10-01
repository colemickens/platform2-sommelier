// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef U2FD_MOCK_ALLOWLISTING_UTIL_H_
#define U2FD_MOCK_ALLOWLISTING_UTIL_H_

#include "u2fd/allowlisting_util.h"

#include <vector>

#include <base/optional.h>
#include <brillo/secure_blob.h>
#include <gmock/gmock.h>

namespace u2f {

class MockAllowlistingUtil : public AllowlistingUtil {
 public:
  MockAllowlistingUtil()
      : AllowlistingUtil(
            std::function<base::Optional<attestation::GetCertifiedNvIndexReply>(
                int)>() /* dummy callback, not used */) {}

  MOCK_METHOD(bool, AppendDataToCert, (std::vector<uint8_t>*), (override));
};

}  // namespace u2f

#endif  // U2FD_MOCK_ALLOWLISTING_UTIL_H_
