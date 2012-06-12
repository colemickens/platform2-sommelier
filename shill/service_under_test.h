// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "shill/service.h"

namespace shill {

class ControlInterface;
class Error;
class EventDispatcher;
class Manager;
class Metrics;

// This is a simple Service subclass with all the pure-virutal methods stubbed.
class ServiceUnderTest : public Service {
 public:
  static const char kRpcId[];
  static const char kStorageId[];

  ServiceUnderTest(ControlInterface *control_interface,
                   EventDispatcher *dispatcher,
                   Metrics *metrics,
                   Manager *manager);
  virtual ~ServiceUnderTest();

  virtual std::string GetRpcIdentifier() const;
  virtual std::string GetDeviceRpcId(Error *error);
  virtual std::string GetStorageIdentifier() const;

 private:
  DISALLOW_COPY_AND_ASSIGN(ServiceUnderTest);
};

}  // namespace shill
