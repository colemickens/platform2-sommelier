// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_PORTAL_DETECTOR_FACTORY_H_
#define SHILL_PORTAL_DETECTOR_FACTORY_H_

#include <base/callback.h>
#include <base/lazy_instance.h>

#include "shill/portal_detector.h"
#include "shill/refptr_types.h"

namespace shill {

class EventDispatcher;

class PortalDetectorFactory {
 public:
  virtual ~PortalDetectorFactory();

  // This is a singleton. Use PortalDetectorFactory::GetInstance()->Foo().
  static PortalDetectorFactory* GetInstance();

  virtual PortalDetector* CreatePortalDetector(
      ConnectionRefPtr connection,
      EventDispatcher* dispatcher,
      const base::Callback<void(const PortalDetector::Result&)> &callback);

 protected:
  PortalDetectorFactory();

 private:
  friend struct base::DefaultLazyInstanceTraits<PortalDetectorFactory>;

  DISALLOW_COPY_AND_ASSIGN(PortalDetectorFactory);
};

}  // namespace shill

#endif  // SHILL_PORTAL_DETECTOR_FACTORY_H_
