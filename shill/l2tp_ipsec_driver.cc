// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/l2tp_ipsec_driver.h"

#include <base/file_util.h>
#include <base/logging.h>
#include <base/string_util.h>
#include <chromeos/dbus/service_constants.h>

#include "shill/device_info.h"
#include "shill/error.h"
#include "shill/manager.h"
#include "shill/nss.h"
#include "shill/scope_logger.h"
#include "shill/vpn.h"
#include "shill/vpn_service.h"

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
const char L2TPIPSecDriver::kPPPDPlugin[] = SCRIPTDIR "/libppp-plugin.so";
// static
const char L2TPIPSecDriver::kL2TPIPSecVPNPath[] = "/usr/sbin/l2tpipsec_vpn";
// static
const VPNDriver::Property L2TPIPSecDriver::kProperties[] = {
  { flimflam::kL2tpIpsecCaCertNssProperty, 0 },
  { flimflam::kL2tpIpsecClientCertIdProperty, 0 },
  { flimflam::kL2tpIpsecClientCertSlotProperty, 0 },
  { flimflam::kL2tpIpsecPasswordProperty, Property::kCrypted },
  { flimflam::kL2tpIpsecPinProperty, Property::kEphemeral },
  { flimflam::kL2tpIpsecPskProperty, Property::kCrypted },
  { flimflam::kL2tpIpsecUserProperty, 0 },
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
    : VPNDriver(manager, kProperties, arraysize(kProperties)),
      control_(control),
      dispatcher_(dispatcher),
      metrics_(metrics),
      device_info_(device_info),
      glib_(glib),
      nss_(NSS::GetInstance()),
      pid_(0),
      child_watch_tag_(0) {}

L2TPIPSecDriver::~L2TPIPSecDriver() {
  Cleanup(Service::kStateIdle);
}

bool L2TPIPSecDriver::ClaimInterface(const string &link_name,
                                     int interface_index) {
  // TODO(petkov): crosbug.com/26843.
  NOTIMPLEMENTED();
  return false;
}

void L2TPIPSecDriver::Connect(const VPNServiceRefPtr &service, Error *error) {
  service_ = service;
  service_->SetState(Service::kStateConfiguring);
  rpc_task_.reset(new RPCTask(control_, this));
  if (!SpawnL2TPIPSecVPN(error)) {
    Cleanup(Service::kStateFailure);
  }
}

void L2TPIPSecDriver::Disconnect() {
  // TODO(petkov): crosbug.com/29364.
  NOTIMPLEMENTED();
}

string L2TPIPSecDriver::GetProviderType() const {
  return flimflam::kProviderL2tpIpsec;
}

void L2TPIPSecDriver::Cleanup(Service::ConnectState state) {
  SLOG(VPN, 2) << __func__
               << "(" << Service::ConnectStateToString(state) << ")";
  DeletePSKFile();
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
  if (device_) {
    device_->OnDisconnected();
    device_->SetEnabled(false);
    device_ = NULL;
  }
  rpc_task_.reset();
  if (service_) {
    service_->SetState(state);
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
  SLOG(VPN, 2) << "L2TP/IPSec VPN process options: "
               << JoinString(options, ' ');

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
  // Redirect all l2tp/ipsec output to stderr.
  int stderr_fd = fileno(stderr);
  if (!glib_->SpawnAsyncWithPipesCWD(process_args.data(),
                                     process_env.data(),
                                     G_SPAWN_DO_NOT_REAP_CHILD,
                                     NULL,
                                     NULL,
                                     &pid_,
                                     NULL,
                                     &stderr_fd,
                                     &stderr_fd,
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
  environment->push_back(
      "CONNMAN_BUSNAME=" + rpc_task_->GetRpcConnectionIdentifier());
  environment->push_back(
      "CONNMAN_INTERFACE=" + rpc_task_->GetRpcInterfaceIdentifier());
  environment->push_back(
      "CONNMAN_PATH=" + rpc_task_->GetRpcIdentifier());
}

bool L2TPIPSecDriver::InitOptions(vector<string> *options, Error *error) {
  string vpnhost = args_.LookupString(flimflam::kProviderHostProperty, "");
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

  InitNSSOptions(options);

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
  string psk = args_.LookupString(flimflam::kL2tpIpsecPskProperty, "");
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
      args_.LookupString(flimflam::kL2tpIpsecCaCertNssProperty, "");
  if (!ca_cert.empty()) {
    const string &vpnhost = args_.GetString(flimflam::kProviderHostProperty);
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

bool L2TPIPSecDriver::AppendValueOption(
    const string &property, const string &option, vector<string> *options) {
  string value = args_.LookupString(property, "");
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
  string value = args_.LookupString(property, "");
  if (!value.empty()) {
    options->push_back(value == "true" ? true_option : false_option);
    return true;
  }
  return false;
}

// static
void L2TPIPSecDriver::OnL2TPIPSecVPNDied(GPid pid, gint status, gpointer data) {
  SLOG(VPN, 2) << __func__ << "(" << pid << ", "  << status << ")";
  L2TPIPSecDriver *me = reinterpret_cast<L2TPIPSecDriver *>(data);
  me->child_watch_tag_ = 0;
  CHECK_EQ(pid, me->pid_);
  me->Cleanup(Service::kStateFailure);
  // TODO(petkov): Figure if we need to restart the connection.
}

void L2TPIPSecDriver::GetLogin(string *user, string *password) {
  SLOG(VPN, 2) << __func__;
  string user_property =
      args_.LookupString(flimflam::kL2tpIpsecUserProperty, "");
  if (user_property.empty()) {
    LOG(ERROR) << "User not set.";
    return;
  }
  string password_property =
      args_.LookupString(flimflam::kL2tpIpsecPasswordProperty, "");
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
    if (key == "INTERNAL_IP4_ADDRESS") {
      properties->address = value;
    } else if (key == "EXTERNAL_IP4_ADDRESS") {
      properties->peer_address = value;
    } else if (key == "GATEWAY_ADDRESS") {
      properties->gateway = value;
    } else if (key == "DNS1") {
      properties->dns_servers.insert(properties->dns_servers.begin(), value);
    } else if (key == "DNS2") {
      properties->dns_servers.push_back(value);
    } else if (key == "INTERNAL_IFNAME") {
      *interface_name = value;
    } else if (key == "LNS_ADDRESS") {
      properties->trusted_ip = value;
    } else {
      SLOG(VPN, 2) << "Key ignored.";
    }
  }
}

void L2TPIPSecDriver::Notify(
    const string &reason, const map<string, string> &dict) {
  SLOG(VPN, 2) << __func__ << "(" << reason << ")";

  if (reason != "connect") {
    // TODO(petkov): Disconnect the device (crosbug.com/29364).
    NOTIMPLEMENTED();
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
    // configuration and apply it when ClaimInterface is invoked.
    NOTIMPLEMENTED() << ": No device info for " << interface_name << ".";
    return;
  }

  if (!device_) {
    device_ = new VPN(control_, dispatcher_, metrics_, manager(),
                      interface_name, interface_index);
  }
  device_->SetEnabled(true);
  device_->SelectService(service_);
  device_->UpdateIPConfig(properties);
}

}  // namespace shill
