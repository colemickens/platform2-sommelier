// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/mock_portal_detector_factory.h"

namespace shill {

namespace {
base::LazyInstance<MockPortalDetectorFactory> g_mock_portal_detector_factory =
    LAZY_INSTANCE_INITIALIZER;
}  // namespace

MockPortalDetectorFactory::MockPortalDetectorFactory() {}
MockPortalDetectorFactory::~MockPortalDetectorFactory() {}

MockPortalDetectorFactory* MockPortalDetectorFactory::GetInstance() {
  return g_mock_portal_detector_factory.Pointer();
}

}  // namespace shill
