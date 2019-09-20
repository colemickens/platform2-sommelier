// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTOHOME_MOCK_SERVICE_H_
#define CRYPTOHOME_MOCK_SERVICE_H_

#include "cryptohome/service.h"
#include "cryptohome/service_monolithic.h"

#include <memory>
#include <string>

#include <base/files/file_path.h>
#include <gmock/gmock.h>

namespace cryptohome {

class MockService : public ServiceMonolithic {
 public:
  explicit MockService(const std::string& abe_data);
  MockService();  // For convenience in unit tests.
  virtual ~MockService();

  MOCK_METHOD(gboolean,
              Mount,
              (const gchar*,
               const gchar*,
               gboolean,
               gboolean,
               gint*,
               gboolean*,
               GError**),
              (override));
  MOCK_METHOD(gboolean, Unmount, (gboolean*, GError**), (override));
  MOCK_METHOD(bool,
              GetMountPointForUser,
              (const std::string&, base::FilePath*),
              (override));
  MOCK_METHOD(bool, IsOwner, (const std::string&), (override));
};

}  // namespace cryptohome

#endif  // CRYPTOHOME_MOCK_SERVICE_H_
