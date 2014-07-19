// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTOHOME_MOCK_SERVICE_H_
#define CRYPTOHOME_MOCK_SERVICE_H_

#include "cryptohome/service.h"

#include <string>

#include <gmock/gmock.h>

namespace cryptohome {

using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;

class MockService : public Service {
 public:
  MockService();
  virtual ~MockService();

  MOCK_METHOD7(Mount, gboolean(const gchar *,
                               const gchar *,
                               gboolean,
                               gboolean,
                               gint *,
                               gboolean *,
                               GError **));
  MOCK_METHOD3(UnmountForUser, gboolean(const gchar*, gboolean *,
                                        GError **));
  MOCK_METHOD2(GetMountPointForUser, bool(const std::string&, std::string*));
  MOCK_METHOD1(IsOwner, bool(const std::string&));
};

}  // namespace cryptohome

#endif  // CRYPTOHOME_MOCK_SERVICE_H_
