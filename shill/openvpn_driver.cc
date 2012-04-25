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
#include "shill/nss.h"
#include "shill/openvpn_management_server.h"
#include "shill/rpc_task.h"
#include "shill/scope_logger.h"
#include "shill/sockets.h"
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

const char kDefaultPKCS11Provider[] = "libchaps.so";

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
const VPNDriver::Property OpenVPNDriver::kProperties[] = {
  { flimflam::kOpenVPNAuthNoCacheProperty, 0 },
  { flimflam::kOpenVPNAuthProperty, 0 },
  { flimflam::kOpenVPNAuthRetryProperty, 0 },
  { flimflam::kOpenVPNAuthUserPassProperty, 0 },
  { flimflam::kOpenVPNCaCertNSSProperty, 0 },
  { flimflam::kOpenVPNCaCertProperty, 0 },
  { flimflam::kOpenVPNCipherProperty, 0 },
  { flimflam::kOpenVPNClientCertIdProperty,
    Property::kEphemeral | Property::kCrypted },
  { flimflam::kOpenVPNCompLZOProperty, 0 },
  { flimflam::kOpenVPNCompNoAdaptProperty, 0 },
  { flimflam::kOpenVPNKeyDirectionProperty, 0 },
  { flimflam::kOpenVPNNsCertTypeProperty, 0 },
  { flimflam::kOpenVPNOTPProperty, Property::kEphemeral | Property::kCrypted },
  { flimflam::kOpenVPNPasswordProperty, Property::kCrypted },
  { flimflam::kOpenVPNPinProperty, 0 },
  { flimflam::kOpenVPNPortProperty, 0 },
  { flimflam::kOpenVPNProtoProperty, 0 },
  { flimflam::kOpenVPNProviderProperty, 0 },
  { flimflam::kOpenVPNPushPeerInfoProperty, 0 },
  { flimflam::kOpenVPNRemoteCertEKUProperty, 0 },
  { flimflam::kOpenVPNRemoteCertKUProperty, 0 },
  { flimflam::kOpenVPNRemoteCertTLSProperty, 0 },
  { flimflam::kOpenVPNRenegSecProperty, 0 },
  { flimflam::kOpenVPNServerPollTimeoutProperty, 0 },
  { flimflam::kOpenVPNShaperProperty, 0 },
  { flimflam::kOpenVPNStaticChallengeProperty, 0 },
  { flimflam::kOpenVPNTLSAuthContentsProperty, 0 },
  { flimflam::kOpenVPNTLSRemoteProperty, 0 },
  { flimflam::kOpenVPNUserProperty, 0 },
  { flimflam::kProviderHostProperty, 0 },
  { flimflam::kProviderNameProperty, 0 },
  { flimflam::kProviderTypeProperty, 0 },
  { kOpenVPNCertProperty, 0 },
  { kOpenVPNKeyProperty, 0 },
  { kOpenVPNPingExitProperty, 0 },
  { kOpenVPNPingProperty, 0 },
  { kOpenVPNPingRestartProperty, 0 },
  { kOpenVPNTLSAuthProperty, 0 },
  { kOpenVPNVerbProperty, 0 },
  { kVPNMTUProperty, 0 },

  // Provided only for compatibility.  crosbug.com/29286
  { flimflam::kOpenVPNMgmtEnableProperty, 0 },
};

OpenVPNDriver::OpenVPNDriver(ControlInterface *control,
                             EventDispatcher *dispatcher,
                             Metrics *metrics,
                             Manager *manager,
                             DeviceInfo *device_info,
                             GLib *glib)
    : VPNDriver(manager, kProperties, arraysize(kProperties)),
      control_(control),
      dispatcher_(dispatcher),
      metrics_(metrics),
      device_info_(device_info),
      glib_(glib),
      management_server_(new OpenVPNManagementServer(this, glib)),
      nss_(NSS::GetInstance()),
      pid_(0),
      child_watch_tag_(0) {}

OpenVPNDriver::~OpenVPNDriver() {
  Cleanup(Service::kStateIdle);
}

void OpenVPNDriver::Cleanup(Service::ConnectState state) {
  SLOG(VPN, 2) << __func__ << "(" << Service::ConnectStateToString(state)
               << ")";
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
    device_->OnDisconnected();
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
  SLOG(VPN, 2) << __func__ << "(" << tunnel_interface_ << ")";

  vector<string> options;
  Error error;
  InitOptions(&options, &error);
  if (error.IsFailure()) {
    return false;
  }
  SLOG(VPN, 2) << "OpenVPN process options: " << JoinString(options, ' ');

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
  SLOG(VPN, 2) << __func__ << "(" << pid << ", "  << status << ")";
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

  SLOG(VPN, 2) << "Claiming " << link_name << " for OpenVPN tunnel";

  CHECK(!device_);
  device_ = new VPN(control_, dispatcher_, metrics_, manager(),
                    link_name, interface_index);

  device_->SetEnabled(true);
  device_->SelectService(service_);

  rpc_task_.reset(new RPCTask(control_, this));
  if (!SpawnOpenVPN()) {
    Cleanup(Service::kStateFailure);
  }
  return true;
}

void OpenVPNDriver::GetLogin(string */*user*/, string */*password*/) {
  NOTREACHED();
}

void OpenVPNDriver::Notify(const string &reason,
                           const map<string, string> &dict) {
  SLOG(VPN, 2) << __func__ << "(" << reason << ")";
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
    SLOG(VPN, 2) << "Processing: " << key << " -> " << value;
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
      SLOG(VPN, 2) << "Key ignored.";
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
  SLOG(VPN, 2) << __func__ << "(" << option << ")";
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
  string vpnhost = args()->LookupString(flimflam::kProviderHostProperty, "");
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

  InitLoggingOptions(options);

  AppendValueOption(kVPNMTUProperty, "--mtu", options);
  AppendValueOption(flimflam::kOpenVPNProtoProperty, "--proto", options);
  AppendValueOption(flimflam::kOpenVPNPortProperty, "--port", options);
  AppendValueOption(kOpenVPNTLSAuthProperty, "--tls-auth", options);
  {
    string contents =
        args()->LookupString(flimflam::kOpenVPNTLSAuthContentsProperty, "");
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

  if (!InitNSSOptions(options, error)) {
    return;
  }

  // Client-side ping support.
  AppendValueOption(kOpenVPNPingProperty, "--ping", options);
  AppendValueOption(kOpenVPNPingExitProperty, "--ping-exit", options);
  AppendValueOption(kOpenVPNPingRestartProperty, "--ping-restart", options);

  AppendValueOption(flimflam::kOpenVPNCaCertProperty, "--ca", options);
  AppendValueOption(kOpenVPNCertProperty, "--cert", options);
  AppendValueOption(
      flimflam::kOpenVPNNsCertTypeProperty, "--ns-cert-type", options);
  AppendValueOption(kOpenVPNKeyProperty, "--key", options);

  InitPKCS11Options(options);

  // TLS suport.
  string remote_cert_tls =
      args()->LookupString(flimflam::kOpenVPNRemoteCertTLSProperty, "");
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

  if (!InitManagementChannelOptions(options, error)) {
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

bool OpenVPNDriver::InitNSSOptions(vector<string> *options, Error *error) {
  string ca_cert =
      args()->LookupString(flimflam::kOpenVPNCaCertNSSProperty, "");
  if (!ca_cert.empty()) {
    if (!args()->LookupString(flimflam::kOpenVPNCaCertProperty, "").empty()) {
      Error::PopulateAndLog(error,
                            Error::kInvalidArguments,
                            "Can't specify both CACert and CACertNSS.");
      return false;
    }
    const string &vpnhost = args()->GetString(flimflam::kProviderHostProperty);
    vector<char> id(vpnhost.begin(), vpnhost.end());
    FilePath certfile = nss_->GetPEMCertfile(ca_cert, id);
    if (certfile.empty()) {
      LOG(ERROR) << "Unable to extract certificate: " << ca_cert;
    } else {
      options->push_back("--ca");
      options->push_back(certfile.value());
    }
  }
  return true;
}

void OpenVPNDriver::InitPKCS11Options(vector<string> *options) {
  string id = args()->LookupString(flimflam::kOpenVPNClientCertIdProperty, "");
  if (!id.empty()) {
    string provider =
        args()->LookupString(flimflam::kOpenVPNProviderProperty, "");
    if (provider.empty()) {
      provider = kDefaultPKCS11Provider;
    }
    options->push_back("--pkcs11-providers");
    options->push_back(provider);
    options->push_back("--pkcs11-id");
    options->push_back(id);
  }
}

bool OpenVPNDriver::InitManagementChannelOptions(
    vector<string> *options, Error *error) {
  if (!management_server_->Start(dispatcher_, &sockets_, options)) {
    Error::PopulateAndLog(
        error, Error::kInternalError, "Unable to setup management channel.");
    return false;
  }
  return true;
}

void OpenVPNDriver::InitLoggingOptions(vector<string> *options) {
  options->push_back("--syslog");

  string verb = args()->LookupString(kOpenVPNVerbProperty, "");
  if (verb.empty() && SLOG_IS_ON(VPN, 0)) {
    verb = "3";
  }
  if (!verb.empty()) {
    options->push_back("--verb");
    options->push_back(verb);
  }
}

bool OpenVPNDriver::AppendValueOption(
    const string &property, const string &option, vector<string> *options) {
  string value = args()->LookupString(property, "");
  if (!value.empty()) {
    options->push_back(option);
    options->push_back(value);
    return true;
  }
  return false;
}

bool OpenVPNDriver::AppendFlag(
    const string &property, const string &option, vector<string> *options) {
  if (args()->ContainsString(property)) {
    options->push_back(option);
    return true;
  }
  return false;
}

void OpenVPNDriver::Disconnect() {
  SLOG(VPN, 2) << __func__;
  Cleanup(Service::kStateIdle);
}

void OpenVPNDriver::OnReconnecting() {
  SLOG(VPN, 2) << __func__;
  if (device_) {
    device_->OnDisconnected();
  }
  if (service_) {
    service_->SetState(Service::kStateAssociating);
  }
}

string OpenVPNDriver::GetProviderType() const {
  return flimflam::kProviderOpenVpn;
}

}  // namespace shill
