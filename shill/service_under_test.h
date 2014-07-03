// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_SERVICE_UNDER_TEST_H_
#define SHILL_SERVICE_UNDER_TEST_H_

#include <string>
#include <vector>

#include "shill/service.h"

namespace shill {

class ControlInterface;
class Error;
class EventDispatcher;
class Manager;
class Metrics;

// This is a simple Service subclass with all the pure-virtual methods stubbed.
class ServiceUnderTest : public Service {
 public:
  static const char kRpcId[];
  static const char kStringsProperty[];
  static const char kStorageId[];

  ServiceUnderTest(ControlInterface *control_interface,
                   EventDispatcher *dispatcher,
                   Metrics *metrics,
                   Manager *manager);
  virtual ~ServiceUnderTest();

  virtual std::string GetRpcIdentifier() const;
  virtual std::string GetDeviceRpcId(Error *error) const;
  virtual std::string GetStorageIdentifier() const;

  // Getter and setter for a string array property for use in testing.
  void set_strings(const std::vector<std::string> &strings) {
    strings_ = strings;
  }
  const std::vector<std::string> &strings() const { return strings_; }

 private:
  // The Service superclass has no string array properties but we need one
  // in order to test Service::Configure.
  std::vector<std::string> strings_;

  DISALLOW_COPY_AND_ASSIGN(ServiceUnderTest);
};

}  // namespace shill

#endif  // SHILL_SERVICE_UNDER_TEST_H_
