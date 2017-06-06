// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Mock for PCAProxy.

#ifndef CRYPTOHOME_MOCK_CERT_PROVISION_PCA_H_
#define CRYPTOHOME_MOCK_CERT_PROVISION_PCA_H_

#include <string>

#include "cryptohome/cert_provision_pca.h"

namespace cert_provision {

class MockPCAProxy : public PCAProxy {
 public:
  MockPCAProxy() : PCAProxy("") {}
  MOCK_METHOD3(MakeRequest,
               OpResult(const std::string&,
                        const brillo::SecureBlob&,
                        brillo::SecureBlob*));
};

}  // namespace cert_provision

#endif  // CRYPTOHOME_MOCK_CERT_PROVISION_PCA_H_
