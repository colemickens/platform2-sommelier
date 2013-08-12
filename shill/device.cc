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

#include <base/bind.h>
#include <base/file_util.h>
#include <base/memory/ref_counted.h>
#include <base/stringprintf.h>
#include <chromeos/dbus/service_constants.h>

#include "shill/async_connection.h"
#include "shill/connection.h"
#include "shill/connection_health_checker.h"
#include "shill/control_interface.h"
#include "shill/device_dbus_adaptor.h"
#include "shill/dhcp_config.h"
#include "shill/dhcp_provider.h"
#include "shill/dns_client.h"
#include "shill/error.h"
#include "shill/event_dispatcher.h"
#include "shill/geolocation_info.h"
#include "shill/http_proxy.h"
#include "shill/ip_address.h"
#include "shill/link_monitor.h"
#include "shill/logging.h"
#include "shill/manager.h"
#include "shill/metrics.h"
#include "shill/property_accessor.h"
#include "shill/refptr_types.h"
#include "shill/rtnl_handler.h"
#include "shill/service.h"
#include "shill/socket_info_reader.h"
#include "shill/store_interface.h"
#include "shill/technology.h"
#include "shill/traffic_monitor.h"

using base::Bind;
using base::FilePath;
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
const char Device::kStorageIPConfigs[] = "IPConfigs";
// static
const char Device::kStoragePowered[] = "Powered";
// static
const char Device::kStorageReceiveByteCount[] = "ReceiveByteCount";
// static
const char Device::kStorageTransmitByteCount[] = "TransmitByteCount";

Device::Device(ControlInterface *control_interface,
               EventDispatcher *dispatcher,
               Metrics *metrics,
               Manager *manager,
               const string &link_name,
               const string &address,
               int interface_index,
               Technology::Identifier technology)
    : enabled_(false),
      enabled_persistent_(true),
      enabled_pending_(enabled_),
      reconnect_(true),
      hardware_address_(address),
      interface_index_(interface_index),
      running_(false),
      link_name_(link_name),
      unique_id_(link_name),
      control_interface_(control_interface),
      dispatcher_(dispatcher),
      metrics_(metrics),
      manager_(manager),
      weak_ptr_factory_(this),
      adaptor_(control_interface->CreateDeviceAdaptor(this)),
      traffic_monitor_enabled_(false),
      portal_detector_callback_(Bind(&Device::PortalDetectorCallback,
                                     weak_ptr_factory_.GetWeakPtr())),
      technology_(technology),
      portal_attempts_to_online_(0),
      receive_byte_offset_(0),
      transmit_byte_offset_(0),
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
  // flimflam::kIccidProperty: Registered in Cellular
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

  store_.RegisterConstString(flimflam::kInterfaceProperty, &link_name_);
  HelpRegisterConstDerivedRpcIdentifiers(flimflam::kIPConfigsProperty,
                                         &Device::AvailableIPConfigs);
  store_.RegisterConstString(flimflam::kNameProperty, &link_name_);
  store_.RegisterConstBool(flimflam::kPoweredProperty, &enabled_);
  HelpRegisterConstDerivedString(flimflam::kTypeProperty,
                                 &Device::GetTechnologyString);
  HelpRegisterConstDerivedUint64(shill::kLinkMonitorResponseTimeProperty,
                                 &Device::GetLinkMonitorResponseTime);

  // TODO(cmasone): Chrome doesn't use this...does anyone?
  // store_.RegisterConstBool(flimflam::kReconnectProperty, &reconnect_);

  // TODO(cmasone): Figure out what shill concept maps to flimflam's "Network".
  // known_properties_.push_back(flimflam::kNetworksProperty);

  // flimflam::kScanningProperty: Registered in WiFi, Cellular
  // flimflam::kScanIntervalProperty: Registered in WiFi, Cellular

  if (manager_ && manager_->device_info()) {  // Unit tests may not have these.
    manager_->device_info()->GetByteCounts(
        interface_index_, &receive_byte_offset_, &transmit_byte_offset_);
    HelpRegisterConstDerivedUint64(shill::kReceiveByteCountProperty,
                                   &Device::GetReceiveByteCountProperty);
    HelpRegisterConstDerivedUint64(shill::kTransmitByteCountProperty,
                                   &Device::GetTransmitByteCountProperty);
  }

  LOG(INFO) << "Device created: " << link_name_
            << " index " << interface_index_;
}

Device::~Device() {
  LOG(INFO) << "Device destructed: " << link_name_
            << " index " << interface_index_;
}

void Device::LinkEvent(unsigned flags, unsigned change) {
  SLOG(Device, 2) << "Device " << link_name_
                  << std::showbase << std::hex
                  << " flags " << flags << " changed " << change
                  << std::dec << std::noshowbase;
}

void Device::Scan(ScanType scan_type, Error *error, const string &reason) {
  SLOG(Device, 2) << __func__ << " [Device] on " << link_name() << " from "
                  << reason;
  Error::PopulateAndLog(error, Error::kNotSupported,
                        "Device doesn't support scan.");
}

void Device::RegisterOnNetwork(const std::string &/*network_id*/, Error *error,
                                 const ResultCallback &/*callback*/) {
  Error::PopulateAndLog(error, Error::kNotSupported,
                        "Device doesn't support network registration.");
}

void Device::RequirePIN(
    const string &/*pin*/, bool /*require*/,
    Error *error, const ResultCallback &/*callback*/) {
  SLOG(Device, 2) << __func__;
  Error::PopulateAndLog(error, Error::kNotSupported,
                        "Device doesn't support RequirePIN.");
}

void Device::EnterPIN(const string &/*pin*/,
                      Error *error, const ResultCallback &/*callback*/) {
  SLOG(Device, 2) << __func__;
  Error::PopulateAndLog(error, Error::kNotSupported,
                        "Device doesn't support EnterPIN.");
}

void Device::UnblockPIN(const string &/*unblock_code*/,
                        const string &/*pin*/,
                        Error *error, const ResultCallback &/*callback*/) {
  SLOG(Device, 2) << __func__;
  Error::PopulateAndLog(error, Error::kNotSupported,
                        "Device doesn't support UnblockPIN.");
}

void Device::ChangePIN(const string &/*old_pin*/,
                       const string &/*new_pin*/,
                       Error *error, const ResultCallback &/*callback*/) {
  SLOG(Device, 2) << __func__;
  Error::PopulateAndLog(error, Error::kNotSupported,
                        "Device doesn't support ChangePIN.");
}

void Device::Reset(Error *error, const ResultCallback &/*callback*/) {
  SLOG(Device, 2) << __func__;
  Error::PopulateAndLog(error, Error::kNotSupported,
                        "Device doesn't support Reset.");
}

void Device::SetCarrier(const string &/*carrier*/,
                        Error *error, const ResultCallback &/*callback*/) {
  SLOG(Device, 2) << __func__;
  Error::PopulateAndLog(error, Error::kNotSupported,
                        "Device doesn't support SetCarrier.");
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
  // for IPv6.  crbug.com/207193
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

bool Device::IsConnectedToService(const ServiceRefPtr &service) const {
  return service == selected_service_ && IsConnected();
}

string Device::GetRpcIdentifier() const {
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

vector<GeolocationInfo> Device::GetGeolocationObjects() const {
  return vector<GeolocationInfo>();
};

string Device::GetTechnologyString(Error */*error*/) {
  return Technology::NameFromIdentifier(technology());
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
  enabled_persistent_ = true;
  storage->GetBool(id, kStoragePowered, &enabled_persistent_);
  uint64 rx_byte_count = 0, tx_byte_count = 0;

  manager_->device_info()->GetByteCounts(
      interface_index_, &rx_byte_count, &tx_byte_count);
  // If there is a byte-count present in the profile, the return value
  // of Device::Get*ByteCount() should be the this stored value plus
  // whatever additional bytes we receive since time-of-load.  We
  // accomplish this by the subtractions below, which can validly
  // roll over "negative" in the subtractions below and in Get*ByteCount.
  uint64 profile_byte_count;
  if (storage->GetUint64(id, kStorageReceiveByteCount, &profile_byte_count)) {
    receive_byte_offset_ = rx_byte_count - profile_byte_count;
  }
  if (storage->GetUint64(id, kStorageTransmitByteCount, &profile_byte_count)) {
    transmit_byte_offset_ = tx_byte_count - profile_byte_count;
  }

  return true;
}

bool Device::Save(StoreInterface *storage) {
  const string id = GetStorageIdentifier();
  storage->SetBool(id, kStoragePowered, enabled_persistent_);
  if (ipconfig_.get()) {
    // The _0 is an index into the list of IPConfigs that this device might
    // have.  We only have one IPConfig right now, and I hope to never have
    // to support more, as sleffler indicates that associating IPConfigs
    // with devices is wrong and due to be changed in flimflam anyhow.
    string suffix = hardware_address_ + "_0";
    ipconfig_->Save(storage, suffix);
    storage->SetString(id, kStorageIPConfigs, SerializeIPConfigs(suffix));
  }
  storage->SetUint64(id, kStorageReceiveByteCount, GetReceiveByteCount());
  storage->SetUint64(id, kStorageTransmitByteCount, GetTransmitByteCount());
  return true;
}

void Device::OnBeforeSuspend() {
  // Nothing to be done in the general case.
}

void Device::OnAfterResume() {
  if (ipconfig_) {
    SLOG(Device, 3) << "Renewing IP address on resume.";
    ipconfig_->RenewIP();
  }
  if (link_monitor_) {
    SLOG(Device, 3) << "Informing Link Monitor of resume.";
    link_monitor_->OnAfterResume();
  }
}

void Device::DropConnection() {
  SLOG(Device, 2) << __func__;
  DestroyIPConfig();
  SelectService(NULL);
}

void Device::DestroyIPConfig() {
  DisableIPv6();
  if (ipconfig_.get()) {
    ipconfig_->ReleaseIP(IPConfig::kReleaseReasonDisconnect);
    ipconfig_ = NULL;
  }
  DestroyConnection();
}

bool Device::ShouldUseArpGateway() const {
  return false;
}

bool Device::AcquireIPConfig() {
  return AcquireIPConfigWithLeaseName(string());
}

bool Device::AcquireIPConfigWithLeaseName(const string &lease_name) {
  DestroyIPConfig();
  EnableIPv6();
  bool arp_gateway = manager_->GetArpGateway() && ShouldUseArpGateway();
  ipconfig_ = dhcp_provider_->CreateConfig(link_name_,
                                           manager_->GetHostName(),
                                           lease_name,
                                           arp_gateway);
  ipconfig_->RegisterUpdateCallback(Bind(&Device::OnIPConfigUpdated,
                                         weak_ptr_factory_.GetWeakPtr()));
  dispatcher_->PostTask(Bind(&Device::ConfigureStaticIPTask,
                             weak_ptr_factory_.GetWeakPtr()));
  return ipconfig_->RequestIP();
}

void Device::DestroyIPConfigLease(const string &name) {
  dhcp_provider_->DestroyLease(name);
}

void Device::HelpRegisterConstDerivedString(
    const string &name,
    string(Device::*get)(Error *error)) {
  store_.RegisterDerivedString(
      name,
      StringAccessor(new CustomAccessor<Device, string>(this, get, NULL)));
}

void Device::HelpRegisterConstDerivedRpcIdentifiers(
    const string &name,
    RpcIdentifiers(Device::*get)(Error *)) {
  store_.RegisterDerivedRpcIdentifiers(
      name,
      RpcIdentifiersAccessor(
          new CustomAccessor<Device, RpcIdentifiers>(this, get, NULL)));
}

void Device::HelpRegisterConstDerivedUint64(
    const string &name,
    uint64(Device::*get)(Error *)) {
  store_.RegisterDerivedUint64(
      name,
      Uint64Accessor(
          new CustomAccessor<Device, uint64>(this, get, NULL)));
}

void Device::ConfigureStaticIPTask() {
  SLOG(Device, 2) << __func__ << " selected_service " << selected_service_.get()
                  << " ipconfig " << ipconfig_.get();

  if (!selected_service_ || !ipconfig_) {
    return;
  }

  const StaticIPParameters &static_ip_parameters =
      selected_service_->static_ip_parameters();
  if (static_ip_parameters.ContainsAddress()) {
    SLOG(Device, 2) << __func__ << " " << " configuring static IP parameters.";
    // If the parameters contain an IP address, apply them now and bring
    // the interface up.  When DHCP information arrives, it will supplement
    // the static information.
    OnIPConfigUpdated(ipconfig_, true);
  } else {
    SLOG(Device, 2) << __func__ << " " << " no static IP address.";
  }
}

void Device::OnIPConfigUpdated(const IPConfigRefPtr &ipconfig, bool success) {
  SLOG(Device, 2) << __func__ << " " << " success: " << success;
  if (success) {
    CreateConnection();
    if (selected_service_) {
      ipconfig->ApplyStaticIPParameters(
          selected_service_->mutable_static_ip_parameters());
      if (selected_service_->static_ip_parameters().ContainsAddress()) {
        // If we are using a statically configured IP address instead
        // of a leased IP address, release any acquired lease so it may
        // be used by others.  This allows us to merge other non-leased
        // parameters (like DNS) when they're available from a DHCP server
        // and not overridden by static parameters, but at the same time
        // we avoid taking up a dynamic IP address the DHCP server could
        // assign to someone else who might actually use it.
        ipconfig->ReleaseIP(IPConfig::kReleaseReasonStaticIP);
      }
    }
    connection_->UpdateFromIPConfig(ipconfig);
    // SetConnection must occur after the UpdateFromIPConfig so the
    // service can use the values derived from the connection.
    if (selected_service_) {
      selected_service_->SetConnection(connection_);
    }
    // The service state change needs to happen last, so that at the
    // time we report the state change to the manager, the service
    // has its connection.
    SetServiceState(Service::kStateConnected);
    OnConnected();
    portal_attempts_to_online_ = 0;
    // Subtle: Start portal detection after transitioning the service
    // to the Connected state because this call may immediately transition
    // to the Online state.
    if (selected_service_) {
      StartPortalDetection();
    }
    StartLinkMonitor();
    StartTrafficMonitor();
    SetupConnectionHealthChecker();
  } else {
    // TODO(pstew): This logic gets yet more complex when multiple
    // IPConfig types are run in parallel (e.g. DHCP and DHCP6)
    if (selected_service_ &&
        selected_service_->static_ip_parameters().ContainsAddress()) {
      // Consider three cases:
      //
      // 1. We're here because DHCP failed while starting up. There
      //    are two subcases:
      //    a. DHCP has failed, and Static IP config has _not yet_
      //       completed. It's fine to do nothing, because we'll
      //       apply the static config shortly.
      //    b. DHCP has failed, and Static IP config has _already_
      //       completed. It's fine to do nothing, because we can
      //       continue to use the static config that's already
      //       been applied.
      //
      // 2. We're here because a previously valid DHCP configuration
      //    is no longer valid. There's still a static IP config,
      //    because the condition in the if clause evaluated to true.
      //    Furthermore, the static config includes an IP address for
      //    us to use.
      //
      //    The current configuration may include some DHCP
      //    parameters, overriden by any static parameters
      //    provided. We continue to use this configuration, because
      //    the only configuration element that is leased to us (IP
      //    address) will be overriden by a static parameter.
      return;
    }

    OnIPConfigFailure();
    DestroyConnection();
  }
}

void Device::OnIPConfigFailure() {
  if (selected_service_) {
    Error error;
    selected_service_->DisconnectWithFailure(Service::kFailureDHCP, &error);
  }
}

void Device::OnConnected() {}

void Device::OnConnectionUpdated() {
  if (selected_service_) {
    manager_->UpdateService(selected_service_);
  }
}

void Device::CreateConnection() {
  SLOG(Device, 2) << __func__;
  if (!connection_.get()) {
    connection_ = new Connection(interface_index_,
                                 link_name_,
                                 technology_,
                                 manager_->device_info());
  }
}

void Device::DestroyConnection() {
  SLOG(Device, 2) << __func__ << " on " << link_name_;
  StopPortalDetection();
  StopLinkMonitor();
  StopTrafficMonitor();
  if (selected_service_.get()) {
    SLOG(Device, 3) << "Clearing connection of service "
                    << selected_service_->unique_name();
    selected_service_->SetConnection(NULL);
  }
  connection_ = NULL;
  health_checker_.reset();
}

void Device::SelectService(const ServiceRefPtr &service) {
  SLOG(Device, 2) << __func__ << ": service "
                  << (service ? service->unique_name() : "*reset*")
                  << " on " << link_name_;

  if (selected_service_.get() == service.get()) {
    // No change to |selected_service_|. Return early to avoid
    // changing its state.
    return;
  }

  if (selected_service_.get()) {
    if (selected_service_->state() != Service::kStateFailure) {
      selected_service_->SetState(Service::kStateIdle);
    }
    // Just in case the Device subclass has not already done so, make
    // sure the previously selected service has its connection removed.
    selected_service_->SetConnection(NULL);
    StopLinkMonitor();
    StopTrafficMonitor();
    StopPortalDetection();
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

void Device::SetServiceFailureSilent(Service::ConnectFailure failure_state) {
  if (selected_service_.get()) {
    selected_service_->SetFailureSilent(failure_state);
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
  SLOG(Device, 2) << "Writing " << value << " to flag file "
                  << flag_file.value();
  if (file_util::WriteFile(flag_file, value.c_str(), value.length()) != 1) {
    LOG(ERROR) << StringPrintf("IP flag write failed: %s to %s",
                               value.c_str(), flag_file.value().c_str());
    return false;
  }
  return true;
}

void Device::ResetByteCounters() {
  manager_->device_info()->GetByteCounts(
      interface_index_, &receive_byte_offset_, &transmit_byte_offset_);
  manager_->UpdateDevice(this);
}

void Device::SetupConnectionHealthChecker() {
  DCHECK(connection_);
  if (!health_checker_.get()) {
    health_checker_.reset(new ConnectionHealthChecker(
        connection_,
        dispatcher_,
        manager_->health_checker_remote_ips(),
        Bind(&Device::OnConnectionHealthCheckerResult,
             weak_ptr_factory_.GetWeakPtr())));
  } else {
    health_checker_->SetConnection(connection_);
  }
  // Add URL in either case because a connection reset could have dropped past
  // DNS queries.
  health_checker_->AddRemoteURL(manager_->GetPortalCheckURL());
}

void Device::RequestConnectionHealthCheck() {
  if (!health_checker_.get()) {
    SLOG(Device, 2) << "No health checker exists, cannot request "
                    << "health check.";
    return;
  }
  if (health_checker_->health_check_in_progress()) {
    SLOG(Device, 2) << "Health check already in progress.";
    return;
  }
  health_checker_->Start();
}

void Device::OnConnectionHealthCheckerResult(
    ConnectionHealthChecker::Result result) {
  SLOG(Device, 2)
      << FriendlyName()
      << ": ConnectionHealthChecker result: "
      << ConnectionHealthChecker::ResultToString(result);
}

bool Device::RestartPortalDetection() {
  StopPortalDetection();
  return StartPortalDetection();
}

bool Device::RequestPortalDetection() {
  if (!selected_service_) {
    SLOG(Device, 2) << FriendlyName()
            << ": No selected service, so no need for portal check.";
    return false;
  }

  if (!connection_.get()) {
    SLOG(Device, 2) << FriendlyName()
            << ": No connection, so no need for portal check.";
    return false;
  }

  if (selected_service_->state() != Service::kStatePortal) {
    SLOG(Device, 2) << FriendlyName()
            << ": Service is not in portal state.  No need to start check.";
    return false;
  }

  if (!connection_->is_default()) {
    SLOG(Device, 2) << FriendlyName()
            << ": Service is not the default connection.  Don't start check.";
    return false;
  }

  if (portal_detector_.get() && portal_detector_->IsInProgress()) {
    SLOG(Device, 2) << FriendlyName()
                    << ": Portal detection is already running.";
    return true;
  }

  return StartPortalDetection();
}

bool Device::StartPortalDetection() {
  DCHECK(selected_service_);
  if (selected_service_->IsPortalDetectionDisabled()) {
    SLOG(Device, 2) << "Service " << selected_service_->unique_name()
                    << ": Portal detection is disabled; "
                    << "marking service online.";
    SetServiceConnectedState(Service::kStateOnline);
    return false;
  }

  if (selected_service_->IsPortalDetectionAuto() &&
      !manager_->IsPortalDetectionEnabled(technology())) {
    // If portal detection is disabled for this technology, immediately set
    // the service state to "Online".
    SLOG(Device, 2) << "Device " << FriendlyName()
                    << ": Portal detection is disabled; "
                    << "marking service online.";
    SetServiceConnectedState(Service::kStateOnline);
    return false;
  }

  if (selected_service_->HasProxyConfig()) {
    // Services with HTTP proxy configurations should not be checked by the
    // connection manager, since we don't have the ability to evaluate
    // arbitrary proxy configs and their possible credentials.
    SLOG(Device, 2) << "Device " << FriendlyName()
                    << ": Service has proxy config; marking it online.";
    SetServiceConnectedState(Service::kStateOnline);
    return false;
  }

  portal_detector_.reset(new PortalDetector(connection_,
                                            dispatcher_,
                                            portal_detector_callback_));
  if (!portal_detector_->Start(manager_->GetPortalCheckURL())) {
    LOG(ERROR) << "Device " << FriendlyName()
               << ": Portal detection failed to start: likely bad URL: "
               << manager_->GetPortalCheckURL();
    SetServiceConnectedState(Service::kStateOnline);
    return false;
  }

  SLOG(Device, 2) << "Device " << FriendlyName()
                  << ": Portal detection has started.";
  return true;
}

void Device::StopPortalDetection() {
  SLOG(Device, 2) << "Device " << FriendlyName()
                  << ": Portal detection stopping.";
  portal_detector_.reset();
}

void Device::set_link_monitor(LinkMonitor *link_monitor) {
  link_monitor_.reset(link_monitor);
}

bool Device::StartLinkMonitor() {
  if (!manager_->IsTechnologyLinkMonitorEnabled(technology())) {
    SLOG(Device, 2) << "Device " << FriendlyName()
                    << ": Link Monitoring is disabled.";
    return false;
  }

  if (!link_monitor()) {
    set_link_monitor(
      new LinkMonitor(
          connection_, dispatcher_, metrics(), manager_->device_info(),
          Bind(&Device::OnLinkMonitorFailure, weak_ptr_factory_.GetWeakPtr())));
  }

  SLOG(Device, 2) << "Device " << FriendlyName()
                  << ": Link Monitor starting.";
  return link_monitor_->Start();
}

void Device::StopLinkMonitor() {
  SLOG(Device, 2) << "Device " << FriendlyName()
                  << ": Link Monitor stopping.";
  link_monitor_.reset();
}

void Device::OnLinkMonitorFailure() {
  LOG(ERROR) << "Device " << FriendlyName()
             << ": Link Monitor indicates failure.";
}

void Device::set_traffic_monitor(TrafficMonitor *traffic_monitor) {
  traffic_monitor_.reset(traffic_monitor);
}

bool Device::StartTrafficMonitor() {
  SLOG(Device, 2) << __func__;
  if (!traffic_monitor_enabled_) {
    SLOG(Device, 2) << "Device " << FriendlyName()
                    << ": Traffic Monitoring is disabled.";
    return false;
  }

  if (!traffic_monitor_.get()) {
    traffic_monitor_.reset(new TrafficMonitor(this, dispatcher_));
    traffic_monitor_->set_tcp_out_traffic_not_routed_callback(
        Bind(&Device::OnNoNetworkRouting, weak_ptr_factory_.GetWeakPtr()));
  }

  SLOG(Device, 2) << "Device " << FriendlyName()
                  << ": Traffic Monitor starting.";
  traffic_monitor_->Start();
  return true;
}

void Device::StopTrafficMonitor() {
  SLOG(Device, 2) << __func__;
  SLOG(Device, 2) << "Device " << FriendlyName()
                  << ": Traffic Monitor stopping.";
  traffic_monitor_.reset();
}

void Device::OnNoNetworkRouting() {
  SLOG(Device, 2) << "Device " << FriendlyName()
                  << ": Traffic Monitor detects network congestion.";
}

void Device::SetServiceConnectedState(Service::ConnectState state) {
  DCHECK(selected_service_.get());

  if (!selected_service_.get()) {
    LOG(ERROR) << FriendlyName() << ": "
               << "Portal detection completed but no selected service exists!";
    return;
  }

  if (!selected_service_->IsConnected()) {
    LOG(ERROR) << FriendlyName() << ": "
               << "Portal detection completed but selected service "
               << selected_service_->unique_name()
               << " is in non-connected state.";
    return;
  }

  if (state == Service::kStatePortal && connection_->is_default() &&
      manager_->GetPortalCheckInterval() != 0) {
    CHECK(portal_detector_.get());
    if (!portal_detector_->StartAfterDelay(
            manager_->GetPortalCheckURL(),
            manager_->GetPortalCheckInterval())) {
      LOG(ERROR) << "Device " << FriendlyName()
                 << ": Portal detection failed to restart: likely bad URL: "
                 << manager_->GetPortalCheckURL();
      SetServiceState(Service::kStateOnline);
      portal_detector_.reset();
      return;
    }
    SLOG(Device, 2) << "Device " << FriendlyName()
                    << ": Portal detection retrying.";
  } else {
    SLOG(Device, 2) << "Device " << FriendlyName()
                    << ": Portal will not retry.";
    portal_detector_.reset();
  }

  SetServiceState(state);
}

void Device::PortalDetectorCallback(const PortalDetector::Result &result) {
  if (!result.final) {
    SLOG(Device, 2) << "Device " << FriendlyName()
                    << ": Received non-final status: "
                    << PortalDetector::StatusToString(result.status);
    return;
  }

  SLOG(Device, 2) << "Device " << FriendlyName()
                  << ": Received final status: "
                  << PortalDetector::StatusToString(result.status);

  portal_attempts_to_online_ += result.num_attempts;

  metrics()->SendEnumToUMA(
      metrics()->GetFullMetricName(Metrics::kMetricPortalResult, technology()),
      Metrics::PortalDetectionResultToEnum(result),
      Metrics::kPortalResultMax);

  if (result.status == PortalDetector::kStatusSuccess) {
    SetServiceConnectedState(Service::kStateOnline);

    metrics()->SendToUMA(
        metrics()->GetFullMetricName(
            Metrics::kMetricPortalAttemptsToOnline, technology()),
        portal_attempts_to_online_,
        Metrics::kMetricPortalAttemptsToOnlineMin,
        Metrics::kMetricPortalAttemptsToOnlineMax,
        Metrics::kMetricPortalAttemptsToOnlineNumBuckets);
  } else {
    SetServiceConnectedState(Service::kStatePortal);

    metrics()->SendToUMA(
        metrics()->GetFullMetricName(
            Metrics::kMetricPortalAttempts, technology()),
        result.num_attempts,
        Metrics::kMetricPortalAttemptsMin,
        Metrics::kMetricPortalAttemptsMax,
        Metrics::kMetricPortalAttemptsNumBuckets);
  }
}

vector<string> Device::AvailableIPConfigs(Error */*error*/) {
  if (ipconfig_.get()) {
    string id = ipconfig_->GetRpcIdentifier();
    return vector<string>(1, id);
  }
  return vector<string>();
}

string Device::GetRpcConnectionIdentifier() {
  return adaptor_->GetRpcConnectionIdentifier();
}

uint64 Device::GetLinkMonitorResponseTime(Error *error) {
  if (!link_monitor_.get()) {
    // It is not strictly an error that the link monitor does not
    // exist, but returning an error here allows the GetProperties
    // call in our Adaptor to omit this parameter.
    error->Populate(Error::kNotFound, "Device is not running LinkMonitor");
    return 0;
  }
  return link_monitor_->GetResponseTimeMilliseconds();
}

uint64 Device::GetReceiveByteCount() {
  uint64 rx_byte_count = 0, tx_byte_count = 0;
  manager_->device_info()->GetByteCounts(
      interface_index_, &rx_byte_count, &tx_byte_count);
  return rx_byte_count - receive_byte_offset_;
}

uint64 Device::GetTransmitByteCount() {
  uint64 rx_byte_count = 0, tx_byte_count = 0;
  manager_->device_info()->GetByteCounts(
      interface_index_, &rx_byte_count, &tx_byte_count);
  return tx_byte_count - transmit_byte_offset_;
}

uint64 Device::GetReceiveByteCountProperty(Error */*error*/) {
  return GetReceiveByteCount();
}

uint64 Device::GetTransmitByteCountProperty(Error */*error*/) {
  return GetTransmitByteCount();
}

bool Device::IsUnderlyingDeviceEnabled() const {
  return false;
}

// callback
void Device::OnEnabledStateChanged(const ResultCallback &callback,
                                   const Error &error) {
  SLOG(Device, 2) << __func__
                  << " (target: " << enabled_pending_ << ","
                  << " success: " << error.IsSuccess() << ")"
                  << " on " << link_name_;
  if (error.IsSuccess()) {
    enabled_ = enabled_pending_;
    manager_->UpdateEnabledTechnologies();
    adaptor_->EmitBoolChanged(flimflam::kPoweredProperty, enabled_);
  }
  enabled_pending_ = enabled_;
  if (!callback.is_null())
    callback.Run(error);
}

void Device::SetEnabled(bool enable) {
  SLOG(Device, 2) << __func__ << "(" << enable << ")";
  Error error;
  SetEnabledInternal(enable, false, &error, ResultCallback());

  // SetEnabledInternal might fail here if there is an unfinished enable or
  // disable operation. Don't log error in this case, as this method is only
  // called when the underlying device is already in the target state and the
  // pending operation should eventually bring the device to the expected
  // state.
  LOG_IF(ERROR,
         error.IsFailure() &&
         !error.IsOngoing() &&
         error.type() != Error::kInProgress)
      << "Enabled failed, but no way to report the failure.";
}

void Device::SetEnabledPersistent(bool enable,
                                  Error *error,
                                  const ResultCallback &callback) {
  SetEnabledInternal(enable, true, error, callback);
}

void Device::SetEnabledInternal(bool enable,
                                bool persist,
                                Error *error,
                                const ResultCallback &callback) {
  DCHECK(error);
  SLOG(Device, 2) << "Device " << link_name_ << " "
                  << (enable ? "starting" : "stopping");
  if (enable == enabled_) {
    error->Reset();
    return;
  }

  if (enabled_pending_ == enable) {
    Error::PopulateAndLog(error, Error::kInProgress,
                          "Enable operation already in progress");
    return;
  }

  if (persist) {
    enabled_persistent_ = enable;
    manager_->UpdateDevice(this);
  }

  enabled_pending_ = enable;
  EnabledStateChangedCallback enabled_callback =
      Bind(&Device::OnEnabledStateChanged,
           weak_ptr_factory_.GetWeakPtr(), callback);
  if (enable) {
    running_ = true;
    Start(error, enabled_callback);
  } else {
    running_ = false;
    DestroyIPConfig();         // breaks a reference cycle
    SelectService(NULL);       // breaks a reference cycle
    rtnl_handler_->SetInterfaceFlags(interface_index(), 0, IFF_UP);
    SLOG(Device, 3) << "Device " << link_name_ << " ipconfig_ "
                    << (ipconfig_ ? "is set." : "is not set.");
    SLOG(Device, 3) << "Device " << link_name_ << " connection_ "
                    << (connection_ ? "is set." : "is not set.");
    SLOG(Device, 3) << "Device " << link_name_ << " selected_service_ "
                    << (selected_service_ ? "is set." : "is not set.");
    Stop(error, enabled_callback);
  }
}

}  // namespace shill
