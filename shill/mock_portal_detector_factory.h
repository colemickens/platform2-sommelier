// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_PORTAL_DETECTOR_FACTORY_H_
#define SHILL_MOCK_PORTAL_DETECTOR_FACTORY_H_

#include <base/lazy_instance.h>
#include <gmock/gmock.h>

#include "shill/connection.h"
#include "shill/portal_detector_factory.h"

namespace shill {

class MockPortalDetectorFactory : public PortalDetectorFactory {
 public:
  ~MockPortalDetectorFactory() override;

  // This is a singleton. Use MockPortalDetectorFactory::GetInstance()->Foo().
  static MockPortalDetectorFactory* GetInstance();

  MOCK_METHOD3(
      CreatePortalDetector,
      PortalDetector* (
          ConnectionRefPtr connection,
          EventDispatcher* dispatcher,
          const base::Callback<void(const PortalDetector::Result&)>& callback));

 protected:
  MockPortalDetectorFactory();

 private:
  friend struct base::DefaultLazyInstanceTraits<MockPortalDetectorFactory>;

  DISALLOW_COPY_AND_ASSIGN(MockPortalDetectorFactory);
};

}  // namespace shill

#endif  // SHILL_MOCK_PORTAL_DETECTOR_FACTORY_H_
