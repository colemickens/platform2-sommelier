// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/openvpn_driver.h"

#include <arpa/inet.h>

#include <base/file_util.h>
#include <base/string_number_conversions.h>
#include <base/string_split.h>
#include <base/string_util.h>
#include <chromeos/dbus/service_constants.h>

#include "shill/certificate_file.h"
#include "shill/connection.h"
#include "shill/device_info.h"
#include "shill/dhcp_config.h"
#include "shill/error.h"
#include "shill/logging.h"
#include "shill/manager.h"
#include "shill/nss.h"
#include "shill/openvpn_management_server.h"
#include "shill/process_killer.h"
#include "shill/rpc_task.h"
#include "shill/sockets.h"
#include "shill/virtual_device.h"
#include "shill/vpn_service.h"

using base::Closure;
using base::FilePath;
using base::SplitString;
using base::Unretained;
using base::WeakPtr;
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

// Some configurations pass the netmask in the ifconfig_remote property.
// This is due to some servers not explicitly indicating that they are using
// a "broadcast mode" network instead of peer-to-peer.  See
// http://crbug.com/241264 for an example of this issue.
const char kSuspectedNetmaskPrefix[] = "255.";

}  // namespace

// static
const char OpenVPNDriver::kDefaultCACertificates[] =
    "/etc/ssl/certs/ca-certificates.crt";
// static
const char OpenVPNDriver::kOpenVPNPath[] = "/usr/sbin/openvpn";
// static
const char OpenVPNDriver::kOpenVPNScript[] = SHIMDIR "/openvpn-script";
// static
const VPNDriver::Property OpenVPNDriver::kProperties[] = {
  { flimflam::kOpenVPNAuthNoCacheProperty, 0 },
  { flimflam::kOpenVPNAuthProperty, 0 },
  { flimflam::kOpenVPNAuthRetryProperty, 0 },
  { flimflam::kOpenVPNAuthUserPassProperty, 0 },
  { flimflam::kOpenVPNCaCertNSSProperty, 0 },
  { flimflam::kOpenVPNCaCertProperty, 0 },
  { flimflam::kOpenVPNCipherProperty, 0 },
  { flimflam::kOpenVPNClientCertIdProperty, Property::kCredential },
  { flimflam::kOpenVPNCompLZOProperty, 0 },
  { flimflam::kOpenVPNCompNoAdaptProperty, 0 },
  { flimflam::kOpenVPNKeyDirectionProperty, 0 },
  { flimflam::kOpenVPNNsCertTypeProperty, 0 },
  { flimflam::kOpenVPNOTPProperty,
    Property::kEphemeral | Property::kCredential | Property::kWriteOnly },
  { flimflam::kOpenVPNPasswordProperty,
    Property::kCredential | Property::kWriteOnly },
  { flimflam::kOpenVPNPinProperty, Property::kCredential },
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
  { flimflam::kProviderTypeProperty, 0 },
  { kOpenVPNCaCertPemProperty, Property::kArray },
  { kOpenVPNCertProperty, 0 },
  { kOpenVPNExtraCertPemProperty, Property::kArray },
  { kOpenVPNKeyProperty, 0 },
  { kOpenVPNPingExitProperty, 0 },
  { kOpenVPNPingProperty, 0 },
  { kOpenVPNPingRestartProperty, 0 },
  { kOpenVPNTLSAuthProperty, 0 },
  { kOpenVPNVerbProperty, 0 },
  { kVPNMTUProperty, 0 },
};

const char OpenVPNDriver::kLSBReleaseFile[] = "/etc/lsb-release";
const char OpenVPNDriver::kChromeOSReleaseName[] = "CHROMEOS_RELEASE_NAME";
const char OpenVPNDriver::kChromeOSReleaseVersion[] =
    "CHROMEOS_RELEASE_VERSION";

// Directory where OpenVPN configuration files are exported while the
// process is running.
const char OpenVPNDriver::kDefaultOpenVPNConfigurationDirectory[] =
    RUNDIR "/openvpn_config";

const int OpenVPNDriver::kReconnectOfflineTimeoutSeconds = 2 * 60;
const int OpenVPNDriver::kReconnectTLSErrorTimeoutSeconds = 20;

OpenVPNDriver::OpenVPNDriver(ControlInterface *control,
                             EventDispatcher *dispatcher,
                             Metrics *metrics,
                             Manager *manager,
                             DeviceInfo *device_info,
                             GLib *glib)
    : VPNDriver(dispatcher, manager, kProperties, arraysize(kProperties)),
      control_(control),
      metrics_(metrics),
      device_info_(device_info),
      glib_(glib),
      management_server_(new OpenVPNManagementServer(this, glib)),
      nss_(NSS::GetInstance()),
      certificate_file_(new CertificateFile()),
      process_killer_(ProcessKiller::GetInstance()),
      lsb_release_file_(kLSBReleaseFile),
      openvpn_config_directory_(kDefaultOpenVPNConfigurationDirectory),
      pid_(0),
      child_watch_tag_(0),
      default_service_callback_tag_(0) {}

OpenVPNDriver::~OpenVPNDriver() {
  IdleService();
}

void OpenVPNDriver::IdleService() {
  Cleanup(Service::kStateIdle,
          Service::kFailureUnknown,
          Service::kErrorDetailsNone);
}

void OpenVPNDriver::FailService(Service::ConnectFailure failure,
                                const string &error_details) {
  Cleanup(Service::kStateFailure, failure, error_details);
}

void OpenVPNDriver::Cleanup(Service::ConnectState state,
                            Service::ConnectFailure failure,
                            const string &error_details) {
  SLOG(VPN, 2) << __func__ << "(" << Service::ConnectStateToString(state)
               << ", " << error_details << ")";
  StopConnectTimeout();
  if (child_watch_tag_) {
    glib_->SourceRemove(child_watch_tag_);
    child_watch_tag_ = 0;
  }
  // Disconnecting the management interface will terminate the openvpn
  // process. Ensure this is handled robustly by first removing the child watch
  // above and then terminating and reaping the process through ProcessKiller.
  management_server_->Stop();
  if (!tls_auth_file_.empty()) {
    file_util::Delete(tls_auth_file_, false);
    tls_auth_file_.clear();
  }
  if (!openvpn_config_file_.empty()) {
    file_util::Delete(openvpn_config_file_, false);
    openvpn_config_file_.clear();
  }
  if (default_service_callback_tag_) {
    manager()->DeregisterDefaultServiceCallback(default_service_callback_tag_);
    default_service_callback_tag_ = 0;
  }
  rpc_task_.reset();
  int interface_index = -1;
  if (device_) {
    interface_index = device_->interface_index();
    device_->DropConnection();
    device_->SetEnabled(false);
    device_ = NULL;
  }
  if (pid_) {
    Closure callback;
    if (interface_index >= 0) {
      callback =
          Bind(&DeleteInterface, device_info_->AsWeakPtr(), interface_index);
      interface_index = -1;
    }
    process_killer_->Kill(pid_, callback);
    pid_ = 0;
  }
  if (interface_index >= 0) {
    device_info_->DeleteInterface(interface_index);
  }
  tunnel_interface_.clear();
  if (service_) {
    if (state == Service::kStateFailure) {
      service_->SetErrorDetails(error_details);
      service_->SetFailure(failure);
    } else {
      service_->SetState(state);
    }
    service_ = NULL;
  }
  ip_properties_ = IPConfig::Properties();
}

// static
string OpenVPNDriver::JoinOptions(const vector<vector<string>> &options,
                                  char separator) {
  vector<string> option_strings;
  for (const auto &option : options) {
    vector<string> quoted_option;
    for (const auto &argument : option) {
      if (argument.find(' ') != string::npos ||
          argument.find('\t') != string::npos ||
          argument.find('"') != string::npos ||
          argument.find(separator) != string::npos) {
        string quoted_argument(argument);
        const char separator_chars[] = { separator, '\0' };
        ReplaceChars(argument, separator_chars, " ", &quoted_argument);
        ReplaceChars(quoted_argument, "\\", "\\\\", &quoted_argument);
        ReplaceChars(quoted_argument, "\"", "\\\"", &quoted_argument);
        quoted_option.push_back("\"" + quoted_argument + "\"");
      } else {
        quoted_option.push_back(argument);
      }
    }
    option_strings.push_back(JoinString(quoted_option, ' '));
  }
  return JoinString(option_strings, separator);
}

bool OpenVPNDriver::WriteConfigFile(
    const vector<vector<string>> &options,
    FilePath *config_file) {
  if (!file_util::DirectoryExists(openvpn_config_directory_)) {
    if (!file_util::CreateDirectory(openvpn_config_directory_)) {
      LOG(ERROR) << "Unable to create configuration directory  "
                 << openvpn_config_directory_.value();
      return false;
    }
    if (chmod(openvpn_config_directory_.value().c_str(), S_IRWXU)) {
      LOG(ERROR) << "Failed to set permissions on "
                 << openvpn_config_directory_.value();
      file_util::Delete(openvpn_config_directory_, true);
      return false;
    }
  }

  string contents = JoinOptions(options, '\n');
  contents.push_back('\n');
  if (!file_util::CreateTemporaryFileInDir(openvpn_config_directory_,
                                           config_file) ||
      file_util::WriteFile(*config_file, contents.data(), contents.size()) !=
      static_cast<int>(contents.size())) {
    LOG(ERROR) << "Unable to setup OpenVPN config file.";
    return false;
  }
  return true;
}

bool OpenVPNDriver::SpawnOpenVPN() {
  SLOG(VPN, 2) << __func__ << "(" << tunnel_interface_ << ")";

  vector<vector<string>> options;
  Error error;
  InitOptions(&options, &error);
  if (error.IsFailure()) {
    return false;
  }
  LOG(INFO) << "OpenVPN process options: " << JoinOptions(options, ',');
  if (!WriteConfigFile(options, &openvpn_config_file_)) {
    return false;
  }

  // TODO(quiche): This should be migrated to use ExternalTask.
  // (crbug.com/246263).
  vector<char *> process_args;
  process_args.push_back(const_cast<char *>(kOpenVPNPath));
  process_args.push_back(const_cast<char *>("--config"));
  process_args.push_back(const_cast<char *>(
      openvpn_config_file_.value().c_str()));
  process_args.push_back(NULL);

  vector<string> environment;
  InitEnvironment(&environment);

  vector<char *> process_env;
  for (const auto &environment_variable : environment) {
    process_env.push_back(const_cast<char *>(environment_variable.c_str()));
  }
  process_env.push_back(NULL);

  CHECK(!pid_);
  if (!glib_->SpawnAsync(NULL,
                         process_args.data(),
                         process_env.data(),
                         G_SPAWN_DO_NOT_REAP_CHILD,
                         NULL,
                         NULL,
                         &pid_,
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
  me->pid_ = 0;
  me->FailService(Service::kFailureInternal, Service::kErrorDetailsNone);
  // TODO(petkov): Figure if we need to restart the connection.
}

// static
void OpenVPNDriver::DeleteInterface(const WeakPtr<DeviceInfo> &device_info,
                                    int interface_index) {
  if (device_info) {
    LOG(INFO) << "Deleting interface " << interface_index;
    device_info->DeleteInterface(interface_index);
  }
}

bool OpenVPNDriver::ClaimInterface(const string &link_name,
                                   int interface_index) {
  if (link_name != tunnel_interface_) {
    return false;
  }

  SLOG(VPN, 2) << "Claiming " << link_name << " for OpenVPN tunnel";

  CHECK(!device_);
  device_ = new VirtualDevice(control_, dispatcher(), metrics_, manager(),
                              link_name, interface_index, Technology::kVPN);
  device_->SetEnabled(true);

  rpc_task_.reset(new RPCTask(control_, this));
  if (SpawnOpenVPN()) {
    default_service_callback_tag_ =
        manager()->RegisterDefaultServiceCallback(
            Bind(&OpenVPNDriver::OnDefaultServiceChanged, Unretained(this)));
  } else {
    FailService(Service::kFailureInternal, Service::kErrorDetailsNone);
  }
  return true;
}

void OpenVPNDriver::GetLogin(string */*user*/, string */*password*/) {
  NOTREACHED();
}

void OpenVPNDriver::Notify(const string &reason,
                           const map<string, string> &dict) {
  LOG(INFO) << "IP configuration received: " << reason;
  if (reason != "up") {
    device_->DropConnection();
    return;
  }
  // On restart/reconnect, update the existing IP configuration.
  ParseIPConfiguration(dict, &ip_properties_);
  device_->SelectService(service_);
  device_->UpdateIPConfig(ip_properties_);
  ReportConnectionMetrics();
  StopConnectTimeout();
}

// static
void OpenVPNDriver::ParseIPConfiguration(
    const map<string, string> &configuration,
    IPConfig::Properties *properties) {
  ForeignOptions foreign_options;
  RouteOptions routes;
  properties->address_family = IPAddress::kFamilyIPv4;
  if (!properties->subnet_prefix) {
    properties->subnet_prefix =
        IPAddress::GetMaxPrefixLength(properties->address_family);
  }
  for (const auto &configuration_map : configuration) {
    const string &key = configuration_map.first;
    const string &value = configuration_map.second;
    SLOG(VPN, 2) << "Processing: " << key << " -> " << value;
    if (LowerCaseEqualsASCII(key, kOpenVPNIfconfigLocal)) {
      properties->address = value;
    } else if (LowerCaseEqualsASCII(key, kOpenVPNIfconfigBroadcast)) {
      properties->broadcast_address = value;
    } else if (LowerCaseEqualsASCII(key, kOpenVPNIfconfigNetmask)) {
      properties->subnet_prefix =
          IPAddress::GetPrefixLengthFromMask(properties->address_family, value);
    } else if (LowerCaseEqualsASCII(key, kOpenVPNIfconfigRemote)) {
      if (StartsWithASCII(value, kSuspectedNetmaskPrefix, false)) {
        LOG(WARNING) << "Option " << key << " value " << value
                     << " looks more like a netmask than a peer address; "
                     << "assuming it is the former.";
        // In this situation, the "peer_address" value will be left
        // unset and Connection::UpdateFromIPConfig() will treat the
        // interface as if it were a broadcast-style network.  The
        // kernel will, automatically set the peer address equal to
        // the local address.
        properties->subnet_prefix =
            IPAddress::GetPrefixLengthFromMask(properties->address_family,
                                               value);
      } else {
        properties->peer_address = value;
      }
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
  vector<string> domain_search;
  vector<string> dns_servers;
  for (const auto &option_map : options) {
    ParseForeignOption(option_map.second, &domain_search, &dns_servers);
  }
  if (!domain_search.empty()) {
    properties->domain_search.swap(domain_search);
  }
  LOG_IF(WARNING, properties->domain_search.empty())
      << "No search domains provided.";
  if (!dns_servers.empty()) {
    properties->dns_servers.swap(dns_servers);
  }
  LOG_IF(WARNING, properties->dns_servers.empty())
      << "No DNS servers provided.";
}

// static
void OpenVPNDriver::ParseForeignOption(const string &option,
                                       vector<string> *domain_search,
                                       vector<string> *dns_servers) {
  SLOG(VPN, 2) << __func__ << "(" << option << ")";
  vector<string> tokens;
  SplitString(option, ' ', &tokens);
  if (tokens.size() != 3 || !LowerCaseEqualsASCII(tokens[0], "dhcp-option")) {
    return;
  }
  if (LowerCaseEqualsASCII(tokens[1], "domain")) {
    domain_search->push_back(tokens[2]);
  } else if (LowerCaseEqualsASCII(tokens[1], "dns")) {
    dns_servers->push_back(tokens[2]);
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
  vector<IPConfig::Route> new_routes;
  for (const auto &route_map : routes) {
    const IPConfig::Route &route = route_map.second;
    if (route.host.empty() || route.netmask.empty() || route.gateway.empty()) {
      LOG(WARNING) << "Ignoring incomplete route: " << route_map.first;
      continue;
    }
    new_routes.push_back(route);
  }
  if (!new_routes.empty()) {
    properties->routes.swap(new_routes);
  }
  LOG_IF(WARNING, properties->routes.empty()) << "No routes provided.";
}

// static
bool OpenVPNDriver::SplitPortFromHost(
    const string &host, string *name, string *port) {
  vector<string> tokens;
  SplitString(host, ':', &tokens);
  int port_number = 0;
  if (tokens.size() != 2 || tokens[0].empty() || tokens[1].empty() ||
      !IsAsciiDigit(tokens[1][0]) ||
      !base::StringToInt(tokens[1], &port_number) || port_number > kuint16max) {
    return false;
  }
  *name = tokens[0];
  *port = tokens[1];
  return true;
}

void OpenVPNDriver::Connect(const VPNServiceRefPtr &service, Error *error) {
  StartConnectTimeout(kDefaultConnectTimeoutSeconds);
  service_ = service;
  service_->SetState(Service::kStateConfiguring);
  if (!device_info_->CreateTunnelInterface(&tunnel_interface_)) {
    Error::PopulateAndLog(
        error, Error::kInternalError, "Could not create tunnel interface.");
    FailService(Service::kFailureInternal, Service::kErrorDetailsNone);
  }
  // Wait for the ClaimInterface callback to continue the connection process.
}

void OpenVPNDriver::InitOptions(vector<vector<string>> *options, Error *error) {
  string vpnhost = args()->LookupString(flimflam::kProviderHostProperty, "");
  if (vpnhost.empty()) {
    Error::PopulateAndLog(
        error, Error::kInvalidArguments, "VPN host not specified.");
    return;
  }
  AppendOption("client", options);
  AppendOption("tls-client", options);

  string host_name, host_port;
  if (SplitPortFromHost(vpnhost, &host_name, &host_port)) {
    DCHECK(!host_name.empty());
    DCHECK(!host_port.empty());
    AppendOption("remote", host_name, host_port, options);
  } else {
    AppendOption("remote", vpnhost, options);
  }

  AppendOption("nobind", options);
  AppendOption("persist-key", options);
  AppendOption("persist-tun", options);

  CHECK(!tunnel_interface_.empty());
  AppendOption("dev", tunnel_interface_, options);
  AppendOption("dev-type", "tun", options);

  InitLoggingOptions(options);

  AppendValueOption(kVPNMTUProperty, "mtu", options);
  AppendValueOption(flimflam::kOpenVPNProtoProperty, "proto", options);
  AppendValueOption(flimflam::kOpenVPNPortProperty, "port", options);
  AppendValueOption(kOpenVPNTLSAuthProperty, "tls-auth", options);
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
      AppendOption("tls-auth", tls_auth_file_.value(), options);
    }
  }
  AppendValueOption(
      flimflam::kOpenVPNTLSRemoteProperty, "tls-remote", options);
  AppendValueOption(flimflam::kOpenVPNCipherProperty, "cipher", options);
  AppendValueOption(flimflam::kOpenVPNAuthProperty, "auth", options);
  AppendFlag(flimflam::kOpenVPNAuthNoCacheProperty, "auth-nocache", options);
  AppendValueOption(
      flimflam::kOpenVPNAuthRetryProperty, "auth-retry", options);
  AppendFlag(flimflam::kOpenVPNCompLZOProperty, "comp-lzo", options);
  AppendFlag(flimflam::kOpenVPNCompNoAdaptProperty, "comp-noadapt", options);
  AppendFlag(
      flimflam::kOpenVPNPushPeerInfoProperty, "push-peer-info", options);
  AppendValueOption(flimflam::kOpenVPNRenegSecProperty, "reneg-sec", options);
  AppendValueOption(flimflam::kOpenVPNShaperProperty, "shaper", options);
  AppendValueOption(flimflam::kOpenVPNServerPollTimeoutProperty,
                    "server-poll-timeout", options);

  if (!InitCAOptions(options, error)) {
    return;
  }

  // Client-side ping support.
  AppendValueOption(kOpenVPNPingProperty, "ping", options);
  AppendValueOption(kOpenVPNPingExitProperty, "ping-exit", options);
  AppendValueOption(kOpenVPNPingRestartProperty, "ping-restart", options);

  AppendValueOption(
      flimflam::kOpenVPNNsCertTypeProperty, "ns-cert-type", options);

  InitClientAuthOptions(options);
  InitPKCS11Options(options);

  // TLS suport.
  string remote_cert_tls =
      args()->LookupString(flimflam::kOpenVPNRemoteCertTLSProperty, "");
  if (remote_cert_tls.empty()) {
    remote_cert_tls = "server";
  }
  if (remote_cert_tls != "none") {
    AppendOption("remote-cert-tls", remote_cert_tls, options);
  }

  // This is an undocumented command line argument that works like a .cfg file
  // entry. TODO(sleffler): Maybe roll this into the "tls-auth" option?
  AppendValueOption(
      flimflam::kOpenVPNKeyDirectionProperty, "key-direction", options);
  // TODO(sleffler): Support more than one eku parameter.
  AppendValueOption(
      flimflam::kOpenVPNRemoteCertEKUProperty, "remote-cert-eku", options);
  AppendValueOption(
      flimflam::kOpenVPNRemoteCertKUProperty, "remote-cert-ku", options);

  if (!InitManagementChannelOptions(options, error)) {
    return;
  }

  // Setup openvpn-script options and RPC information required to send back
  // Layer 3 configuration.
  AppendOption("setenv", kRPCTaskServiceVariable,
               rpc_task_->GetRpcConnectionIdentifier(), options);
  AppendOption("setenv", kRPCTaskServiceVariable,
               rpc_task_->GetRpcConnectionIdentifier(), options);
  AppendOption("setenv", kRPCTaskPathVariable, rpc_task_->GetRpcIdentifier(),
               options);
  AppendOption("script-security", "2", options);
  AppendOption("up", kOpenVPNScript, options);
  AppendOption("up-restart", options);

  // Disable openvpn handling since we do route+ifconfig work.
  AppendOption("route-noexec", options);
  AppendOption("ifconfig-noexec", options);

  // Drop root privileges on connection and enable callback scripts to send
  // notify messages.
  AppendOption("user", "openvpn", options);
  AppendOption("group", "openvpn", options);
}

bool OpenVPNDriver::InitCAOptions(
    vector<vector<string>> *options, Error *error) {
  string ca_cert =
      args()->LookupString(flimflam::kOpenVPNCaCertProperty, "");
  string ca_cert_nss =
      args()->LookupString(flimflam::kOpenVPNCaCertNSSProperty, "");
  vector<string> ca_cert_pem;
  if (args()->ContainsStrings(kOpenVPNCaCertPemProperty)) {
    ca_cert_pem = args()->GetStrings(kOpenVPNCaCertPemProperty);
  }

  int num_ca_cert_types = 0;
  if (!ca_cert.empty())
      num_ca_cert_types++;
  if (!ca_cert_nss.empty())
      num_ca_cert_types++;
  if (!ca_cert_pem.empty())
      num_ca_cert_types++;
  if (num_ca_cert_types == 0) {
    // Use default CAs if no CA certificate is provided.
    AppendOption("ca", kDefaultCACertificates, options);
    return true;
  } else if (num_ca_cert_types > 1) {
    Error::PopulateAndLog(
        error, Error::kInvalidArguments,
        "Can't specify more than one of CACert, CACertNSS and CACertPEM.");
    return false;
  }
  string cert_file;
  if (!ca_cert_nss.empty()) {
    DCHECK(ca_cert.empty() && ca_cert_pem.empty());
    const string &vpnhost = args()->GetString(flimflam::kProviderHostProperty);
    vector<char> id(vpnhost.begin(), vpnhost.end());
    FilePath certfile = nss_->GetPEMCertfile(ca_cert_nss, id);
    if (certfile.empty()) {
      Error::PopulateAndLog(
          error,
          Error::kInvalidArguments,
          "Unable to extract NSS CA certificate: " + ca_cert_nss);
      return false;
    }
    AppendOption("ca", certfile.value(), options);
    return true;
  } else if (!ca_cert_pem.empty()) {
    DCHECK(ca_cert.empty() && ca_cert_nss.empty());
    FilePath certfile = certificate_file_->CreatePEMFromStrings(ca_cert_pem);
    if (certfile.empty()) {
      Error::PopulateAndLog(
          error,
          Error::kInvalidArguments,
          "Unable to extract PEM CA certificates.");
      return false;
    }
    AppendOption("ca", certfile.value(), options);
    return true;
  }
  DCHECK(!ca_cert.empty() && ca_cert_nss.empty() && ca_cert_pem.empty());
  AppendOption("ca", ca_cert, options);
  return true;
}

void OpenVPNDriver::InitPKCS11Options(vector<vector<string>> *options) {
  string id = args()->LookupString(flimflam::kOpenVPNClientCertIdProperty, "");
  if (!id.empty()) {
    string provider =
        args()->LookupString(flimflam::kOpenVPNProviderProperty, "");
    if (provider.empty()) {
      provider = kDefaultPKCS11Provider;
    }
    AppendOption("pkcs11-providers", provider, options);
    AppendOption("pkcs11-id", id, options);
  }
}

void OpenVPNDriver::InitClientAuthOptions(vector<vector<string>> *options) {
  bool has_cert = AppendValueOption(kOpenVPNCertProperty, "cert", options) ||
      !args()->LookupString(flimflam::kOpenVPNClientCertIdProperty, "").empty();
  bool has_key = AppendValueOption(kOpenVPNKeyProperty, "key", options);
  // If the AuthUserPass property is set, or the User property is non-empty, or
  // there's neither a key, nor a cert available, specify user-password client
  // authentication.
  if (args()->ContainsString(flimflam::kOpenVPNAuthUserPassProperty) ||
      !args()->LookupString(flimflam::kOpenVPNUserProperty, "").empty() ||
      (!has_cert && !has_key)) {
    AppendOption("auth-user-pass", options);
  }
}

bool OpenVPNDriver::InitManagementChannelOptions(
    vector<vector<string>> *options, Error *error) {
  if (!management_server_->Start(dispatcher(), &sockets_, options)) {
    Error::PopulateAndLog(
        error, Error::kInternalError, "Unable to setup management channel.");
    return false;
  }
  // If there's a connected default service already, allow the openvpn client to
  // establish connection as soon as it's started. Otherwise, hold the client
  // until an underlying service connects and OnDefaultServiceChanged is
  // invoked.
  if (manager()->IsOnline()) {
    management_server_->ReleaseHold();
  }
  return true;
}

void OpenVPNDriver::InitLoggingOptions(vector<vector<string>> *options) {
  AppendOption("syslog", options);

  string verb = args()->LookupString(kOpenVPNVerbProperty, "");
  if (verb.empty() && SLOG_IS_ON(VPN, 0)) {
    verb = "3";
  }
  if (!verb.empty()) {
    AppendOption("verb", verb, options);
  }
}

void OpenVPNDriver::AppendOption(
    const string &option, vector<vector<string>> *options) {
  options->push_back(vector<string>{ option });
}

void OpenVPNDriver::AppendOption(
    const string &option,
    const string &value,
    vector<vector<string>> *options) {
  options->push_back(vector<string>{ option, value });
}

void OpenVPNDriver::AppendOption(
    const string &option,
    const string &value0,
    const string &value1,
    vector<vector<string>> *options) {
  options->push_back(vector<string>{ option, value0, value1 });
}

bool OpenVPNDriver::AppendValueOption(
    const string &property,
    const string &option,
    vector<vector<string>> *options) {
  string value = args()->LookupString(property, "");
  if (!value.empty()) {
    AppendOption(option, value, options);
    return true;
  }
  return false;
}

bool OpenVPNDriver::AppendFlag(
    const string &property,
    const string &option,
    vector<vector<string>> *options) {
  if (args()->ContainsString(property)) {
    AppendOption(option, options);
    return true;
  }
  return false;
}

void OpenVPNDriver::Disconnect() {
  SLOG(VPN, 2) << __func__;
  IdleService();
}

void OpenVPNDriver::OnConnectionDisconnected() {
  LOG(INFO) << "Underlying connection disconnected.";
  // Restart the OpenVPN client forcing a reconnect attempt.
  management_server_->Restart();
  // Indicate reconnect state right away to drop the VPN connection and start
  // the connect timeout. This ensures that any miscommunication between shill
  // and openvpn will not lead to a permanently stale connectivity state. Note
  // that a subsequent invocation of OnReconnecting due to a RECONNECTING
  // message will essentially be a no-op.
  OnReconnecting(kReconnectReasonOffline);
}

void OpenVPNDriver::OnConnectTimeout() {
  VPNDriver::OnConnectTimeout();
  Service::ConnectFailure failure =
      management_server_->state() == OpenVPNManagementServer::kStateResolve ?
      Service::kFailureDNSLookup : Service::kFailureConnect;
  FailService(failure, Service::kErrorDetailsNone);
}

void OpenVPNDriver::OnReconnecting(ReconnectReason reason) {
  LOG(INFO) << __func__ << "(" << reason << ")";
  int timeout_seconds = GetReconnectTimeoutSeconds(reason);
  if (reason == kReconnectReasonTLSError &&
      timeout_seconds < connect_timeout_seconds()) {
    // Reconnect due to TLS error happens during connect so we need to cancel
    // the original connect timeout first and then reduce the time limit.
    StopConnectTimeout();
  }
  StartConnectTimeout(timeout_seconds);
  // On restart/reconnect, drop the VPN connection, if any. The openvpn client
  // might be in hold state if the VPN connection was previously established
  // successfully. The hold will be released by OnDefaultServiceChanged when a
  // new default service connects. This ensures that the client will use a fully
  // functional underlying connection to reconnect.
  if (device_) {
    device_->DropConnection();
  }
  if (service_) {
    service_->SetState(Service::kStateAssociating);
  }
}

// static
int OpenVPNDriver::GetReconnectTimeoutSeconds(ReconnectReason reason) {
  switch (reason) {
    case kReconnectReasonOffline:
      return kReconnectOfflineTimeoutSeconds;
    case kReconnectReasonTLSError:
      return kReconnectTLSErrorTimeoutSeconds;
    default:
      break;
  }
  return kDefaultConnectTimeoutSeconds;
}

string OpenVPNDriver::GetProviderType() const {
  return flimflam::kProviderOpenVpn;
}

KeyValueStore OpenVPNDriver::GetProvider(Error *error) {
  SLOG(VPN, 2) << __func__;
  KeyValueStore props = VPNDriver::GetProvider(error);
  props.SetBool(flimflam::kPassphraseRequiredProperty,
                args()->LookupString(
                    flimflam::kOpenVPNPasswordProperty, "").empty());
  return props;
}

// TODO(petkov): Consider refactoring lsb-release parsing out into a shared
// singleton if it's used outside OpenVPN.
bool OpenVPNDriver::ParseLSBRelease(map<string, string> *lsb_release) {
  SLOG(VPN, 2) << __func__ << "(" << lsb_release_file_.value() << ")";
  string contents;
  if (!file_util::ReadFileToString(lsb_release_file_, &contents)) {
    LOG(ERROR) << "Unable to read the lsb-release file: "
               << lsb_release_file_.value();
    return false;
  }
  vector<string> lines;
  SplitString(contents, '\n', &lines);
  for (const auto &line : lines) {
    size_t assign = line.find('=');
    if (assign == string::npos) {
      continue;
    }
    (*lsb_release)[line.substr(0, assign)] = line.substr(assign + 1);
  }
  return true;
}

void OpenVPNDriver::InitEnvironment(vector<string> *environment) {
  // Adds the platform name and version to the environment so that openvpn can
  // send them to the server when OpenVPN.PushPeerInfo is set.
  map<string, string> lsb_release;
  ParseLSBRelease(&lsb_release);
  string platform_name = lsb_release[kChromeOSReleaseName];
  if (!platform_name.empty()) {
    environment->push_back("IV_PLAT=" + platform_name);
  }
  string platform_version = lsb_release[kChromeOSReleaseVersion];
  if (!platform_version.empty()) {
    environment->push_back("IV_PLAT_REL=" + platform_version);
  }
}

void OpenVPNDriver::OnDefaultServiceChanged(const ServiceRefPtr &service) {
  SLOG(VPN, 2) << __func__
               << "(" << (service ? service->unique_name() : "-") << ")";
  // Allow the openvpn client to connect/reconnect only over a connected
  // underlying default service. If there's no default connected service, hold
  // the openvpn client until an underlying connection is established. If the
  // default service is our VPN service, hold the openvpn client on reconnect so
  // that the VPN connection can be torn down fully before a new connection
  // attempt is made over the underlying service.
  if (service && service != service_ && service->IsConnected()) {
    management_server_->ReleaseHold();
  } else {
    management_server_->Hold();
  }
}

void OpenVPNDriver::ReportConnectionMetrics() {
  metrics_->SendEnumToUMA(
      Metrics::kMetricVpnDriver,
      Metrics::kVpnDriverOpenVpn,
      Metrics::kMetricVpnDriverMax);

  if (args()->LookupString(flimflam::kOpenVPNCaCertNSSProperty, "") != "" ||
      args()->LookupString(flimflam::kOpenVPNCaCertProperty, "") != "") {
    metrics_->SendEnumToUMA(
        Metrics::kMetricVpnRemoteAuthenticationType,
        Metrics::kVpnRemoteAuthenticationTypeOpenVpnCertificate,
        Metrics::kMetricVpnRemoteAuthenticationTypeMax);
  } else {
    metrics_->SendEnumToUMA(
        Metrics::kMetricVpnRemoteAuthenticationType,
        Metrics::kVpnRemoteAuthenticationTypeOpenVpnDefault,
        Metrics::kMetricVpnRemoteAuthenticationTypeMax);
  }

  bool has_user_authentication = false;
  if (args()->LookupString(flimflam::kOpenVPNOTPProperty, "") != "") {
    metrics_->SendEnumToUMA(
        Metrics::kMetricVpnUserAuthenticationType,
        Metrics::kVpnUserAuthenticationTypeOpenVpnUsernamePasswordOtp,
        Metrics::kMetricVpnUserAuthenticationTypeMax);
    has_user_authentication = true;
  }
  if (args()->LookupString(flimflam::kOpenVPNAuthUserPassProperty, "") != ""||
      args()->LookupString(flimflam::kOpenVPNUserProperty, "") != "")  {
    metrics_->SendEnumToUMA(
        Metrics::kMetricVpnUserAuthenticationType,
        Metrics::kVpnUserAuthenticationTypeOpenVpnUsernamePassword,
        Metrics::kMetricVpnUserAuthenticationTypeMax);
    has_user_authentication = true;
  }
  if (args()->LookupString(flimflam::kOpenVPNClientCertIdProperty, "") != "" ||
      args()->LookupString(kOpenVPNCertProperty, "") != "") {
    metrics_->SendEnumToUMA(
        Metrics::kMetricVpnUserAuthenticationType,
        Metrics::kVpnUserAuthenticationTypeOpenVpnCertificate,
        Metrics::kMetricVpnUserAuthenticationTypeMax);
    has_user_authentication = true;
  }
  if (!has_user_authentication) {
    metrics_->SendEnumToUMA(
        Metrics::kMetricVpnUserAuthenticationType,
        Metrics::kVpnUserAuthenticationTypeOpenVpnNone,
        Metrics::kMetricVpnUserAuthenticationTypeMax);
  }
}

}  // namespace shill
