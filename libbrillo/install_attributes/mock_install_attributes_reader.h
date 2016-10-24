// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBBRILLO_INSTALL_ATTRIBUTES_MOCK_INSTALL_ATTRIBUTES_READER_H_
#define LIBBRILLO_INSTALL_ATTRIBUTES_MOCK_INSTALL_ATTRIBUTES_READER_H_

#include "libinstallattributes.h"

#include "bindings/install_attributes.pb.h"

class MockInstallAttributesReader : public InstallAttributesReader {
 public:
  explicit MockInstallAttributesReader(
      const cryptohome::SerializedInstallAttributes& install_attributes);
};

#endif  // LIBBRILLO_INSTALL_ATTRIBUTES_MOCK_INSTALL_ATTRIBUTES_READER_H_
