// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_WIMAX_SERVICE_H_
#define SHILL_MOCK_WIMAX_SERVICE_H_

#include <gmock/gmock.h>

#include "shill/wimax_service.h"

namespace shill {

class MockWiMaxService : public WiMaxService {
 public:
  MockWiMaxService(ControlInterface *control,
                   EventDispatcher *dispatcher,
                   Metrics *metrics,
                   Manager *manager,
                   const WiMaxRefPtr &wimax);
  virtual ~MockWiMaxService();

 private:
  DISALLOW_COPY_AND_ASSIGN(MockWiMaxService);
};

}  // namespace shill

#endif  // SHILL_MOCK_WIMAX_SERVICE_H_
