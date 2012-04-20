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
#include "shill/key_value_store.h"
#include "shill/refptr_types.h"
#include "shill/rpc_task.h"
#include "shill/service.h"
#include "shill/sockets.h"
#include "shill/vpn_driver.h"

namespace shill {

class ControlInterface;
class DeviceInfo;
class Error;
class EventDispatcher;
class Manager;
class Metrics;
class NSS;
class OpenVPNManagementServer;

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

  virtual bool Load(StoreInterface *storage, const std::string &storage_id);
  virtual bool Save(StoreInterface *storage, const std::string &storage_id);

  virtual void OnReconnecting();

  virtual void InitPropertyStore(PropertyStore *store);

  virtual std::string GetProviderType() const;

  virtual void Cleanup(Service::ConnectState state);

  void ClearMappedProperty(const size_t &index, Error *error);
  std::string GetMappedProperty(const size_t &index, Error *error);
  void SetMappedProperty(const size_t &index,
                         const std::string &value,
                         Error *error);
  Stringmap GetProvider(Error *error);

  KeyValueStore *args() { return &args_; }

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
  FRIEND_TEST(OpenVPNDriverTest, Disconnect);
  FRIEND_TEST(OpenVPNDriverTest, GetRouteOptionEntry);
  FRIEND_TEST(OpenVPNDriverTest, InitManagementChannelOptions);
  FRIEND_TEST(OpenVPNDriverTest, InitNSSOptions);
  FRIEND_TEST(OpenVPNDriverTest, InitOptions);
  FRIEND_TEST(OpenVPNDriverTest, InitOptionsNoHost);
  FRIEND_TEST(OpenVPNDriverTest, InitPKCS11Options);
  FRIEND_TEST(OpenVPNDriverTest, Notify);
  FRIEND_TEST(OpenVPNDriverTest, NotifyFail);
  FRIEND_TEST(OpenVPNDriverTest, OnOpenVPNDied);
  FRIEND_TEST(OpenVPNDriverTest, OnReconnecting);
  FRIEND_TEST(OpenVPNDriverTest, ParseForeignOption);
  FRIEND_TEST(OpenVPNDriverTest, ParseForeignOptions);
  FRIEND_TEST(OpenVPNDriverTest, ParseIPConfiguration);
  FRIEND_TEST(OpenVPNDriverTest, ParseRouteOption);
  FRIEND_TEST(OpenVPNDriverTest, PinHostRoute);
  FRIEND_TEST(OpenVPNDriverTest, SetRoutes);
  FRIEND_TEST(OpenVPNDriverTest, SpawnOpenVPN);
  FRIEND_TEST(OpenVPNDriverTest, VerifyPaths);

  struct Property {
    enum Flags {
      kEphemeral = 1 << 0,
      kCrypted = 1 << 1,
    };

    const char *property;
    int flags;
  };

  static const char kOpenVPNPath[];
  static const char kOpenVPNScript[];
  static const Property kProperties[];

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
  bool InitNSSOptions(std::vector<std::string> *options, Error *error);
  void InitPKCS11Options(std::vector<std::string> *options);
  bool InitManagementChannelOptions(
      std::vector<std::string> *options, Error *error);

  bool PinHostRoute(const IPConfig::Properties &properties);

  bool SpawnOpenVPN();

  // Called when the openpvn process exits.
  static void OnOpenVPNDied(GPid pid, gint status, gpointer data);

  // Implements RPCTaskDelegate.
  virtual void GetLogin(std::string *user, std::string *password);
  virtual void Notify(const std::string &reason,
                      const std::map<std::string, std::string> &dict);

  ControlInterface *control_;
  EventDispatcher *dispatcher_;
  Metrics *metrics_;
  Manager *manager_;
  DeviceInfo *device_info_;
  GLib *glib_;
  KeyValueStore args_;
  Sockets sockets_;
  scoped_ptr<OpenVPNManagementServer> management_server_;
  NSS *nss_;

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

  DISALLOW_COPY_AND_ASSIGN(OpenVPNDriver);
};

}  // namespace shill

#endif  // SHILL_OPENVPN_DRIVER_
