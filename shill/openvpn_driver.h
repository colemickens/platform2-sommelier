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
#include "shill/refptr_types.h"
#include "shill/vpn_driver.h"

namespace shill {

class ControlInterface;
class DeviceInfo;
class Error;
class EventDispatcher;
class Manager;
class Metrics;
class RPCTask;
class DeviceStub;

class OpenVPNDriver : public VPNDriver {
 public:
  OpenVPNDriver(ControlInterface *control,
                EventDispatcher *dispatcher,
                Metrics *metrics,
                Manager *manager,
                DeviceInfo *device_info,
                const KeyValueStore &args);
  virtual ~OpenVPNDriver();

  bool Notify(const std::string &reason,
              const std::map<std::string, std::string> &dict);

  // Inherited from VPNDriver. |Connect| initiates the VPN connection by
  // creating a tunnel device. When the device index becomes available, this
  // instance is notified through |ClaimInterface| and resumes the connection
  // process by setting up and spawning an external 'openvpn' process. IP
  // configuration settings are passed back from the external process through
  // the |Notify| RPC service method.
  virtual void Connect(Error *error);
  virtual bool ClaimInterface(const std::string &link_name,
                              int interface_index);

 private:
  friend class OpenVPNDriverTest;
  FRIEND_TEST(OpenVPNDriverTest, AppendFlag);
  FRIEND_TEST(OpenVPNDriverTest, AppendValueOption);
  FRIEND_TEST(OpenVPNDriverTest, ClaimInterface);
  FRIEND_TEST(OpenVPNDriverTest, Connect);
  FRIEND_TEST(OpenVPNDriverTest, GetRouteOptionEntry);
  FRIEND_TEST(OpenVPNDriverTest, InitOptions);
  FRIEND_TEST(OpenVPNDriverTest, InitOptionsNoHost);
  FRIEND_TEST(OpenVPNDriverTest, ParseForeignOption);
  FRIEND_TEST(OpenVPNDriverTest, ParseForeignOptions);
  FRIEND_TEST(OpenVPNDriverTest, ParseIPConfiguration);
  FRIEND_TEST(OpenVPNDriverTest, ParseRouteOption);
  FRIEND_TEST(OpenVPNDriverTest, SetRoutes);

  // The map is a sorted container that allows us to iterate through the options
  // in order.
  typedef std::map<int, std::string> ForeignOptions;
  typedef std::map<int, IPConfig::Route> RouteOptions;

  static void ParseIPConfiguration(
      const std::map<std::string, std::string> &configuration,
      IPConfig::Properties *properties);
  static void ParseForeignOptions(const ForeignOptions &options,
                                  IPConfig::Properties *properties);
  static void ParseForeignOption(const std::string &option,
                                 IPConfig::Properties *properties);
  static IPConfig::Route *GetRouteOptionEntry(const std::string &prefix,
                                              const std::string &key,
                                              RouteOptions *routes);
  static void ParseRouteOption(const std::string &key,
                               const std::string &value,
                               RouteOptions *routes);
  static void SetRoutes(const RouteOptions &routes,
                        IPConfig::Properties *properties);

  void InitOptions(std::vector<std::string> *options, Error *error);

  void AppendValueOption(const std::string &property,
                         const std::string &option,
                         std::vector<std::string> *options);
  void AppendFlag(const std::string &property,
                  const std::string &option,
                  std::vector<std::string> *options);

  ControlInterface *control_;
  EventDispatcher *dispatcher_;
  Metrics *metrics_;
  Manager *manager_;
  DeviceInfo *device_info_;
  KeyValueStore args_;
  scoped_ptr<RPCTask> rpc_task_;
  std::string tunnel_interface_;
  VPNRefPtr device_;

  DISALLOW_COPY_AND_ASSIGN(OpenVPNDriver);
};

}  // namespace shill

#endif  // SHILL_OPENVPN_DRIVER_
