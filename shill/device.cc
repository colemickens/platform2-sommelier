// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/device.h"

#include <netinet/in.h>
#include <linux/if.h>  // Needs definitions from netinet/in.h
#include <stdio.h>
#include <time.h>

#include <string>
#include <vector>

#include <base/file_util.h>
#include <base/logging.h>
#include <base/memory/ref_counted.h>
#include <base/stringprintf.h>
#include <chromeos/dbus/service_constants.h>

#include "shill/connection.h"
#include "shill/control_interface.h"
#include "shill/device_dbus_adaptor.h"
#include "shill/dhcp_config.h"
#include "shill/dhcp_provider.h"
#include "shill/error.h"
#include "shill/event_dispatcher.h"
#include "shill/http_proxy.h"
#include "shill/manager.h"
#include "shill/property_accessor.h"
#include "shill/refptr_types.h"
#include "shill/rtnl_handler.h"
#include "shill/service.h"
#include "shill/store_interface.h"
#include "shill/technology.h"

using base::StringPrintf;
using std::string;
using std::vector;

namespace shill {

// static
const char Device::kIPFlagTemplate[] = "/proc/sys/net/%s/conf/%s/%s";
// static
const char Device::kIPFlagVersion4[] = "ipv4";
// static
const char Device::kIPFlagVersion6[] = "ipv6";
// static
const char Device::kIPFlagDisableIPv6[] = "disable_ipv6";
// static
const char Device::kIPFlagUseTempAddr[] = "use_tempaddr";
// static
const char Device::kIPFlagUseTempAddrUsedAndDefault[] = "2";
// static
const char Device::kIPFlagReversePathFilter[] = "rp_filter";
// static
const char Device::kIPFlagReversePathFilterEnabled[] = "1";
// static
const char Device::kIPFlagReversePathFilterLooseMode[] = "2";

// static
const char Device::kStoragePowered[] = "Powered";

// static
const char Device::kStorageIPConfigs[] = "IPConfigs";

Device::Device(ControlInterface *control_interface,
               EventDispatcher *dispatcher,
               Manager *manager,
               const string &link_name,
               const string &address,
               int interface_index,
               Technology::Identifier technology)
    : powered_(true),
      reconnect_(true),
      hardware_address_(address),
      interface_index_(interface_index),
      running_(false),
      link_name_(link_name),
      unique_id_(link_name),
      control_interface_(control_interface),
      dispatcher_(dispatcher),
      manager_(manager),
      adaptor_(control_interface->CreateDeviceAdaptor(this)),
      technology_(technology),
      dhcp_provider_(DHCPProvider::GetInstance()),
      rtnl_handler_(RTNLHandler::GetInstance()) {
  store_.RegisterConstString(flimflam::kAddressProperty, &hardware_address_);

  // flimflam::kBgscanMethodProperty: Registered in WiFi
  // flimflam::kBgscanShortIntervalProperty: Registered in WiFi
  // flimflam::kBgscanSignalThresholdProperty: Registered in WiFi

  // flimflam::kCellularAllowRoamingProperty: Registered in Cellular
  // flimflam::kCarrierProperty: Registered in Cellular
  // flimflam::kEsnProperty: Registered in Cellular
  // flimflam::kHomeProviderProperty: Registered in Cellular
  // flimflam::kImeiProperty: Registered in Cellular
  // flimflam::kImsiProperty: Registered in Cellular
  // flimflam::kManufacturerProperty: Registered in Cellular
  // flimflam::kMdnProperty: Registered in Cellular
  // flimflam::kMeidProperty: Registered in Cellular
  // flimflam::kMinProperty: Registered in Cellular
  // flimflam::kModelIDProperty: Registered in Cellular
  // flimflam::kFirmwareRevisionProperty: Registered in Cellular
  // flimflam::kHardwareRevisionProperty: Registered in Cellular
  // flimflam::kPRLVersionProperty: Registered in Cellular
  // flimflam::kSIMLockStatusProperty: Registered in Cellular
  // flimflam::kFoundNetworksProperty: Registered in Cellular
  // flimflam::kDBusConnectionProperty: Registered in Cellular
  // flimflam::kDBusObjectProperty: Register in Cellular

  // TODO(cmasone): Chrome doesn't use this...does anyone?
  // store_.RegisterConstString(flimflam::kInterfaceProperty, &link_name_);
  HelpRegisterDerivedStrings(flimflam::kIPConfigsProperty,
                             &Device::AvailableIPConfigs,
                             NULL);
  store_.RegisterConstString(flimflam::kNameProperty, &link_name_);
  store_.RegisterBool(flimflam::kPoweredProperty, &powered_);
  // TODO(cmasone): Chrome doesn't use this...does anyone?
  // store_.RegisterConstBool(flimflam::kReconnectProperty, &reconnect_);

  // TODO(cmasone): Figure out what shill concept maps to flimflam's "Network".
  // known_properties_.push_back(flimflam::kNetworksProperty);

  // flimflam::kScanningProperty: Registered in WiFi, Cellular
  // flimflam::kScanIntervalProperty: Registered in WiFi, Cellular

  // TODO(pstew): Initialize Interface monitor, so we can detect new interfaces
  VLOG(2) << "Device " << link_name_ << " index " << interface_index;
}

Device::~Device() {
  VLOG(2) << "Device " << link_name_ << " destroyed.";
}

void Device::Start() {
  running_ = true;
  VLOG(2) << "Device " << link_name_ << " starting.";
  adaptor_->UpdateEnabled();
}

void Device::Stop() {
  VLOG(2) << "Device " << link_name_ << " stopping.";
  running_ = false;
  DestroyIPConfig();         // breaks a reference cycle
  SelectService(NULL);       // breaks a reference cycle
  adaptor_->UpdateEnabled();
  rtnl_handler_->SetInterfaceFlags(interface_index(), 0, IFF_UP);

  VLOG(3) << "Device " << link_name_ << " ipconfig_ "
          << (ipconfig_.get() ? "is set." : "is not set.");
  VLOG(3) << "Device " << link_name_ << " connection_ "
          << (connection_.get() ? "is set." : "is not set.");
  VLOG(3) << "Device " << link_name_ << " selected_service_ "
          << (selected_service_.get() ? "is set." : "is not set.");
}

bool Device::TechnologyIs(const Technology::Identifier /*type*/) const {
  return false;
}

void Device::LinkEvent(unsigned flags, unsigned change) {
  VLOG(2) << "Device " << link_name_
          << std::showbase << std::hex
          << " flags " << flags << " changed " << change
          << std::dec << std::noshowbase;
}

void Device::Scan(Error *error) {
  VLOG(2) << "Device " << link_name_ << " scan requested.";
  Error::PopulateAndLog(error, Error::kNotSupported,
                        "Device doesn't support scan.");
}

void Device::RegisterOnNetwork(const std::string &/*network_id*/,
                               Error *error) {
  Error::PopulateAndLog(error, Error::kNotSupported,
                        "Device doesn't support network registration.");
}

void Device::RequirePIN(
    const string &/*pin*/, bool /*require*/, ReturnerInterface *returner) {
  VLOG(2) << __func__;
  Error error;
  Error::PopulateAndLog(
      &error, Error::kNotSupported, "Device doesn't support RequirePIN.");
  returner->ReturnError(error);
}

void Device::EnterPIN(const string &/*pin*/, ReturnerInterface *returner) {
  VLOG(2) << __func__;
  Error error;
  Error::PopulateAndLog(
      &error, Error::kNotSupported, "Device doesn't support EnterPIN.");
  returner->ReturnError(error);
}

void Device::UnblockPIN(const string &/*unblock_code*/,
                        const string &/*pin*/,
                        ReturnerInterface *returner) {
  VLOG(2) << __func__;
  Error error;
  Error::PopulateAndLog(
      &error, Error::kNotSupported, "Device doesn't support UnblockPIN.");
  returner->ReturnError(error);
}

void Device::ChangePIN(const string &/*old_pin*/,
                       const string &/*new_pin*/,
                       ReturnerInterface *returner) {
  VLOG(2) << __func__;
  Error error;
  Error::PopulateAndLog(
      &error, Error::kNotSupported, "Device doesn't support ChangePIN.");
  returner->ReturnError(error);
}

void Device::DisableIPv6() {
  SetIPFlag(IPAddress::kFamilyIPv6, kIPFlagDisableIPv6, "1");
}

void Device::EnableIPv6() {
  SetIPFlag(IPAddress::kFamilyIPv6, kIPFlagDisableIPv6, "0");
}

void Device::EnableIPv6Privacy() {
  SetIPFlag(IPAddress::kFamilyIPv6, kIPFlagUseTempAddr,
            kIPFlagUseTempAddrUsedAndDefault);
}

void Device::DisableReversePathFilter() {
  // TODO(pstew): Current kernel doesn't offer reverse-path filtering flag
  // for IPv6.  crosbug.com/24228
  SetIPFlag(IPAddress::kFamilyIPv4, kIPFlagReversePathFilter,
            kIPFlagReversePathFilterLooseMode);
}

void Device::EnableReversePathFilter() {
  SetIPFlag(IPAddress::kFamilyIPv4, kIPFlagReversePathFilter,
            kIPFlagReversePathFilterEnabled);
}

bool Device::IsConnected() const {
  if (selected_service_)
    return selected_service_->IsConnected();
  return false;
}

string Device::GetRpcIdentifier() {
  return adaptor_->GetRpcIdentifier();
}

string Device::GetStorageIdentifier() {
  string id = GetRpcIdentifier();
  ControlInterface::RpcIdToStorageId(&id);
  size_t needle = id.find('_');
  DLOG_IF(ERROR, needle == string::npos) << "No _ in storage id?!?!";
  id.replace(id.begin() + needle + 1, id.end(), hardware_address_);
  return id;
}

const string& Device::FriendlyName() const {
  return link_name_;
}

const string& Device::UniqueName() const {
  return unique_id_;
}

bool Device::Load(StoreInterface *storage) {
  const string id = GetStorageIdentifier();
  if (!storage->ContainsGroup(id)) {
    LOG(WARNING) << "Device is not available in the persistent store: " << id;
    return false;
  }
  storage->GetBool(id, kStoragePowered, &powered_);
  // TODO(cmasone): What does it mean to load an IPConfig identifier??
  return true;
}

bool Device::Save(StoreInterface *storage) {
  const string id = GetStorageIdentifier();
  storage->SetBool(id, kStoragePowered, powered_);
  if (ipconfig_.get()) {
    // The _0 is an index into the list of IPConfigs that this device might
    // have.  We only have one IPConfig right now, and I hope to never have
    // to support more, as sleffler indicates that associating IPConfigs
    // with devices is wrong and due to be changed in flimflam anyhow.
    string suffix = hardware_address_ + "_0";
    ipconfig_->Save(storage, suffix);
    storage->SetString(id, kStorageIPConfigs, SerializeIPConfigs(suffix));
  }
  return true;
}

void Device::DestroyIPConfig() {
  DisableIPv6();
  if (ipconfig_.get()) {
    ipconfig_->ReleaseIP();
    ipconfig_ = NULL;
  }
  DestroyConnection();
}

bool Device::AcquireIPConfig() {
  DestroyIPConfig();
  EnableIPv6();
  ipconfig_ = dhcp_provider_->CreateConfig(link_name_);
  ipconfig_->RegisterUpdateCallback(
      NewCallback(this, &Device::IPConfigUpdatedCallback));
  return ipconfig_->RequestIP();
}

void Device::HelpRegisterDerivedStrings(
    const string &name,
    Strings(Device::*get)(Error *),
    void(Device::*set)(const Strings&, Error *)) {
  store_.RegisterDerivedStrings(
      name,
      StringsAccessor(new CustomAccessor<Device, Strings>(this, get, set)));
}

void Device::IPConfigUpdatedCallback(const IPConfigRefPtr &ipconfig,
                                     bool success) {
  VLOG(2) << __func__ << " " << " success: " << success;
  if (success) {
    CreateConnection();
    connection_->UpdateFromIPConfig(ipconfig);
    // SetConnection must occur after the UpdateFromIPConfig so the
    // service can use the values derived from the connection.
    if (selected_service_.get()) {
      selected_service_->SetConnection(connection_);
    }
    // The service state change needs to happen last, so that at the
    // time we report the state change to the manager, the service
    // has its connection.
    SetServiceState(Service::kStateConnected);
  } else {
    // TODO(pstew): This logic gets more complex when multiple IPConfig types
    // are run in parallel (e.g. DHCP and DHCP6)
    SetServiceState(Service::kStateDisconnected);
    DestroyConnection();
  }
}

void Device::CreateConnection() {
  VLOG(2) << __func__;
  if (!connection_.get()) {
    connection_ = new Connection(interface_index_,
                                 link_name_,
                                 manager_->device_info());
  }
}

void Device::DestroyConnection() {
  VLOG(2) << __func__;
  if (selected_service_.get()) {
    selected_service_->SetConnection(NULL);
  }
  connection_ = NULL;
}

void Device::SelectService(const ServiceRefPtr &service) {
  VLOG(2) << __func__ << ": "
          << (service.get() ?
              StringPrintf("%s (%s)",
                           service->UniqueName().c_str(),
                           service->friendly_name().c_str()) :
              "*reset*");

  if (selected_service_.get() == service.get()) {
    // No change to |selected_service_|. Return early to avoid
    // changing its state.
    return;
  }

  if (selected_service_.get()) {
    if (selected_service_->state() != Service::kStateFailure) {
      selected_service_->SetState(Service::kStateIdle);
    }
    // TODO(pstew): We need to revisit the model here: should the Device
    // subclass be responsible for calling DestroyIPConfig() (which would
    // trigger DestroyConnection() and Service::SetConnection(NULL))?
    // Ethernet does, but WiFi currently does not.  crosbug.com/23929
    selected_service_->SetConnection(NULL);
  }
  selected_service_ = service;
}

void Device::SetServiceState(Service::ConnectState state) {
  if (selected_service_.get()) {
    selected_service_->SetState(state);
  }
}

void Device::SetServiceFailure(Service::ConnectFailure failure_state) {
  if (selected_service_.get()) {
    selected_service_->SetFailure(failure_state);
  }
}

string Device::SerializeIPConfigs(const string &suffix) {
  return StringPrintf("%s:%s", suffix.c_str(), ipconfig_->type().c_str());
}

bool Device::SetIPFlag(IPAddress::Family family, const string &flag,
                       const string &value) {
  string ip_version;
  if (family == IPAddress::kFamilyIPv4) {
    ip_version = kIPFlagVersion4;
  } else if (family == IPAddress::kFamilyIPv6) {
    ip_version = kIPFlagVersion6;
  } else {
    NOTIMPLEMENTED();
  }
  FilePath flag_file(StringPrintf(kIPFlagTemplate, ip_version.c_str(),
                                  link_name_.c_str(), flag.c_str()));
  VLOG(2) << "Writing " << value << " to flag file " << flag_file.value();
  if (file_util::WriteFile(flag_file, value.c_str(), value.length()) != 1) {
    LOG(ERROR) << StringPrintf("IP flag write failed: %s to %s",
                               value.c_str(), flag_file.value().c_str());
    return false;
  }
  return true;
}

vector<string> Device::AvailableIPConfigs(Error */*error*/) {
  string id = (ipconfig_.get() ? ipconfig_->GetRpcIdentifier() : string());
  return vector<string>(1, id);
}

string Device::GetRpcConnectionIdentifier() {
  return adaptor_->GetRpcConnectionIdentifier();
}

}  // namespace shill
