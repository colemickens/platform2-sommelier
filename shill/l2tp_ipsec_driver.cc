// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/l2tp_ipsec_driver.h"

#include <base/file_util.h>
#include <base/logging.h>
#include <chromeos/dbus/service_constants.h>

#include "shill/error.h"
#include "shill/manager.h"
#include "shill/nss.h"

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

L2TPIPSecDriver::L2TPIPSecDriver(Manager *manager)
    : manager_(manager),
      nss_(NSS::GetInstance()) {}

L2TPIPSecDriver::~L2TPIPSecDriver() {}

bool L2TPIPSecDriver::ClaimInterface(const string &link_name,
                                     int interface_index) {
  // TODO(petkov): crosbug.com/26843.
  NOTIMPLEMENTED();
  return false;
}

void L2TPIPSecDriver::Connect(const VPNServiceRefPtr &service, Error *error) {
  // TODO(petkov): crosbug.com/26843.
  NOTIMPLEMENTED();
}

void L2TPIPSecDriver::Disconnect() {
  // TODO(petkov): crosbug.com/29364.
  NOTIMPLEMENTED();
}

bool L2TPIPSecDriver::Load(StoreInterface *storage, const string &storage_id) {
  // TODO(petkov): crosbug.com/29362.
  NOTIMPLEMENTED();
  return false;
}

bool L2TPIPSecDriver::Save(StoreInterface *storage, const string &storage_id) {
  // TODO(petkov): crosbug.com/29362.
  NOTIMPLEMENTED();
  return false;
}

void L2TPIPSecDriver::InitPropertyStore(PropertyStore *store) {
  // TODO(petkov): crosbug.com/29362.
  NOTIMPLEMENTED();
}

string L2TPIPSecDriver::GetProviderType() const {
  return flimflam::kProviderL2tpIpsec;
}

void L2TPIPSecDriver::Cleanup() {
  VLOG(2) << __func__;
  if (!psk_file_.empty()) {
    file_util::Delete(psk_file_, false);
    psk_file_.clear();
  }
}

void L2TPIPSecDriver::InitOptions(vector<string> *options, Error *error) {
  string vpnhost = args_.LookupString(flimflam::kProviderHostProperty, "");
  if (vpnhost.empty()) {
    Error::PopulateAndLog(
        error, Error::kInvalidArguments, "VPN host not specified.");
    return;
  }

  if (!InitPSKOptions(options, error)) {
    return;
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
}

bool L2TPIPSecDriver::InitPSKOptions(vector<string> *options, Error *error) {
  string psk = args_.LookupString(flimflam::kL2tpIpsecPskProperty, "");
  if (!psk.empty()) {
    if (!file_util::CreateTemporaryFileInDir(
            manager_->run_path(), &psk_file_) ||
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

}  // namespace shill
