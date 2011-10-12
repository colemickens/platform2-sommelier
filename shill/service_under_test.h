// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "shill/service.h"

namespace shill {

class ControlInterface;
class Error;
class EventDispatcher;
class Manager;

// This is a simple Service subclass with all the pure-virutal methods stubbed
class ServiceUnderTest : public Service {
 public:
  static const char kRpcId[];
  static const char kStorageId[];

  ServiceUnderTest(ControlInterface *control_interface,
                   EventDispatcher *dispatcher,
                   Manager *manager);
  virtual ~ServiceUnderTest();

  virtual void Connect(Error */*error*/);
  virtual void Disconnect();
  virtual std::string CalculateState();
  virtual std::string GetRpcIdentifier() const;
  virtual std::string GetDeviceRpcId();
  virtual std::string GetStorageIdentifier() const;

 private:
  DISALLOW_COPY_AND_ASSIGN(ServiceUnderTest);
};

}  // namespace shill
