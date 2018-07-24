// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_DIAGNOSTICS_REPORTER_H_
#define SHILL_MOCK_DIAGNOSTICS_REPORTER_H_

#include <gmock/gmock.h>

#include "shill/diagnostics_reporter.h"

namespace shill {

class MockDiagnosticsReporter : public DiagnosticsReporter {
 public:
  MockDiagnosticsReporter();
  ~MockDiagnosticsReporter() override;

  MOCK_METHOD0(OnConnectivityEvent, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockDiagnosticsReporter);
};

}  // namespace shill

#endif  // SHILL_MOCK_DIAGNOSTICS_REPORTER_H_
