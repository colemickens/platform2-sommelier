// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_OPENVPN_DRIVER_
#define SHILL_OPENVPN_DRIVER_

#include <map>
#include <string>
#include <vector>

#include <base/file_path.h>
#include <base/memory/scoped_ptr.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST

#include "shill/glib.h"
#include "shill/ipconfig.h"
#include "shill/refptr_types.h"
#include "shill/rpc_task.h"
#include "shill/service.h"
#include "shill/sockets.h"
#include "shill/vpn_driver.h"

namespace base {

template<typename T>
class WeakPtr;

}  // namespace base;

namespace shill {

class ControlInterface;
class DeviceInfo;
class Error;
class Metrics;
class NSS;
class OpenVPNManagementServer;
class ProcessKiller;

class OpenVPNDriver : public VPNDriver,
                      public RPCTaskDelegate {
 public:
  OpenVPNDriver(ControlInterface *control,
                EventDispatcher *dispatcher,
                Metrics *metrics,
                Manager *manager,
                DeviceInfo *device_info,
                GLib *glib);
  virtual ~OpenVPNDriver();

  // Inherited from VPNDriver. |Connect| initiates the VPN connection by
  // creating a tunnel device. When the device index becomes available, this
  // instance is notified through |ClaimInterface| and resumes the connection
  // process by setting up and spawning an external 'openvpn' process. IP
  // configuration settings are passed back from the external process through
  // the |Notify| RPC service method.
  virtual void Connect(const VPNServiceRefPtr &service, Error *error);
  virtual bool ClaimInterface(const std::string &link_name,
                              int interface_index);
  virtual void Disconnect();
  virtual void OnConnectionDisconnected();
  virtual std::string GetProviderType() const;

  virtual void OnReconnecting();

  virtual void Cleanup(Service::ConnectState state);

  // Returns true if an opton was appended.
  bool AppendValueOption(const std::string &property,
                         const std::string &option,
                         std::vector<std::string> *options);

  // Returns true if a flag was appended.
  bool AppendFlag(const std::string &property,
                  const std::string &option,
                  std::vector<std::string> *options);

 private:
  friend class OpenVPNDriverTest;
  FRIEND_TEST(OpenVPNDriverTest, ClaimInterface);
  FRIEND_TEST(OpenVPNDriverTest, Cleanup);
  FRIEND_TEST(OpenVPNDriverTest, Connect);
  FRIEND_TEST(OpenVPNDriverTest, ConnectTunnelFailure);
  FRIEND_TEST(OpenVPNDriverTest, DeleteInterface);
  FRIEND_TEST(OpenVPNDriverTest, Disconnect);
  FRIEND_TEST(OpenVPNDriverTest, GetRouteOptionEntry);
  FRIEND_TEST(OpenVPNDriverTest, InitEnvironment);
  FRIEND_TEST(OpenVPNDriverTest, InitLoggingOptions);
  FRIEND_TEST(OpenVPNDriverTest, InitManagementChannelOptions);
  FRIEND_TEST(OpenVPNDriverTest, InitNSSOptions);
  FRIEND_TEST(OpenVPNDriverTest, InitOptions);
  FRIEND_TEST(OpenVPNDriverTest, InitOptionsNoHost);
  FRIEND_TEST(OpenVPNDriverTest, InitPKCS11Options);
  FRIEND_TEST(OpenVPNDriverTest, Notify);
  FRIEND_TEST(OpenVPNDriverTest, NotifyFail);
  FRIEND_TEST(OpenVPNDriverTest, OnConnectionDisconnected);
  FRIEND_TEST(OpenVPNDriverTest, OnDefaultServiceChanged);
  FRIEND_TEST(OpenVPNDriverTest, OnOpenVPNDied);
  FRIEND_TEST(OpenVPNDriverTest, OnReconnecting);
  FRIEND_TEST(OpenVPNDriverTest, ParseForeignOption);
  FRIEND_TEST(OpenVPNDriverTest, ParseForeignOptions);
  FRIEND_TEST(OpenVPNDriverTest, ParseIPConfiguration);
  FRIEND_TEST(OpenVPNDriverTest, ParseLSBRelease);
  FRIEND_TEST(OpenVPNDriverTest, ParseRouteOption);
  FRIEND_TEST(OpenVPNDriverTest, SetRoutes);
  FRIEND_TEST(OpenVPNDriverTest, SpawnOpenVPN);
  FRIEND_TEST(OpenVPNDriverTest, VerifyPaths);

  static const char kOpenVPNPath[];
  static const char kOpenVPNScript[];
  static const Property kProperties[];

  static const char kLSBReleaseFile[];
  static const char kChromeOSReleaseName[];
  static const char kChromeOSReleaseVersion[];

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
                                 std::vector<std::string> *domain_search,
                                 std::vector<std::string> *dns_servers);
  static IPConfig::Route *GetRouteOptionEntry(const std::string &prefix,
                                              const std::string &key,
                                              RouteOptions *routes);
  static void ParseRouteOption(const std::string &key,
                               const std::string &value,
                               RouteOptions *routes);
  static void SetRoutes(const RouteOptions &routes,
                        IPConfig::Properties *properties);

  void InitOptions(std::vector<std::string> *options, Error *error);
  bool InitNSSOptions(std::vector<std::string> *options, Error *error);
  void InitPKCS11Options(std::vector<std::string> *options);
  bool InitManagementChannelOptions(
      std::vector<std::string> *options, Error *error);
  void InitLoggingOptions(std::vector<std::string> *options);

  void InitEnvironment(std::vector<std::string> *environment);
  bool ParseLSBRelease(std::map<std::string, std::string> *lsb_release);

  bool SpawnOpenVPN();

  // Called when the openpvn process exits.
  static void OnOpenVPNDied(GPid pid, gint status, gpointer data);

  // Standalone callback used to delete the tunnel interface when the openvpn
  // process dies.
  static void DeleteInterface(const base::WeakPtr<DeviceInfo> &device_info,
                              int interface_index);

  // Inherit from VPNDriver to add custom properties.
  virtual KeyValueStore GetProvider(Error *error);

  // Implements RPCTaskDelegate.
  virtual void GetLogin(std::string *user, std::string *password);
  virtual void Notify(const std::string &reason,
                      const std::map<std::string, std::string> &dict);

  void OnDefaultServiceChanged(const ServiceRefPtr &service);

  ControlInterface *control_;
  Metrics *metrics_;
  DeviceInfo *device_info_;
  GLib *glib_;
  Sockets sockets_;
  scoped_ptr<OpenVPNManagementServer> management_server_;
  NSS *nss_;
  ProcessKiller *process_killer_;
  FilePath lsb_release_file_;

  VPNServiceRefPtr service_;
  scoped_ptr<RPCTask> rpc_task_;
  std::string tunnel_interface_;
  VPNRefPtr device_;
  FilePath tls_auth_file_;

  // The PID of the spawned openvpn process. May be 0 if no process has been
  // spawned yet or the process has died.
  int pid_;

  // Child exit watch callback source tag.
  unsigned int child_watch_tag_;

  // Default service watch callback tag.
  int default_service_callback_tag_;

  DISALLOW_COPY_AND_ASSIGN(OpenVPNDriver);
};

}  // namespace shill

#endif  // SHILL_OPENVPN_DRIVER_
