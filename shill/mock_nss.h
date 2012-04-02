// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_NSS_
#define SHILL_MOCK_NSS_

#include <gmock/gmock.h>

#include "shill/nss.h"

namespace shill {

class MockNSS : public NSS {
 public:
  MockNSS();
  virtual ~MockNSS();

  MOCK_METHOD2(GetPEMCertfile, FilePath(const std::string &nickname,
                                        const std::vector<char> &id));
  MOCK_METHOD2(GetDERCertfile, FilePath(const std::string &nickname,
                                        const std::vector<char> &id));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockNSS);
};

}  // namespace shill

#endif  // SHILL_MOCK_NSS_
