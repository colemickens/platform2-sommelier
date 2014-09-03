// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_PROPERTY_OBSERVER_INTERFACE_H_
#define SHILL_PROPERTY_OBSERVER_INTERFACE_H_

#include <base/macros.h>

namespace shill {

// An abstract interface for objects that retain a saved copy of
// a property accessor and can report whether that value has changed.
class PropertyObserverInterface {
 public:
  PropertyObserverInterface() {}
  virtual ~PropertyObserverInterface() {}

  // Update the saved value used for comparison.
  virtual void Update() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(PropertyObserverInterface);
};

}  // namespace shill

#endif  // SHILL_PROPERTY_OBSERVER_INTERFACE_H_
