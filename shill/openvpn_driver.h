// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_OPENVPN_DRIVER_
#define SHILL_OPENVPN_DRIVER_

#include <string>
#include <vector>

#include <gtest/gtest_prod.h>  // for FRIEND_TEST

#include "shill/key_value_store.h"
#include "shill/vpn_driver.h"

namespace shill {

class Error;

class OpenVPNDriver : public VPNDriver {
 public:
  explicit OpenVPNDriver(const KeyValueStore &args);
  virtual ~OpenVPNDriver();

  // Inherited from VPNDriver.
  virtual void Connect(Error *error);

 private:
  friend class OpenVPNDriverTest;
  FRIEND_TEST(OpenVPNDriverTest, AppendFlag);
  FRIEND_TEST(OpenVPNDriverTest, AppendValueOption);
  FRIEND_TEST(OpenVPNDriverTest, InitOptions);
  FRIEND_TEST(OpenVPNDriverTest, InitOptionsNoHost);

  void InitOptions(std::vector<std::string> *options, Error *error);

  void AppendValueOption(const std::string &property,
                         const std::string &option,
                         std::vector<std::string> *options);
  void AppendFlag(const std::string &property,
                  const std::string &option,
                  std::vector<std::string> *options);

  KeyValueStore args_;

  DISALLOW_COPY_AND_ASSIGN(OpenVPNDriver);
};

}  // namespace shill

#endif  // SHILL_OPENVPN_DRIVER_
