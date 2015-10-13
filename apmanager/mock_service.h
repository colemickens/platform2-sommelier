// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APMANAGER_MOCK_SERVICE_H_
#define APMANAGER_MOCK_SERVICE_H_

#include <base/macros.h>
#include <gmock/gmock.h>

#include "apmanager/service.h"

namespace apmanager {

class MockService : public Service {
 public:
  MockService();
  ~MockService() override;

  MOCK_METHOD1(Start, bool(brillo::ErrorPtr *error));
  MOCK_METHOD1(Stop, bool(brillo::ErrorPtr *error));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockService);
};

}  // namespace apmanager

#endif  // APMANAGER_MOCK_SERVICE_H_
