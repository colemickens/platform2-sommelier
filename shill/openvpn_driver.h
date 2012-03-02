// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_OPENVPN_DRIVER_
#define SHILL_OPENVPN_DRIVER_

#include <map>
#include <string>
#include <vector>

#include <base/memory/scoped_ptr.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST

#include "shill/ipconfig.h"
#include "shill/key_value_store.h"
#include "shill/vpn_driver.h"

namespace shill {

class ControlInterface;
class DeviceInfo;
class Error;
class RPCTask;
class DeviceStub;

class OpenVPNDriver : public VPNDriver {
 public:
  OpenVPNDriver(ControlInterface *control,
                DeviceInfo *device_info,
                const KeyValueStore &args);
  virtual ~OpenVPNDriver();

  bool Notify(const std::string &reason,
              const std::map<std::string, std::string> &dict);

  // Inherited from VPNDriver.
  virtual bool ClaimInterface(const std::string &link_name,
                              int interface_index);
  virtual void Connect(Error *error);

 private:
  friend class OpenVPNDriverTest;
  FRIEND_TEST(OpenVPNDriverTest, AppendFlag);
  FRIEND_TEST(OpenVPNDriverTest, AppendValueOption);
  FRIEND_TEST(OpenVPNDriverTest, ClaimInterface);
  FRIEND_TEST(OpenVPNDriverTest, InitOptions);
  FRIEND_TEST(OpenVPNDriverTest, InitOptionsNoHost);
  FRIEND_TEST(OpenVPNDriverTest, ParseForeignOption);
  FRIEND_TEST(OpenVPNDriverTest, ParseForeignOptions);
  FRIEND_TEST(OpenVPNDriverTest, ParseIPConfiguration);

  // The map is a sorted container that allows us to iterate through the options
  // in order.
  typedef std::map<int, std::string> ForeignOptions;

  static void ParseIPConfiguration(
      const std::map<std::string, std::string> &configuration,
      IPConfig::Properties *properties);
  static void ParseForeignOptions(const ForeignOptions &options,
                                  IPConfig::Properties *properties);
  static void ParseForeignOption(const std::string &option,
                                 IPConfig::Properties *properties);

  void InitOptions(std::vector<std::string> *options, Error *error);

  void AppendValueOption(const std::string &property,
                         const std::string &option,
                         std::vector<std::string> *options);
  void AppendFlag(const std::string &property,
                  const std::string &option,
                  std::vector<std::string> *options);

  ControlInterface *control_;
  DeviceInfo *device_info_;
  KeyValueStore args_;
  scoped_ptr<RPCTask> rpc_task_;
  std::string tunnel_interface_;
  int interface_index_;

  DISALLOW_COPY_AND_ASSIGN(OpenVPNDriver);
};

}  // namespace shill

#endif  // SHILL_OPENVPN_DRIVER_
