// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/portal_detector_factory.h"

#include "shill/connection.h"

namespace shill {

namespace {
base::LazyInstance<PortalDetectorFactory> g_portal_detector_factory =
    LAZY_INSTANCE_INITIALIZER;
}  // namespace

PortalDetectorFactory::PortalDetectorFactory() {}
PortalDetectorFactory::~PortalDetectorFactory() {}

PortalDetectorFactory* PortalDetectorFactory::GetInstance() {
  return g_portal_detector_factory.Pointer();
}

PortalDetector* PortalDetectorFactory::CreatePortalDetector(
    ConnectionRefPtr connection,
    EventDispatcher* dispatcher,
    const base::Callback<void(const PortalDetector::Result&)> &callback) {
  return new PortalDetector(connection, dispatcher, callback);
}

}  // namespace shill
