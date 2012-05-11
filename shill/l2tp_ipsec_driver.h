// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_L2TP_IPSEC_DRIVER_
#define SHILL_L2TP_IPSEC_DRIVER_

#include <vector>

#include <base/file_path.h>
#include <base/memory/scoped_ptr.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST

#include "shill/glib.h"
#include "shill/ipconfig.h"
#include "shill/rpc_task.h"
#include "shill/service.h"
#include "shill/vpn_driver.h"

namespace shill {

class ControlInterface;
class DeviceInfo;
class EventDispatcher;
class GLib;
class Metrics;
class NSS;

class L2TPIPSecDriver : public VPNDriver,
                        public RPCTaskDelegate {
 public:
  L2TPIPSecDriver(ControlInterface *control,
                  EventDispatcher *dispatcher,
                  Metrics *metrics,
                  Manager *manager,
                  DeviceInfo *device_info,
                  GLib *glib);
  virtual ~L2TPIPSecDriver();

  // Inherited from VPNDriver.
  virtual bool ClaimInterface(const std::string &link_name,
                              int interface_index);
  virtual void Connect(const VPNServiceRefPtr &service, Error *error);
  virtual void Disconnect();
  virtual void OnConnectionDisconnected();
  virtual std::string GetProviderType() const;

 private:
  friend class L2TPIPSecDriverTest;
  FRIEND_TEST(L2TPIPSecDriverTest, AppendFlag);
  FRIEND_TEST(L2TPIPSecDriverTest, AppendValueOption);
  FRIEND_TEST(L2TPIPSecDriverTest, Cleanup);
  FRIEND_TEST(L2TPIPSecDriverTest, DeletePSKFile);
  FRIEND_TEST(L2TPIPSecDriverTest, Disconnect);
  FRIEND_TEST(L2TPIPSecDriverTest, GetLogin);
  FRIEND_TEST(L2TPIPSecDriverTest, InitEnvironment);
  FRIEND_TEST(L2TPIPSecDriverTest, InitNSSOptions);
  FRIEND_TEST(L2TPIPSecDriverTest, InitOptions);
  FRIEND_TEST(L2TPIPSecDriverTest, InitOptionsNoHost);
  FRIEND_TEST(L2TPIPSecDriverTest, InitPSKOptions);
  FRIEND_TEST(L2TPIPSecDriverTest, Notify);
  FRIEND_TEST(L2TPIPSecDriverTest, NotifyFail);
  FRIEND_TEST(L2TPIPSecDriverTest, OnConnectionDisconnected);
  FRIEND_TEST(L2TPIPSecDriverTest, OnL2TPIPSecVPNDied);
  FRIEND_TEST(L2TPIPSecDriverTest, ParseIPConfiguration);
  FRIEND_TEST(L2TPIPSecDriverTest, SpawnL2TPIPSecVPN);
  FRIEND_TEST(L2TPIPSecDriverTest, VerifyPaths);

  static const char kPPPDPlugin[];
  static const char kL2TPIPSecVPNPath[];
  static const Property kProperties[];

  bool SpawnL2TPIPSecVPN(Error *error);

  void InitEnvironment(std::vector<std::string> *environment);

  bool InitOptions(std::vector<std::string> *options, Error *error);
  bool InitPSKOptions(std::vector<std::string> *options, Error *error);
  void InitNSSOptions(std::vector<std::string> *options);

  void Cleanup(Service::ConnectState state);

  void DeletePSKFile();

  // Returns true if an opton was appended.
  bool AppendValueOption(const std::string &property,
                         const std::string &option,
                         std::vector<std::string> *options);

  // Returns true if a flag was appended.
  bool AppendFlag(const std::string &property,
                  const std::string &true_option,
                  const std::string &false_option,
                  std::vector<std::string> *options);

  static void ParseIPConfiguration(
      const std::map<std::string, std::string> &configuration,
      IPConfig::Properties *properties,
      std::string *interface_name);

  // Called when the l2tpipsec_vpn process exits.
  static void OnL2TPIPSecVPNDied(GPid pid, gint status, gpointer data);

  // Inherit from VPNDriver to add custom properties.
  virtual KeyValueStore GetProvider(Error *error);

  // Implements RPCTaskDelegate.
  virtual void GetLogin(std::string *user, std::string *password);
  virtual void Notify(const std::string &reason,
                      const std::map<std::string, std::string> &dict);

  ControlInterface *control_;
  EventDispatcher *dispatcher_;
  Metrics *metrics_;
  DeviceInfo *device_info_;
  GLib *glib_;
  NSS *nss_;

  VPNServiceRefPtr service_;
  scoped_ptr<RPCTask> rpc_task_;
  FilePath psk_file_;
  VPNRefPtr device_;

  // The PID of the spawned l2tpipsec_vpn process. May be 0 if no process has
  // been spawned yet or the process has died.
  int pid_;

  // Child exit watch callback source tag.
  unsigned int child_watch_tag_;

  DISALLOW_COPY_AND_ASSIGN(L2TPIPSecDriver);
};

}  // namespace shill

#endif  // SHILL_L2TP_IPSEC_DRIVER_
