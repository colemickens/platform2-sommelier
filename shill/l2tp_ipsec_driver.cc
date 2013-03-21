// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/l2tp_ipsec_driver.h"

#include <sys/wait.h>

#include <base/bind.h>
#include <base/file_util.h>
#include <base/string_util.h>
#include <chromeos/dbus/service_constants.h>
#include <chromeos/vpn-manager/service_error.h>

#include "shill/certificate_file.h"
#include "shill/device_info.h"
#include "shill/error.h"
#include "shill/logging.h"
#include "shill/manager.h"
#include "shill/nss.h"
#include "shill/process_killer.h"
#include "shill/vpn.h"
#include "shill/vpn_service.h"

using base::Bind;
using base::Closure;
using base::FilePath;
using std::map;
using std::string;
using std::vector;

namespace shill {

namespace {
const char kL2TPIPSecIPSecTimeoutProperty[] = "L2TPIPsec.IPsecTimeout";
const char kL2TPIPSecLeftProtoPortProperty[] = "L2TPIPsec.LeftProtoPort";
const char kL2TPIPSecLengthBitProperty[] = "L2TPIPsec.LengthBit";
const char kL2TPIPSecPFSProperty[] = "L2TPIPsec.PFS";
const char kL2TPIPSecRefusePapProperty[] = "L2TPIPsec.RefusePap";
const char kL2TPIPSecRekeyProperty[] = "L2TPIPsec.Rekey";
const char kL2TPIPSecRequireAuthProperty[] = "L2TPIPsec.RequireAuth";
const char kL2TPIPSecRequireChapProperty[] = "L2TPIPsec.RequireChap";
const char kL2TPIPSecRightProtoPortProperty[] = "L2TPIPsec.RightProtoPort";
}  // namespace

// static
const char L2TPIPSecDriver::kPPPDPlugin[] = SHIMDIR "/shill-pppd-plugin.so";
// static
const char L2TPIPSecDriver::kL2TPIPSecVPNPath[] = "/usr/sbin/l2tpipsec_vpn";
// static
const VPNDriver::Property L2TPIPSecDriver::kProperties[] = {
  { flimflam::kL2tpIpsecAuthenticationType, 0 },
  { flimflam::kL2tpIpsecCaCertNssProperty, 0 },
  { flimflam::kL2tpIpsecClientCertIdProperty, 0 },
  { flimflam::kL2tpIpsecClientCertSlotProperty, 0 },
  { flimflam::kL2tpIpsecIkeVersion, 0 },
  { flimflam::kL2tpIpsecPasswordProperty,
    Property::kCredential | Property::kWriteOnly },
  { flimflam::kL2tpIpsecPinProperty, Property::kCredential },
  { flimflam::kL2tpIpsecPskProperty, Property::kCredential },
  { flimflam::kL2tpIpsecUserProperty, 0 },
  { flimflam::kProviderHostProperty, 0 },
  { flimflam::kProviderNameProperty, 0 },
  { flimflam::kProviderTypeProperty, 0 },
  { kL2tpIpsecCaCertPemProperty, 0 },
  { kL2TPIPSecIPSecTimeoutProperty, 0 },
  { kL2TPIPSecLeftProtoPortProperty, 0 },
  { kL2TPIPSecLengthBitProperty, 0 },
  { kL2TPIPSecPFSProperty, 0 },
  { kL2TPIPSecRefusePapProperty, 0 },
  { kL2TPIPSecRekeyProperty, 0 },
  { kL2TPIPSecRequireAuthProperty, 0 },
  { kL2TPIPSecRequireChapProperty, 0 },
  { kL2TPIPSecRightProtoPortProperty, 0 },
};

L2TPIPSecDriver::L2TPIPSecDriver(ControlInterface *control,
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
      nss_(NSS::GetInstance()),
      process_killer_(ProcessKiller::GetInstance()),
      certificate_file_(new CertificateFile(glib)),
      pid_(0),
      child_watch_tag_(0) {}

L2TPIPSecDriver::~L2TPIPSecDriver() {
  IdleService();
}

bool L2TPIPSecDriver::ClaimInterface(const string &link_name,
                                     int interface_index) {
  // TODO(petkov): crosbug.com/29970.
  NOTIMPLEMENTED();
  return false;
}

void L2TPIPSecDriver::Connect(const VPNServiceRefPtr &service, Error *error) {
  StartConnectTimeout(kDefaultConnectTimeoutSeconds);
  service_ = service;
  service_->SetState(Service::kStateConfiguring);
  rpc_task_.reset(new RPCTask(control_, this));
  if (!SpawnL2TPIPSecVPN(error)) {
    FailService(Service::kFailureInternal);
  }
}

void L2TPIPSecDriver::Disconnect() {
  SLOG(VPN, 2) << __func__;
  IdleService();
}

void L2TPIPSecDriver::OnConnectionDisconnected() {
  LOG(INFO) << "Underlying connection disconnected.";
  IdleService();
}

void L2TPIPSecDriver::OnConnectTimeout() {
  VPNDriver::OnConnectTimeout();
  FailService(Service::kFailureConnect);
}

string L2TPIPSecDriver::GetProviderType() const {
  return flimflam::kProviderL2tpIpsec;
}

void L2TPIPSecDriver::IdleService() {
  Cleanup(Service::kStateIdle, Service::kFailureUnknown);
}

void L2TPIPSecDriver::FailService(Service::ConnectFailure failure) {
  Cleanup(Service::kStateFailure, failure);
}

void L2TPIPSecDriver::Cleanup(Service::ConnectState state,
                              Service::ConnectFailure failure) {
  SLOG(VPN, 2) << __func__ << "("
               << Service::ConnectStateToString(state) << ", "
               << Service::ConnectFailureToString(failure) << ")";
  StopConnectTimeout();
  DeletePSKFile();
  if (child_watch_tag_) {
    glib_->SourceRemove(child_watch_tag_);
    child_watch_tag_ = 0;
  }
  if (pid_) {
    process_killer_->Kill(pid_, Closure());
    pid_ = 0;
  }
  if (device_) {
    device_->OnDisconnected();
    device_->SetEnabled(false);
    device_ = NULL;
  }
  rpc_task_.reset();
  if (service_) {
    if (state == Service::kStateFailure) {
      service_->SetFailure(failure);
    } else {
      service_->SetState(state);
    }
    service_ = NULL;
  }
}

void L2TPIPSecDriver::DeletePSKFile() {
  if (!psk_file_.empty()) {
    file_util::Delete(psk_file_, false);
    psk_file_.clear();
  }
}

bool L2TPIPSecDriver::SpawnL2TPIPSecVPN(Error *error) {
  SLOG(VPN, 2) << __func__;

  vector<string> options;
  if (!InitOptions(&options, error)) {
    return false;
  }
  LOG(INFO) << "L2TP/IPSec VPN process options: " << JoinString(options, ' ');

  // TODO(petkov): This code needs to be abstracted away in a separate external
  // process module (crosbug.com/27131).
  vector<char *> process_args;
  process_args.push_back(const_cast<char *>(kL2TPIPSecVPNPath));
  for (vector<string>::const_iterator it = options.begin();
       it != options.end(); ++it) {
    process_args.push_back(const_cast<char *>(it->c_str()));
  }
  process_args.push_back(NULL);

  vector<string> environment;
  InitEnvironment(&environment);

  vector<char *> process_env;
  for (vector<string>::const_iterator it = environment.begin();
       it != environment.end(); ++it) {
    process_env.push_back(const_cast<char *>(it->c_str()));
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
    Error::PopulateAndLog(error, Error::kInternalError,
                          string("Unable to spawn: ") + process_args[0]);
    return false;
  }
  CHECK(!child_watch_tag_);
  child_watch_tag_ = glib_->ChildWatchAdd(pid_, OnL2TPIPSecVPNDied, this);
  return true;
}

void L2TPIPSecDriver::InitEnvironment(vector<string> *environment) {
  environment->push_back(string(kRPCTaskServiceVariable) + "=" +
                         rpc_task_->GetRpcConnectionIdentifier());
  environment->push_back(string(kRPCTaskPathVariable) + "=" +
                         rpc_task_->GetRpcIdentifier());
}

bool L2TPIPSecDriver::InitOptions(vector<string> *options, Error *error) {
  string vpnhost = args()->LookupString(flimflam::kProviderHostProperty, "");
  if (vpnhost.empty()) {
    Error::PopulateAndLog(
        error, Error::kInvalidArguments, "VPN host not specified.");
    return false;
  }

  if (!InitPSKOptions(options, error)) {
    return false;
  }

  options->push_back("--remote_host");
  options->push_back(vpnhost);
  options->push_back("--pppd_plugin");
  options->push_back(kPPPDPlugin);
  // Disable pppd from configuring IP addresses, routes, DNS.
  options->push_back("--nosystemconfig");

  // Accept a PEM CA certificate or an NSS certificate, but not both.
  // Prefer PEM to NSS.
  if (!InitPEMOptions(options)) {
    InitNSSOptions(options);
  }

  AppendValueOption(flimflam::kL2tpIpsecClientCertIdProperty,
                    "--client_cert_id", options);
  AppendValueOption(flimflam::kL2tpIpsecClientCertSlotProperty,
                    "--client_cert_slot", options);
  AppendValueOption(flimflam::kL2tpIpsecPinProperty, "--user_pin", options);
  AppendValueOption(flimflam::kL2tpIpsecUserProperty, "--user", options);
  AppendValueOption(kL2TPIPSecIPSecTimeoutProperty, "--ipsec_timeout", options);
  AppendValueOption(kL2TPIPSecLeftProtoPortProperty,
                    "--leftprotoport", options);
  AppendFlag(kL2TPIPSecPFSProperty, "--pfs", "--nopfs", options);
  AppendFlag(kL2TPIPSecRekeyProperty, "--rekey", "--norekey", options);
  AppendValueOption(kL2TPIPSecRightProtoPortProperty,
                    "--rightprotoport", options);
  AppendFlag(kL2TPIPSecRequireChapProperty,
             "--require_chap", "--norequire_chap", options);
  AppendFlag(kL2TPIPSecRefusePapProperty,
             "--refuse_pap", "--norefuse_pap", options);
  AppendFlag(kL2TPIPSecRequireAuthProperty,
             "--require_authentication", "--norequire_authentication", options);
  AppendFlag(kL2TPIPSecLengthBitProperty,
             "--length_bit", "--nolength_bit", options);
  if (SLOG_IS_ON(VPN, 0)) {
    options->push_back("--debug");
  }
  return true;
}

bool L2TPIPSecDriver::InitPSKOptions(vector<string> *options, Error *error) {
  string psk = args()->LookupString(flimflam::kL2tpIpsecPskProperty, "");
  if (!psk.empty()) {
    if (!file_util::CreateTemporaryFileInDir(
            manager()->run_path(), &psk_file_) ||
        chmod(psk_file_.value().c_str(), S_IRUSR | S_IWUSR) ||
        file_util::WriteFile(psk_file_, psk.data(), psk.size()) !=
        static_cast<int>(psk.size())) {
      Error::PopulateAndLog(
          error, Error::kInternalError, "Unable to setup psk file.");
      return false;
    }
    options->push_back("--psk_file");
    options->push_back(psk_file_.value());
  }
  return true;
}

void L2TPIPSecDriver::InitNSSOptions(vector<string> *options) {
  string ca_cert =
      args()->LookupString(flimflam::kL2tpIpsecCaCertNssProperty, "");
  if (!ca_cert.empty()) {
    const string &vpnhost = args()->GetString(flimflam::kProviderHostProperty);
    vector<char> id(vpnhost.begin(), vpnhost.end());
    FilePath certfile = nss_->GetDERCertfile(ca_cert, id);
    if (certfile.empty()) {
      LOG(ERROR) << "Unable to extract certificate: " << ca_cert;
    } else {
      options->push_back("--server_ca_file");
      options->push_back(certfile.value());
    }
  }
}

bool L2TPIPSecDriver::InitPEMOptions(vector<string> *options) {
  string ca_cert = args()->LookupString(kL2tpIpsecCaCertPemProperty, "");
  if (ca_cert.empty()) {
    return false;
  }
  FilePath certfile = certificate_file_->CreateDERFromString(ca_cert);
  if (certfile.empty()) {
    LOG(ERROR) << "Unable to extract certificate from PEM string.";
    return false;
  }
  options->push_back("--server_ca_file");
  options->push_back(certfile.value());
  return true;
}

bool L2TPIPSecDriver::AppendValueOption(
    const string &property, const string &option, vector<string> *options) {
  string value = args()->LookupString(property, "");
  if (!value.empty()) {
    options->push_back(option);
    options->push_back(value);
    return true;
  }
  return false;
}

bool L2TPIPSecDriver::AppendFlag(const string &property,
                                 const string &true_option,
                                 const string &false_option,
                                 vector<string> *options) {
  string value = args()->LookupString(property, "");
  if (!value.empty()) {
    options->push_back(value == "true" ? true_option : false_option);
    return true;
  }
  return false;
}

// static
void L2TPIPSecDriver::OnL2TPIPSecVPNDied(GPid pid, gint status, gpointer data) {
  LOG(INFO) << __func__ << "(" << pid << ", "  << status << ")";
  L2TPIPSecDriver *me = reinterpret_cast<L2TPIPSecDriver *>(data);
  me->child_watch_tag_ = 0;
  CHECK_EQ(pid, me->pid_);
  me->pid_ = 0;
  me->FailService(TranslateExitStatusToFailure(status));
  // TODO(petkov): Figure if we need to restart the connection.
}

// static
Service::ConnectFailure L2TPIPSecDriver::TranslateExitStatusToFailure(
    int status) {
  if (!WIFEXITED(status)) {
    return Service::kFailureInternal;
  }
  switch (WEXITSTATUS(status)) {
    case vpn_manager::kServiceErrorResolveHostnameFailed:
      return Service::kFailureDNSLookup;
    case vpn_manager::kServiceErrorIpsecConnectionFailed:
    case vpn_manager::kServiceErrorL2tpConnectionFailed:
    case vpn_manager::kServiceErrorPppConnectionFailed:
      return Service::kFailureConnect;
    case vpn_manager::kServiceErrorIpsecPresharedKeyAuthenticationFailed:
      return Service::kFailureIPSecPSKAuth;
    case vpn_manager::kServiceErrorIpsecCertificateAuthenticationFailed:
      return Service::kFailureIPSecCertAuth;
    case vpn_manager::kServiceErrorPppAuthenticationFailed:
      return Service::kFailurePPPAuth;
    default:
      break;
  }
  return Service::kFailureUnknown;
}

void L2TPIPSecDriver::GetLogin(string *user, string *password) {
  LOG(INFO) << "Login requested.";
  string user_property =
      args()->LookupString(flimflam::kL2tpIpsecUserProperty, "");
  if (user_property.empty()) {
    LOG(ERROR) << "User not set.";
    return;
  }
  string password_property =
      args()->LookupString(flimflam::kL2tpIpsecPasswordProperty, "");
  if (password_property.empty()) {
    LOG(ERROR) << "Password not set.";
    return;
  }
  *user = user_property;
  *password = password_property;
}

void L2TPIPSecDriver::ParseIPConfiguration(
    const map<string, string> &configuration,
    IPConfig::Properties *properties,
    string *interface_name) {
  properties->address_family = IPAddress::kFamilyIPv4;
  properties->subnet_prefix = IPAddress::GetMaxPrefixLength(
      properties->address_family);
  for (map<string, string>::const_iterator it = configuration.begin();
       it != configuration.end(); ++it) {
    const string &key = it->first;
    const string &value = it->second;
    SLOG(VPN, 2) << "Processing: " << key << " -> " << value;
    if (key == kL2TPIPSecInternalIP4Address) {
      properties->address = value;
    } else if (key == kL2TPIPSecExternalIP4Address) {
      properties->peer_address = value;
    } else if (key == kL2TPIPSecGatewayAddress) {
      properties->gateway = value;
    } else if (key == kL2TPIPSecDNS1) {
      properties->dns_servers.insert(properties->dns_servers.begin(), value);
    } else if (key == kL2TPIPSecDNS2) {
      properties->dns_servers.push_back(value);
    } else if (key == kL2TPIPSecInterfaceName) {
      *interface_name = value;
    } else if (key == kL2TPIPSecLNSAddress) {
      properties->trusted_ip = value;
    } else {
      SLOG(VPN, 2) << "Key ignored.";
    }
  }

  // There is no IPv6 support for L2TP/IPsec VPN at this moment, so create a
  // blackhole route for IPv6 traffic after establishing a IPv4 VPN.
  // TODO(benchan): Generalize this when IPv6 support is added.
  properties->blackhole_ipv6 = true;
}

void L2TPIPSecDriver::Notify(
    const string &reason, const map<string, string> &dict) {
  LOG(INFO) << "IP configuration received: " << reason;

  if (reason != kL2TPIPSecReasonConnect) {
    DCHECK(reason == kL2TPIPSecReasonDisconnect);
    // Avoid destroying the RPC task inside the adaptor callback by doing it
    // from the main event loop.
    dispatcher()->PostTask(Bind(&DeleteRPCTask, rpc_task_.release()));
    FailService(Service::kFailureUnknown);
    return;
  }

  DeletePSKFile();

  IPConfig::Properties properties;
  string interface_name;
  ParseIPConfiguration(dict, &properties, &interface_name);

  int interface_index = device_info_->GetIndex(interface_name);
  if (interface_index < 0) {
    // TODO(petkov): Consider handling the race when the RTNL notification about
    // the new PPP device has not been received yet. We can keep the IP
    // configuration and apply it when ClaimInterface is
    // invoked. crosbug.com/29970.
    NOTIMPLEMENTED() << ": No device info for " << interface_name << ".";
    return;
  }

  if (!device_) {
    device_ = new VPN(control_, dispatcher(), metrics_, manager(),
                      interface_name, interface_index);
  }
  device_->SetEnabled(true);
  device_->SelectService(service_);
  device_->UpdateIPConfig(properties);
  ReportConnectionMetrics();
  StopConnectTimeout();
}

// static
void L2TPIPSecDriver::DeleteRPCTask(RPCTask *rpc_task) {
  delete rpc_task;
}

KeyValueStore L2TPIPSecDriver::GetProvider(Error *error) {
  SLOG(VPN, 2) << __func__;
  KeyValueStore props = VPNDriver::GetProvider(error);
  props.SetBool(flimflam::kPassphraseRequiredProperty,
                args()->LookupString(
                    flimflam::kL2tpIpsecPasswordProperty, "").empty());
  props.SetBool(flimflam::kL2tpIpsecPskRequiredProperty,
                args()->LookupString(
                    flimflam::kL2tpIpsecPskProperty, "").empty());
  return props;
}

void L2TPIPSecDriver::ReportConnectionMetrics() {
  metrics_->SendEnumToUMA(
      Metrics::kMetricVpnDriver,
      Metrics::kVpnDriverL2tpIpsec,
      Metrics::kMetricVpnDriverMax);

  // We output an enum for each of the authentication types specified,
  // even if more than one is set at the same time.
  bool has_remote_authentication = false;
  if (args()->LookupString(flimflam::kL2tpIpsecCaCertNssProperty, "") != "") {
    metrics_->SendEnumToUMA(
        Metrics::kMetricVpnRemoteAuthenticationType,
        Metrics::kVpnRemoteAuthenticationTypeL2tpIpsecCertificate,
        Metrics::kMetricVpnRemoteAuthenticationTypeMax);
    has_remote_authentication = true;
  }
  if (args()->LookupString(flimflam::kL2tpIpsecPskProperty, "") != "") {
    metrics_->SendEnumToUMA(
        Metrics::kMetricVpnRemoteAuthenticationType,
        Metrics::kVpnRemoteAuthenticationTypeL2tpIpsecPsk,
        Metrics::kMetricVpnRemoteAuthenticationTypeMax);
    has_remote_authentication = true;
  }
  if (!has_remote_authentication) {
    metrics_->SendEnumToUMA(
        Metrics::kMetricVpnRemoteAuthenticationType,
        Metrics::kVpnRemoteAuthenticationTypeL2tpIpsecDefault,
        Metrics::kMetricVpnRemoteAuthenticationTypeMax);
  }

  bool has_user_authentication = false;
  if (args()->LookupString(flimflam::kL2tpIpsecClientCertIdProperty,
                           "") != "") {
    metrics_->SendEnumToUMA(
        Metrics::kMetricVpnUserAuthenticationType,
        Metrics::kVpnUserAuthenticationTypeL2tpIpsecCertificate,
        Metrics::kMetricVpnUserAuthenticationTypeMax);
    has_user_authentication = true;
  }
  if (args()->LookupString(flimflam::kL2tpIpsecPasswordProperty, "") != "") {
    metrics_->SendEnumToUMA(
        Metrics::kMetricVpnUserAuthenticationType,
        Metrics::kVpnUserAuthenticationTypeL2tpIpsecUsernamePassword,
        Metrics::kMetricVpnUserAuthenticationTypeMax);
    has_user_authentication = true;
  }
  if (!has_user_authentication) {
    metrics_->SendEnumToUMA(
        Metrics::kMetricVpnUserAuthenticationType,
        Metrics::kVpnUserAuthenticationTypeL2tpIpsecNone,
        Metrics::kMetricVpnUserAuthenticationTypeMax);
  }
}

}  // namespace shill
