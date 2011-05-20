// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_ADAPTOR_INTERFACES_
#define SHILL_ADAPTOR_INTERFACES_

#include <string>

namespace shill {

// These are the functions that a Manager adaptor must support
class ManagerAdaptorInterface {
 public:
  virtual void UpdateRunning() = 0;
  virtual ~ManagerAdaptorInterface() {}
};

// These are the functions that a Service adaptor must support
class ServiceAdaptorInterface {
 public:
  virtual void UpdateConnected() = 0;
  virtual ~ServiceAdaptorInterface() {}
};

// These are the functions that a Device adaptor must support
class DeviceAdaptorInterface {
 public:
  virtual void UpdateEnabled() = 0;
  virtual ~DeviceAdaptorInterface() {}
};

}  // namespace shill
#endif  // SHILL_ADAPTOR_INTERFACES_
