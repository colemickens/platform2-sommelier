// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/openvpn_driver.h"

#include <arpa/inet.h>

#include <base/file_util.h>
#include <base/logging.h>
#include <base/string_number_conversions.h>
#include <base/string_split.h>
#include <base/string_util.h>
#include <chromeos/dbus/service_constants.h>

#include "shill/connection.h"
#include "shill/device_info.h"
#include "shill/dhcp_config.h"
#include "shill/error.h"
#include "shill/manager.h"
#include "shill/openvpn_management_server.h"
#include "shill/property_accessor.h"
#include "shill/rpc_task.h"
#include "shill/sockets.h"
#include "shill/store_interface.h"
#include "shill/vpn.h"
#include "shill/vpn_service.h"

using base::SplitString;
using std::map;
using std::string;
using std::vector;

namespace shill {

namespace {
const char kOpenVPNForeignOptionPrefix[] = "foreign_option_";
const char kOpenVPNIfconfigBroadcast[] = "ifconfig_broadcast";
const char kOpenVPNIfconfigLocal[] = "ifconfig_local";
const char kOpenVPNIfconfigNetmask[] = "ifconfig_netmask";
const char kOpenVPNIfconfigRemote[] = "ifconfig_remote";
const char kOpenVPNRouteOptionPrefix[] = "route_";
const char kOpenVPNRouteVPNGateway[] = "route_vpn_gateway";
const char kOpenVPNTrustedIP[] = "trusted_ip";
const char kOpenVPNTunMTU[] = "tun_mtu";

// TODO(petkov): Move to chromeos/dbus/service_constants.h.
const char kOpenVPNCertProperty[] = "OpenVPN.Cert";
const char kOpenVPNKeyProperty[] = "OpenVPN.Key";
const char kOpenVPNPingProperty[] = "OpenVPN.Ping";
const char kOpenVPNPingExitProperty[] = "OpenVPN.PingExit";
const char kOpenVPNPingRestartProperty[] = "OpenVPN.PingRestart";
const char kOpenVPNTLSAuthProperty[] = "OpenVPN.TLSAuth";
const char kOpenVPNVerbProperty[] = "OpenVPN.Verb";
const char kVPNMTUProperty[] = "VPN.MTU";

}  // namespace

// static
const char OpenVPNDriver::kOpenVPNPath[] = "/usr/sbin/openvpn";
// static
const char OpenVPNDriver::kOpenVPNScript[] = SCRIPTDIR "/openvpn-script";
// static
const OpenVPNDriver::Property OpenVPNDriver::kProperties[] = {
  { flimflam::kOpenVPNAuthNoCacheProperty, false },
  { flimflam::kOpenVPNAuthProperty, false },
  { flimflam::kOpenVPNAuthRetryProperty, false },
  { flimflam::kOpenVPNAuthUserPassProperty, false },
  { flimflam::kOpenVPNCaCertNSSProperty, false },
  { flimflam::kOpenVPNCaCertProperty, false },
  { flimflam::kOpenVPNCipherProperty, false },
  { flimflam::kOpenVPNCompLZOProperty, false },
  { flimflam::kOpenVPNCompNoAdaptProperty, false },
  { flimflam::kOpenVPNKeyDirectionProperty, false },
  { flimflam::kOpenVPNNsCertTypeProperty, false },
  { flimflam::kOpenVPNPasswordProperty, true },
  { flimflam::kOpenVPNPinProperty, false },
  { flimflam::kOpenVPNPortProperty, false },
  { flimflam::kOpenVPNProtoProperty, false },
  { flimflam::kOpenVPNProviderProperty, false },
  { flimflam::kOpenVPNPushPeerInfoProperty, false },
  { flimflam::kOpenVPNRemoteCertEKUProperty, false },
  { flimflam::kOpenVPNRemoteCertKUProperty, false },
  { flimflam::kOpenVPNRemoteCertTLSProperty, false },
  { flimflam::kOpenVPNRenegSecProperty, false },
  { flimflam::kOpenVPNServerPollTimeoutProperty, false },
  { flimflam::kOpenVPNShaperProperty, false },
  { flimflam::kOpenVPNStaticChallengeProperty, false },
  { flimflam::kOpenVPNTLSAuthContentsProperty, false },
  { flimflam::kOpenVPNTLSRemoteProperty, false },
  { flimflam::kOpenVPNUserProperty, false },
  { flimflam::kProviderHostProperty, false },
  { flimflam::kProviderNameProperty, false },
  { flimflam::kProviderTypeProperty, false },
  { kOpenVPNCertProperty, false },
  { kOpenVPNKeyProperty, false },
  { kOpenVPNPingExitProperty, false },
  { kOpenVPNPingProperty, false },
  { kOpenVPNPingRestartProperty, false },
  { kOpenVPNTLSAuthProperty, false },
  { kOpenVPNVerbProperty, false },
  { kVPNMTUProperty, false },
};

OpenVPNDriver::OpenVPNDriver(ControlInterface *control,
                             EventDispatcher *dispatcher,
                             Metrics *metrics,
                             Manager *manager,
                             DeviceInfo *device_info,
                             GLib *glib,
                             const KeyValueStore &args)
    : control_(control),
      dispatcher_(dispatcher),
      metrics_(metrics),
      manager_(manager),
      device_info_(device_info),
      glib_(glib),
      args_(args),
      management_server_(new OpenVPNManagementServer(this, glib)),
      pid_(0),
      child_watch_tag_(0) {}

OpenVPNDriver::~OpenVPNDriver() {
  Cleanup(Service::kStateIdle);
}

void OpenVPNDriver::Cleanup(Service::ConnectState state) {
  VLOG(2) << __func__ << "(" << Service::ConnectStateToString(state) << ")";
  management_server_->Stop();
  if (!tls_auth_file_.empty()) {
    file_util::Delete(tls_auth_file_, false);
    tls_auth_file_.clear();
  }
  if (child_watch_tag_) {
    glib_->SourceRemove(child_watch_tag_);
    child_watch_tag_ = 0;
    CHECK(pid_);
    kill(pid_, SIGTERM);
  }
  if (pid_) {
    glib_->SpawnClosePID(pid_);
    pid_ = 0;
  }
  rpc_task_.reset();
  if (device_) {
    int interface_index = device_->interface_index();
    device_->SetEnabled(false);
    device_ = NULL;
    device_info_->DeleteInterface(interface_index);
  }
  tunnel_interface_.clear();
  if (service_) {
    service_->SetState(state);
    service_ = NULL;
  }
}

bool OpenVPNDriver::SpawnOpenVPN() {
  VLOG(2) << __func__ << "(" << tunnel_interface_ << ")";

  vector<string> options;
  Error error;
  InitOptions(&options, &error);
  if (error.IsFailure()) {
    return false;
  }
  VLOG(2) << "OpenVPN process options: " << JoinString(options, ' ');

  // TODO(petkov): This code needs to be abstracted away in a separate external
  // process module (crosbug.com/27131).
  vector<char *> process_args;
  process_args.push_back(const_cast<char *>(kOpenVPNPath));
  for (vector<string>::const_iterator it = options.begin();
       it != options.end(); ++it) {
    process_args.push_back(const_cast<char *>(it->c_str()));
  }
  process_args.push_back(NULL);
  char *envp[1] = { NULL };

  CHECK(!pid_);
  // Redirect all openvpn output to stderr.
  int stderr_fd = fileno(stderr);
  if (!glib_->SpawnAsyncWithPipesCWD(process_args.data(),
                                     envp,
                                     G_SPAWN_DO_NOT_REAP_CHILD,
                                     NULL,
                                     NULL,
                                     &pid_,
                                     NULL,
                                     &stderr_fd,
                                     &stderr_fd,
                                     NULL)) {
    LOG(ERROR) << "Unable to spawn: " << kOpenVPNPath;
    return false;
  }
  CHECK(!child_watch_tag_);
  child_watch_tag_ = glib_->ChildWatchAdd(pid_, OnOpenVPNDied, this);
  return true;
}

// static
void OpenVPNDriver::OnOpenVPNDied(GPid pid, gint status, gpointer data) {
  VLOG(2) << __func__ << "(" << pid << ", "  << status << ")";
  OpenVPNDriver *me = reinterpret_cast<OpenVPNDriver *>(data);
  me->child_watch_tag_ = 0;
  CHECK_EQ(pid, me->pid_);
  me->Cleanup(Service::kStateFailure);
  // TODO(petkov): Figure if we need to restart the connection.
}

bool OpenVPNDriver::ClaimInterface(const string &link_name,
                                   int interface_index) {
  if (link_name != tunnel_interface_) {
    return false;
  }

  VLOG(2) << "Claiming " << link_name << " for OpenVPN tunnel";

  CHECK(!device_);
  device_ = new VPN(control_, dispatcher_, metrics_, manager_,
                    link_name, interface_index);

  device_->SetEnabled(true);
  device_->SelectService(service_);

  rpc_task_.reset(new RPCTask(control_, this));
  if (!SpawnOpenVPN()) {
    Cleanup(Service::kStateFailure);
  }
  return true;
}

void OpenVPNDriver::Notify(const string &reason,
                           const map<string, string> &dict) {
  VLOG(2) << __func__ << "(" << reason << ")";
  if (reason != "up") {
    device_->OnDisconnected();
    return;
  }
  IPConfig::Properties properties;
  ParseIPConfiguration(dict, &properties);
  PinHostRoute(properties);

  device_->UpdateIPConfig(properties);
}

// static
void OpenVPNDriver::ParseIPConfiguration(
    const map<string, string> &configuration,
    IPConfig::Properties *properties) {
  ForeignOptions foreign_options;
  RouteOptions routes;
  properties->address_family = IPAddress::kFamilyIPv4;
  properties->subnet_prefix = IPAddress::GetMaxPrefixLength(
      properties->address_family);
  for (map<string, string>::const_iterator it = configuration.begin();
       it != configuration.end(); ++it) {
    const string &key = it->first;
    const string &value = it->second;
    VLOG(2) << "Processing: " << key << " -> " << value;
    if (LowerCaseEqualsASCII(key, kOpenVPNIfconfigLocal)) {
      properties->address = value;
    } else if (LowerCaseEqualsASCII(key, kOpenVPNIfconfigBroadcast)) {
      properties->broadcast_address = value;
    } else if (LowerCaseEqualsASCII(key, kOpenVPNIfconfigNetmask)) {
      properties->subnet_prefix =
          IPAddress::GetPrefixLengthFromMask(properties->address_family, value);
    } else if (LowerCaseEqualsASCII(key, kOpenVPNIfconfigRemote)) {
      properties->peer_address = value;
    } else if (LowerCaseEqualsASCII(key, kOpenVPNRouteVPNGateway)) {
      properties->gateway = value;
    } else if (LowerCaseEqualsASCII(key, kOpenVPNTrustedIP)) {
      properties->trusted_ip = value;
    } else if (LowerCaseEqualsASCII(key, kOpenVPNTunMTU)) {
      int mtu = 0;
      if (base::StringToInt(value, &mtu) && mtu >= DHCPConfig::kMinMTU) {
        properties->mtu = mtu;
      } else {
        LOG(ERROR) << "MTU " << value << " ignored.";
      }
    } else if (StartsWithASCII(key, kOpenVPNForeignOptionPrefix, false)) {
      const string suffix = key.substr(strlen(kOpenVPNForeignOptionPrefix));
      int order = 0;
      if (base::StringToInt(suffix, &order)) {
        foreign_options[order] = value;
      } else {
        LOG(ERROR) << "Ignored unexpected foreign option suffix: " << suffix;
      }
    } else if (StartsWithASCII(key, kOpenVPNRouteOptionPrefix, false)) {
      ParseRouteOption(key.substr(strlen(kOpenVPNRouteOptionPrefix)),
                       value, &routes);
    } else {
      VLOG(2) << "Key ignored.";
    }
  }
  ParseForeignOptions(foreign_options, properties);
  SetRoutes(routes, properties);
}

// static
void OpenVPNDriver::ParseForeignOptions(const ForeignOptions &options,
                                        IPConfig::Properties *properties) {
  for (ForeignOptions::const_iterator it = options.begin();
       it != options.end(); ++it) {
    ParseForeignOption(it->second, properties);
  }
}

// static
void OpenVPNDriver::ParseForeignOption(const string &option,
                                       IPConfig::Properties *properties) {
  VLOG(2) << __func__ << "(" << option << ")";
  vector<string> tokens;
  SplitString(option, ' ', &tokens);
  if (tokens.size() != 3 || !LowerCaseEqualsASCII(tokens[0], "dhcp-option")) {
    return;
  }
  if (LowerCaseEqualsASCII(tokens[1], "domain")) {
    properties->domain_search.push_back(tokens[2]);
  } else if (LowerCaseEqualsASCII(tokens[1], "dns")) {
    properties->dns_servers.push_back(tokens[2]);
  }
}

// static
IPConfig::Route *OpenVPNDriver::GetRouteOptionEntry(
    const string &prefix, const string &key, RouteOptions *routes) {
  int order = 0;
  if (!StartsWithASCII(key, prefix, false) ||
      !base::StringToInt(key.substr(prefix.size()), &order)) {
    return NULL;
  }
  return &(*routes)[order];
}

// static
void OpenVPNDriver::ParseRouteOption(
    const string &key, const string &value, RouteOptions *routes) {
  IPConfig::Route *route = GetRouteOptionEntry("network_", key, routes);
  if (route) {
    route->host = value;
    return;
  }
  route = GetRouteOptionEntry("netmask_", key, routes);
  if (route) {
    route->netmask = value;
    return;
  }
  route = GetRouteOptionEntry("gateway_", key, routes);
  if (route) {
    route->gateway = value;
    return;
  }
  LOG(WARNING) << "Unknown route option ignored: " << key;
}

// static
void OpenVPNDriver::SetRoutes(const RouteOptions &routes,
                              IPConfig::Properties *properties) {
  for (RouteOptions::const_iterator it = routes.begin();
       it != routes.end(); ++it) {
    const IPConfig::Route &route = it->second;
    if (route.host.empty() || route.netmask.empty() || route.gateway.empty()) {
      LOG(WARNING) << "Ignoring incomplete route: " << it->first;
      continue;
    }
    properties->routes.push_back(route);
  }
}

void OpenVPNDriver::Connect(const VPNServiceRefPtr &service,
                            Error *error) {
  service_ = service;
  service_->SetState(Service::kStateConfiguring);
  if (!device_info_->CreateTunnelInterface(&tunnel_interface_)) {
    Error::PopulateAndLog(
        error, Error::kInternalError, "Could not create tunnel interface.");
    Cleanup(Service::kStateFailure);
  }
  // Wait for the ClaimInterface callback to continue the connection process.
}

void OpenVPNDriver::InitOptions(vector<string> *options, Error *error) {
  string vpnhost = args_.LookupString(flimflam::kProviderHostProperty, "");
  if (vpnhost.empty()) {
    Error::PopulateAndLog(
        error, Error::kInvalidArguments, "VPN host not specified.");
    return;
  }
  options->push_back("--client");
  options->push_back("--tls-client");
  options->push_back("--remote");
  options->push_back(vpnhost);
  options->push_back("--nobind");
  options->push_back("--persist-key");
  options->push_back("--persist-tun");

  CHECK(!tunnel_interface_.empty());
  options->push_back("--dev");
  options->push_back(tunnel_interface_);
  options->push_back("--dev-type");
  options->push_back("tun");
  options->push_back("--syslog");

  // TODO(petkov): Enable verbosity based on shill logging options too.
  AppendValueOption(kOpenVPNVerbProperty, "--verb", options);

  AppendValueOption(kVPNMTUProperty, "--mtu", options);
  AppendValueOption(flimflam::kOpenVPNProtoProperty, "--proto", options);
  AppendValueOption(flimflam::kOpenVPNPortProperty, "--port", options);
  AppendValueOption(kOpenVPNTLSAuthProperty, "--tls-auth", options);
  {
    string contents =
        args_.LookupString(flimflam::kOpenVPNTLSAuthContentsProperty, "");
    if (!contents.empty()) {
      if (!file_util::CreateTemporaryFile(&tls_auth_file_) ||
          file_util::WriteFile(
              tls_auth_file_, contents.data(), contents.size()) !=
          static_cast<int>(contents.size())) {
        Error::PopulateAndLog(
            error, Error::kInternalError, "Unable to setup tls-auth file.");
        return;
      }
      options->push_back("--tls-auth");
      options->push_back(tls_auth_file_.value());
    }
  }
  AppendValueOption(
      flimflam::kOpenVPNTLSRemoteProperty, "--tls-remote", options);
  AppendValueOption(flimflam::kOpenVPNCipherProperty, "--cipher", options);
  AppendValueOption(flimflam::kOpenVPNAuthProperty, "--auth", options);
  AppendFlag(flimflam::kOpenVPNAuthNoCacheProperty, "--auth-nocache", options);
  AppendValueOption(
      flimflam::kOpenVPNAuthRetryProperty, "--auth-retry", options);
  AppendFlag(flimflam::kOpenVPNCompLZOProperty, "--comp-lzo", options);
  AppendFlag(flimflam::kOpenVPNCompNoAdaptProperty, "--comp-noadapt", options);
  AppendFlag(
      flimflam::kOpenVPNPushPeerInfoProperty, "--push-peer-info", options);
  AppendValueOption(flimflam::kOpenVPNRenegSecProperty, "--reneg-sec", options);
  AppendValueOption(flimflam::kOpenVPNShaperProperty, "--shaper", options);
  AppendValueOption(flimflam::kOpenVPNServerPollTimeoutProperty,
                    "--server-poll-timeout", options);

  // TODO(petkov): Implement this.
  LOG_IF(ERROR, args_.ContainsString(flimflam::kOpenVPNCaCertNSSProperty))
      << "Support for NSS CA not implemented yet.";

  // Client-side ping support.
  AppendValueOption(kOpenVPNPingProperty, "--ping", options);
  AppendValueOption(kOpenVPNPingExitProperty, "--ping-exit", options);
  AppendValueOption(kOpenVPNPingRestartProperty, "--ping-restart", options);

  AppendValueOption(flimflam::kOpenVPNCaCertProperty, "--ca", options);
  AppendValueOption(kOpenVPNCertProperty, "--cert", options);
  AppendValueOption(
      flimflam::kOpenVPNNsCertTypeProperty, "--ns-cert-type", options);
  AppendValueOption(kOpenVPNKeyProperty, "--key", options);

  // TODO(petkov): Implement this.
  LOG_IF(ERROR, args_.ContainsString(flimflam::kOpenVPNClientCertIdProperty))
      << "Support for PKCS#11 (--pkcs11-id and --pkcs11-providers) "
      << "not implemented yet.";

  // TLS suport.
  string remote_cert_tls =
      args_.LookupString(flimflam::kOpenVPNRemoteCertTLSProperty, "");
  if (remote_cert_tls.empty()) {
    remote_cert_tls = "server";
  }
  if (remote_cert_tls != "none") {
    options->push_back("--remote-cert-tls");
    options->push_back(remote_cert_tls);
  }

  // This is an undocumented command line argument that works like a .cfg file
  // entry. TODO(sleffler): Maybe roll this into --tls-auth?
  AppendValueOption(
      flimflam::kOpenVPNKeyDirectionProperty, "--key-direction", options);
  // TODO(sleffler): Support more than one eku parameter.
  AppendValueOption(
      flimflam::kOpenVPNRemoteCertEKUProperty, "--remote-cert-eku", options);
  AppendValueOption(
      flimflam::kOpenVPNRemoteCertKUProperty, "--remote-cert-ku", options);

  if (!management_server_->Start(dispatcher_, &sockets_, options)) {
    Error::PopulateAndLog(
        error, Error::kInternalError, "Unable to setup management channel.");
    return;
  }

  // Setup openvpn-script options and RPC information required to send back
  // Layer 3 configuration.
  options->push_back("--setenv");
  options->push_back("CONNMAN_BUSNAME");
  options->push_back(rpc_task_->GetRpcConnectionIdentifier());
  options->push_back("--setenv");
  options->push_back("CONNMAN_INTERFACE");
  options->push_back(rpc_task_->GetRpcInterfaceIdentifier());
  options->push_back("--setenv");
  options->push_back("CONNMAN_PATH");
  options->push_back(rpc_task_->GetRpcIdentifier());
  options->push_back("--script-security");
  options->push_back("2");
  options->push_back("--up");
  options->push_back(kOpenVPNScript);
  options->push_back("--up-restart");

  // Disable openvpn handling since we do route+ifconfig work.
  options->push_back("--route-noexec");
  options->push_back("--ifconfig-noexec");

  // Drop root privileges on connection and enable callback scripts to send
  // notify messages.
  options->push_back("--user");
  options->push_back("openvpn");
  options->push_back("--group");
  options->push_back("openvpn");
}

bool OpenVPNDriver::AppendValueOption(
    const string &property, const string &option, vector<string> *options) {
  string value = args_.LookupString(property, "");
  if (!value.empty()) {
    options->push_back(option);
    options->push_back(value);
    return true;
  }
  return false;
}

bool OpenVPNDriver::AppendFlag(
    const string &property, const string &option, vector<string> *options) {
  if (args_.ContainsString(property)) {
    options->push_back(option);
    return true;
  }
  return false;
}

bool OpenVPNDriver::PinHostRoute(const IPConfig::Properties &properties) {
  if (properties.gateway.empty() || properties.trusted_ip.empty()) {
    return false;
  }

  IPAddress trusted_ip(properties.address_family);
  if (!trusted_ip.SetAddressFromString(properties.trusted_ip)) {
    LOG(ERROR) << "Failed to parse trusted_ip "
               << properties.trusted_ip << "; ignored.";
    return false;
  }

  ServiceRefPtr default_service = manager_->GetDefaultService();
  if (!default_service) {
    LOG(ERROR) << "No default service exists.";
    return false;
  }

  CHECK(default_service->connection());
  return default_service->connection()->RequestHostRoute(trusted_ip);
}

void OpenVPNDriver::Disconnect() {
  VLOG(2) << __func__;
  Cleanup(Service::kStateIdle);
}

void OpenVPNDriver::OnReconnecting() {
  VLOG(2) << __func__;
  if (device_) {
    device_->OnDisconnected();
  }
  if (service_) {
    service_->SetState(Service::kStateAssociating);
  }
}

bool OpenVPNDriver::Load(StoreInterface *storage, const string &storage_id) {
  for (size_t i = 0; i < arraysize(kProperties); i++) {
    const string property = kProperties[i].property;
    string value;
    bool loaded = kProperties[i].crypted ?
        storage->GetCryptedString(storage_id, property, &value) :
        storage->GetString(storage_id, property, &value);
    if (loaded) {
      args_.SetString(property, value);
    } else {
      args_.RemoveString(property);
    }
  }
  return true;
}

bool OpenVPNDriver::Save(StoreInterface *storage, const string &storage_id) {
  for (size_t i = 0; i < arraysize(kProperties); i++) {
    const string property = kProperties[i].property;
    const string value = args_.LookupString(property, "");
    if (value.empty()) {
      storage->DeleteKey(storage_id, property);
    } else if (kProperties[i].crypted) {
      storage->SetCryptedString(storage_id, property, value);
    } else {
      storage->SetString(storage_id, property, value);
    }
  }
  return true;
}

void OpenVPNDriver::InitPropertyStore(PropertyStore *store) {
  for (size_t i = 0; i < arraysize(kProperties); i++) {
    store->RegisterDerivedString(
        kProperties[i].property,
        StringAccessor(
            new CustomMappedAccessor<OpenVPNDriver, string, size_t>(
                this,
                &OpenVPNDriver::ClearMappedProperty,
                &OpenVPNDriver::GetMappedProperty,
                &OpenVPNDriver::SetMappedProperty,
                i)));
  }

  // TODO(pstew): Add the PassphraseRequired and PSKRequired properties.
  // crosbug.com/27323
}

void OpenVPNDriver::ClearMappedProperty(const size_t &index,
                                        Error *error) {
  CHECK(index < arraysize(kProperties));
  if (args_.ContainsString(kProperties[index].property)) {
    args_.RemoveString(kProperties[index].property);
  } else {
    error->Populate(Error::kNotFound, "Property is not set");
  }
}

string OpenVPNDriver::GetMappedProperty(const size_t &index,
                                        Error *error) {
  CHECK(index < arraysize(kProperties));
  if (kProperties[index].crypted) {
    error->Populate(Error::kPermissionDenied, "Property is write-only");
    return string();
  }

  if (!args_.ContainsString(kProperties[index].property)) {
    error->Populate(Error::kNotFound, "Property is not set");
    return string();
  }

  return args_.LookupString(kProperties[index].property, "");
}

void OpenVPNDriver::SetMappedProperty(const size_t &index,
                                      const string &value,
                                      Error *error) {
  CHECK(index < arraysize(kProperties));
  args_.SetString(kProperties[index].property, value);
}


}  // namespace shill
