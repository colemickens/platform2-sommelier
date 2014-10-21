// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/ethernet.h"

#include <linux/ethtool.h>
#include <netinet/ether.h>
#include <netinet/in.h>
#include <linux/if.h>  // NOLINT - Needs definitions from netinet/ether.h
#include <linux/sockios.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include <map>
#include <string>
#include <vector>

#include <base/bind.h>

#include "shill/adaptor_interfaces.h"
#include "shill/control_interface.h"
#include "shill/device.h"
#include "shill/device_info.h"
#include "shill/eap_credentials.h"
#include "shill/eap_listener.h"
#include "shill/ethernet_eap_provider.h"
#include "shill/ethernet_service.h"
#include "shill/event_dispatcher.h"
#include "shill/logging.h"
#include "shill/manager.h"
#include "shill/net/rtnl_handler.h"
#include "shill/profile.h"
#include "shill/proxy_factory.h"
#include "shill/supplicant_interface_proxy_interface.h"
#include "shill/supplicant_process_proxy_interface.h"
#include "shill/wpa_supplicant.h"

using std::map;
using std::string;
using std::vector;

namespace shill {

namespace Logging {
static auto kModuleLogScope = ScopeLogger::kEthernet;
static string ObjectID(Ethernet *e) { return e->GetRpcIdentifier(); }
}

Ethernet::Ethernet(ControlInterface *control_interface,
                   EventDispatcher *dispatcher,
                   Metrics *metrics,
                   Manager *manager,
                   const string &link_name,
                   const string &address,
                   int interface_index)
    : Device(control_interface,
             dispatcher,
             metrics,
             manager,
             link_name,
             address,
             interface_index,
             Technology::kEthernet),
      link_up_(false),
      is_eap_authenticated_(false),
      is_eap_detected_(false),
      eap_listener_(new EapListener(dispatcher, interface_index)),
      proxy_factory_(ProxyFactory::GetInstance()),
      sockets_(new Sockets()),
      weak_ptr_factory_(this) {
  PropertyStore *store = this->mutable_store();
  store->RegisterConstBool(kEapAuthenticationCompletedProperty,
                           &is_eap_authenticated_);
  store->RegisterConstBool(kEapAuthenticatorDetectedProperty,
                           &is_eap_detected_);
  eap_listener_->set_request_received_callback(
      base::Bind(&Ethernet::OnEapDetected, weak_ptr_factory_.GetWeakPtr()));
  SLOG(this, 2) << "Ethernet device " << link_name << " initialized.";
}

Ethernet::~Ethernet() {
  Stop(nullptr, EnabledStateChangedCallback());
}

void Ethernet::Start(Error *error,
                     const EnabledStateChangedCallback &/*callback*/) {
  service_ = new EthernetService(control_interface(),
                                 dispatcher(),
                                 metrics(),
                                 manager(),
                                 this);
  rtnl_handler()->SetInterfaceFlags(interface_index(), IFF_UP, IFF_UP);
  OnEnabledStateChanged(EnabledStateChangedCallback(), Error());
  if (error)
    error->Reset();       // indicate immediate completion
}

void Ethernet::Stop(Error *error,
                    const EnabledStateChangedCallback &/*callback*/) {
  if (service_) {
    manager()->DeregisterService(service_);
    service_ = nullptr;
  }
  StopSupplicant();
  OnEnabledStateChanged(EnabledStateChangedCallback(), Error());
  if (error)
    error->Reset();       // indicate immediate completion
}

void Ethernet::LinkEvent(unsigned int flags, unsigned int change) {
  Device::LinkEvent(flags, change);
  if ((flags & IFF_LOWER_UP) != 0 && !link_up_) {
    link_up_ = true;
    // We SetupWakeOnLan() here, instead of in Start(), because with
    // r8139, "ethtool -s eth0 wol g" fails when no cable is plugged
    // in.
    SetupWakeOnLan();
    if (service_) {
      LOG(INFO) << "Registering " << link_name() << " with manager.";
      // Manager will bring up L3 for us.
      manager()->RegisterService(service_);
    }
    eap_listener_->Start();
  } else if ((flags & IFF_LOWER_UP) == 0 && link_up_) {
    link_up_ = false;
    is_eap_detected_ = false;
    DestroyIPConfig();
    if (service_)
      manager()->DeregisterService(service_);
    SelectService(nullptr);
    GetEapProvider()->ClearCredentialChangeCallback(this);
    SetIsEapAuthenticated(false);
    StopSupplicant();
    eap_listener_->Stop();
  }
}

void Ethernet::ConnectTo(EthernetService *service) {
  CHECK(service == service_.get()) << "Ethernet was asked to connect the "
                                   << "wrong service?";
  SelectService(service);
  if (AcquireIPConfigWithLeaseName(service->GetStorageIdentifier())) {
    SetServiceState(Service::kStateConfiguring);
  } else {
    LOG(ERROR) << "Unable to acquire DHCP config.";
    SetServiceState(Service::kStateFailure);
    DestroyIPConfig();
  }
}

void Ethernet::DisconnectFrom(EthernetService *service) {
  CHECK(service == service_.get()) << "Ethernet was asked to disconnect the "
                                   << "wrong service?";
  DropConnection();
}

void Ethernet::TryEapAuthentication() {
  try_eap_authentication_callback_.Reset(
      Bind(&Ethernet::TryEapAuthenticationTask,
           weak_ptr_factory_.GetWeakPtr()));
  dispatcher()->PostTask(try_eap_authentication_callback_.callback());
}

void Ethernet::BSSAdded(const ::DBus::Path &path,
                        const map<string, ::DBus::Variant> &properties) {
  NOTREACHED() << __func__ << " is not implemented for Ethernet";
}

void Ethernet::BSSRemoved(const ::DBus::Path &path) {
  NOTREACHED() << __func__ << " is not implemented for Ethernet";
}

void Ethernet::Certification(const map<string, ::DBus::Variant> &properties) {
  string subject;
  uint32_t depth;
  if (WPASupplicant::ExtractRemoteCertification(properties, &subject, &depth)) {
    dispatcher()->PostTask(Bind(&Ethernet::CertificationTask,
                                weak_ptr_factory_.GetWeakPtr(),
                                subject, depth));
  }
}

void Ethernet::EAPEvent(const string &status, const string &parameter) {
  dispatcher()->PostTask(Bind(&Ethernet::EAPEventTask,
                              weak_ptr_factory_.GetWeakPtr(),
                              status,
                              parameter));
}

void Ethernet::PropertiesChanged(
  const map<string, ::DBus::Variant> &properties) {
  const map<string, ::DBus::Variant>::const_iterator properties_it =
      properties.find(WPASupplicant::kInterfacePropertyState);
  if (properties_it == properties.end()) {
    return;
  }
  dispatcher()->PostTask(Bind(&Ethernet::SupplicantStateChangedTask,
                              weak_ptr_factory_.GetWeakPtr(),
                              properties_it->second.reader().get_string()));
}

void Ethernet::ScanDone() {
  NOTREACHED() << __func__ << " is not implented for Ethernet";
}

EthernetEapProvider *Ethernet::GetEapProvider() {
  EthernetEapProvider *eap_provider = manager()->ethernet_eap_provider();
  CHECK(eap_provider);
  return eap_provider;
}

ServiceConstRefPtr Ethernet::GetEapService() {
  ServiceConstRefPtr eap_service = GetEapProvider()->service();
  CHECK(eap_service);
  return eap_service;
}

void Ethernet::OnEapDetected() {
  is_eap_detected_ = true;
  eap_listener_->Stop();
  GetEapProvider()->SetCredentialChangeCallback(
      this,
      base::Bind(&Ethernet::TryEapAuthentication,
                 weak_ptr_factory_.GetWeakPtr()));
  TryEapAuthentication();
}

bool Ethernet::StartSupplicant() {
  if (supplicant_process_proxy_.get()) {
    return true;
  }

  supplicant_process_proxy_.reset(
      proxy_factory_->CreateSupplicantProcessProxy(
          WPASupplicant::kDBusPath, WPASupplicant::kDBusAddr));
  ::DBus::Path interface_path;
  try {
    map<string, DBus::Variant> create_interface_args;
    create_interface_args[WPASupplicant::kInterfacePropertyName].writer().
        append_string(link_name().c_str());
    create_interface_args[WPASupplicant::kInterfacePropertyDriver].writer().
        append_string(WPASupplicant::kDriverWired);
    create_interface_args[
        WPASupplicant::kInterfacePropertyConfigFile].writer().
        append_string(WPASupplicant::kSupplicantConfPath);
    interface_path =
        supplicant_process_proxy_->CreateInterface(create_interface_args);
  } catch (const DBus::Error &e) {  // NOLINT
    if (!strcmp(e.name(), WPASupplicant::kErrorInterfaceExists)) {
      interface_path = supplicant_process_proxy_->GetInterface(link_name());
    } else {
      LOG(ERROR) << __func__ << ": Failed to create interface with supplicant.";
      StopSupplicant();
      return false;
    }
  }
  supplicant_interface_proxy_.reset(
      proxy_factory_->CreateSupplicantInterfaceProxy(
          this, interface_path, WPASupplicant::kDBusAddr));
  supplicant_interface_path_ = interface_path;
  return true;
}

bool Ethernet::StartEapAuthentication() {
  map<string, DBus::Variant> params;
  GetEapService()->eap()->PopulateSupplicantProperties(
      &certificate_file_, &params);
  params[WPASupplicant::kNetworkPropertyEapKeyManagement].writer().
      append_string(WPASupplicant::kKeyManagementIeee8021X);
  params[WPASupplicant::kNetworkPropertyEapolFlags].writer().
      append_uint32(0);
  params[WPASupplicant::kNetworkPropertyScanSSID].writer().
      append_uint32(0);
  service_->ClearEAPCertification();
  eap_state_handler_.Reset();

  try {
    if (!supplicant_network_path_.empty()) {
      supplicant_interface_proxy_->RemoveNetwork(supplicant_network_path_);
    }
    supplicant_network_path_ = supplicant_interface_proxy_->AddNetwork(params);
    CHECK(!supplicant_network_path_.empty());
  } catch (const DBus::Error &e) {  // NOLINT
    LOG(ERROR) << "exception while adding network: " << e.what();
    return false;
  }

  supplicant_interface_proxy_->SelectNetwork(supplicant_network_path_);
  supplicant_interface_proxy_->EAPLogon();
  return true;
}

void Ethernet::StopSupplicant() {
  if (supplicant_interface_proxy_.get()) {
    supplicant_interface_proxy_->EAPLogoff();
  }
  supplicant_interface_proxy_.reset();
  if (!supplicant_interface_path_.empty() && supplicant_process_proxy_.get()) {
    try {
      supplicant_process_proxy_->RemoveInterface(
          ::DBus::Path(supplicant_interface_path_));
    } catch (const DBus::Error &e) {  // NOLINT
      LOG(ERROR) << __func__ << ": Failed to remove interface from supplicant.";
    }
  }
  supplicant_network_path_ = "";
  supplicant_interface_path_ = "";
  supplicant_process_proxy_.reset();
  SetIsEapAuthenticated(false);
}

void Ethernet::SetIsEapAuthenticated(bool is_eap_authenticated) {
  if (is_eap_authenticated == is_eap_authenticated_) {
    return;
  }

  // If our EAP authentication state changes, we have now joined a different
  // network.  Restart the DHCP process and any other connection state.
  DisconnectFrom(service_);
  if (service_ && link_up_) {
    ConnectTo(service_);
  }
  is_eap_authenticated_ = is_eap_authenticated;
  adaptor()->EmitBoolChanged(kEapAuthenticationCompletedProperty,
                             is_eap_authenticated_);
}

void Ethernet::CertificationTask(const string &subject, uint32_t depth) {
  if (!service_) {
    LOG(ERROR) << "Ethernet " << link_name() << " " << __func__
               << " with no service.";
    return;
  }

  service_->AddEAPCertification(subject, depth);
}

void Ethernet::EAPEventTask(const string &status, const string &parameter) {
  LOG(INFO) << "In " << __func__ << " with status " << status
            << ", parameter " << parameter;
  Service::ConnectFailure failure = Service::kFailureUnknown;
  if (eap_state_handler_.ParseStatus(status, parameter, &failure)) {
    LOG(INFO) << "EAP authentication succeeded!";
    SetIsEapAuthenticated(true);
  } else if (failure != Service::Service::kFailureUnknown) {
    LOG(INFO) << "EAP authentication failed!";
    SetIsEapAuthenticated(false);
  }
}

void Ethernet::SupplicantStateChangedTask(const string &state) {
  LOG(INFO) << "Supplicant state changed to " << state;
}

void Ethernet::TryEapAuthenticationTask() {
  if (!service_) {
    LOG(INFO) << "Service is missing; not doing EAP authentication.";
    return;
  }

  if (!GetEapService()->Is8021xConnectable()) {
    if (is_eap_authenticated_) {
      LOG(INFO) << "EAP Service lost 802.1X credentials; "
                << "terminating EAP authentication.";
    } else {
      LOG(INFO) << "EAP Service lacks 802.1X credentials; "
                << "not doing EAP authentication.";
    }
    StopSupplicant();
    return;
  }

  if (!is_eap_detected_) {
    LOG(WARNING) << "EAP authenticator not detected; "
                 << "not doing EAP authentication.";
    return;
  }
  if (!StartSupplicant()) {
    LOG(ERROR) << "Failed to start supplicant.";
    return;
  }
  StartEapAuthentication();
}

void Ethernet::SetupWakeOnLan() {
  int sock;
  struct ifreq interface_command;
  struct ethtool_wolinfo wake_on_lan_command;

  if (link_name().length() >= sizeof(interface_command.ifr_name)) {
    LOG(WARNING) << "Interface name " << link_name() << " too long: "
                 << link_name().size() << " >= "
                 << sizeof(interface_command.ifr_name);
    return;
  }

  sock = sockets_->Socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);
  if (sock < 0) {
    LOG(WARNING) << "Failed to allocate socket: "
                 << sockets_->ErrorString() << ".";
    return;
  }
  ScopedSocketCloser socket_closer(sockets_.get(), sock);

  memset(&interface_command, 0, sizeof(interface_command));
  memset(&wake_on_lan_command, 0, sizeof(wake_on_lan_command));
  wake_on_lan_command.cmd = ETHTOOL_SWOL;
  if (manager()->IsWakeOnLanEnabled()) {
    wake_on_lan_command.wolopts = WAKE_MAGIC;
  }
  interface_command.ifr_data = &wake_on_lan_command;
  memcpy(interface_command.ifr_name,
         link_name().data(), link_name().length());

  int res = sockets_->Ioctl(sock, SIOCETHTOOL, &interface_command);
  if (res < 0) {
    LOG(WARNING) << "Failed to enable wake-on-lan: "
                 << sockets_->ErrorString() << ".";
    return;
  }
}

}  // namespace shill
