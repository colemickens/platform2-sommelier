// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/wifi.h"

#include <stdio.h>
#include <string.h>
#include <netinet/ether.h>
#include <linux/if.h>  // Needs definitions from netinet/ether.h

#include <algorithm>
#include <map>
#include <string>
#include <vector>

#include <base/bind.h>
#include <base/stringprintf.h>
#include <base/string_util.h>
#include <chromeos/dbus/service_constants.h>
#include <glib.h>

#include "shill/control_interface.h"
#include "shill/dbus_adaptor.h"
#include "shill/device.h"
#include "shill/error.h"
#include "shill/geolocation_info.h"
#include "shill/ieee80211.h"
#include "shill/link_monitor.h"
#include "shill/logging.h"
#include "shill/manager.h"
#include "shill/metrics.h"
#include "shill/netlink_manager.h"
#include "shill/nl80211_message.h"
#include "shill/property_accessor.h"
#include "shill/proxy_factory.h"
#include "shill/rtnl_handler.h"
#include "shill/scope_logger.h"
#include "shill/shill_time.h"
#include "shill/supplicant_eap_state_handler.h"
#include "shill/supplicant_interface_proxy_interface.h"
#include "shill/supplicant_network_proxy_interface.h"
#include "shill/supplicant_process_proxy_interface.h"
#include "shill/technology.h"
#include "shill/wifi_endpoint.h"
#include "shill/wifi_provider.h"
#include "shill/wifi_service.h"
#include "shill/wpa_supplicant.h"

using base::Bind;
using base::StringPrintf;
using std::map;
using std::string;
using std::vector;

namespace shill {

// statics
const char *WiFi::kDefaultBgscanMethod =
    WPASupplicant::kNetworkBgscanMethodSimple;
const uint16 WiFi::kDefaultBgscanShortIntervalSeconds = 30;
const int32 WiFi::kDefaultBgscanSignalThresholdDbm = -50;
const uint16 WiFi::kDefaultScanIntervalSeconds = 180;
// Scan interval while connected.
const uint16 WiFi::kBackgroundScanIntervalSeconds = 3601;
// Age (in seconds) beyond which a BSS cache entry will not be preserved,
// across a suspend/resume.
const time_t WiFi::kMaxBSSResumeAgeSeconds = 10;
const char WiFi::kInterfaceStateUnknown[] = "shill-unknown";
const time_t WiFi::kRescanIntervalSeconds = 1;
const int WiFi::kNumFastScanAttempts = 3;
const int WiFi::kFastScanIntervalSeconds = 10;
const int WiFi::kPendingTimeoutSeconds = 15;
const int WiFi::kReconnectTimeoutSeconds = 10;

WiFi::WiFi(ControlInterface *control_interface,
           EventDispatcher *dispatcher,
           Metrics *metrics,
           Manager *manager,
           const string& link,
           const string &address,
           int interface_index)
    : Device(control_interface,
             dispatcher,
             metrics,
             manager,
             link,
             address,
             interface_index,
             Technology::kWifi),
      provider_(manager->wifi_provider()),
      weak_ptr_factory_(this),
      proxy_factory_(ProxyFactory::GetInstance()),
      time_(Time::GetInstance()),
      supplicant_present_(false),
      supplicant_state_(kInterfaceStateUnknown),
      supplicant_bss_("(unknown)"),
      need_bss_flush_(false),
      resumed_at_((struct timeval) {0}),
      fast_scans_remaining_(kNumFastScanAttempts),
      has_already_completed_(false),
      is_debugging_connection_(false),
      eap_state_handler_(new SupplicantEAPStateHandler()),
      bgscan_short_interval_seconds_(kDefaultBgscanShortIntervalSeconds),
      bgscan_signal_threshold_dbm_(kDefaultBgscanSignalThresholdDbm),
      scan_pending_(false),
      scan_interval_seconds_(kDefaultScanIntervalSeconds),
      netlink_manager_(NetlinkManager::GetInstance()) {
  PropertyStore *store = this->mutable_store();
  store->RegisterDerivedString(
      flimflam::kBgscanMethodProperty,
      StringAccessor(
          // TODO(petkov): CustomMappedAccessor is used for convenience because
          // it provides a way to define a custom clearer (unlike
          // CustomAccessor). We need to implement a fully custom accessor with
          // no extra argument.
          new CustomMappedAccessor<WiFi, string, int>(this,
                                                      &WiFi::ClearBgscanMethod,
                                                      &WiFi::GetBgscanMethod,
                                                      &WiFi::SetBgscanMethod,
                                                      0)));  // Unused.
  HelpRegisterDerivedUint16(store,
                            flimflam::kBgscanShortIntervalProperty,
                            &WiFi::GetBgscanShortInterval,
                            &WiFi::SetBgscanShortInterval);
  HelpRegisterDerivedInt32(store,
                           flimflam::kBgscanSignalThresholdProperty,
                           &WiFi::GetBgscanSignalThreshold,
                           &WiFi::SetBgscanSignalThreshold);

  // TODO(quiche): Decide if scan_pending_ is close enough to
  // "currently scanning" that we don't care, or if we want to track
  // scan pending/currently scanning/no scan scheduled as a tri-state
  // kind of thing.
  store->RegisterConstBool(flimflam::kScanningProperty, &scan_pending_);
  HelpRegisterDerivedUint16(store,
                            flimflam::kScanIntervalProperty,
                            &WiFi::GetScanInterval,
                            &WiFi::SetScanInterval);
  ScopeLogger::GetInstance()->RegisterScopeEnableChangedCallback(
      ScopeLogger::kWiFi,
      Bind(&WiFi::OnWiFiDebugScopeChanged, weak_ptr_factory_.GetWeakPtr()));
  CHECK(netlink_manager_);
  SLOG(WiFi, 2) << "WiFi device " << link_name() << " initialized.";
}

WiFi::~WiFi() {}

void WiFi::Start(Error *error, const EnabledStateChangedCallback &callback) {
  SLOG(WiFi, 2) << "WiFi " << link_name() << " starting.";
  if (enabled()) {
    return;
  }
  OnEnabledStateChanged(EnabledStateChangedCallback(), Error());
  if (error) {
    error->Reset();       // indicate immediate completion
  }
  if (on_supplicant_appear_.IsCancelled()) {
    // Registers the WPA supplicant appear/vanish callbacks only once per WiFi
    // device instance.
    on_supplicant_appear_.Reset(
        Bind(&WiFi::OnSupplicantAppear, Unretained(this)));
    on_supplicant_vanish_.Reset(
        Bind(&WiFi::OnSupplicantVanish, Unretained(this)));
    manager()->dbus_manager()->WatchName(WPASupplicant::kDBusAddr,
                                         on_supplicant_appear_.callback(),
                                         on_supplicant_vanish_.callback());
  }
  // Connect to WPA supplicant if it's already present. If not, we'll connect to
  // it when it appears.
  ConnectToSupplicant();
  // Subscribe to multicast events.
  netlink_manager_->SubscribeToEvents(Nl80211Message::kMessageTypeString,
                                      NetlinkManager::kEventTypeConfig);
  netlink_manager_->SubscribeToEvents(Nl80211Message::kMessageTypeString,
                                      NetlinkManager::kEventTypeScan);
  netlink_manager_->SubscribeToEvents(Nl80211Message::kMessageTypeString,
                                      NetlinkManager::kEventTypeRegulatory);
  netlink_manager_->SubscribeToEvents(Nl80211Message::kMessageTypeString,
                                      NetlinkManager::kEventTypeMlme);
}

void WiFi::Stop(Error *error, const EnabledStateChangedCallback &callback) {
  SLOG(WiFi, 2) << "WiFi " << link_name() << " stopping.";
  DropConnection();
  StopScanTimer();
  for (EndpointMap::iterator it = endpoint_by_rpcid_.begin();
       it != endpoint_by_rpcid_.end(); ++it) {
    provider_->OnEndpointRemoved(it->second);
  }
  endpoint_by_rpcid_.clear();
  for (ReverseServiceMap::const_iterator it = rpcid_by_service_.begin();
       it != rpcid_by_service_.end(); ++it) {
    RemoveNetwork(it->second);
  }
  rpcid_by_service_.clear();
  supplicant_interface_proxy_.reset();  // breaks a reference cycle
  // TODO(quiche): Remove interface from supplicant.
  supplicant_process_proxy_.reset();
  current_service_ = NULL;            // breaks a reference cycle
  pending_service_ = NULL;            // breaks a reference cycle
  is_debugging_connection_ = false;
  SetScanPending(false);
  StopPendingTimer();
  StopReconnectTimer();

  OnEnabledStateChanged(EnabledStateChangedCallback(), Error());
  if (error)
    error->Reset();       // indicate immediate completion
  weak_ptr_factory_.InvalidateWeakPtrs();

  SLOG(WiFi, 3) << "WiFi " << link_name() << " supplicant_process_proxy_ "
                << (supplicant_process_proxy_.get() ?
                    "is set." : "is not set.");
  SLOG(WiFi, 3) << "WiFi " << link_name() << " supplicant_interface_proxy_ "
                << (supplicant_interface_proxy_.get() ?
                    "is set." : "is not set.");
  SLOG(WiFi, 3) << "WiFi " << link_name() << " pending_service_ "
                << (pending_service_.get() ? "is set." : "is not set.");
  SLOG(WiFi, 3) << "WiFi " << link_name() << " has "
                << endpoint_by_rpcid_.size() << " EndpointMap entries.";
}

void WiFi::Scan(ScanType scan_type, Error */*error*/) {
  LOG(INFO) << __func__;

  if (scan_type == kProgressiveScan)
    metrics()->NotifyDeviceScanStarted(interface_index());

  // Needs to send a D-Bus message, but may be called from D-Bus
  // signal handler context (via Manager::RequestScan). So defer work
  // to event loop.
  dispatcher()->PostTask(Bind(&WiFi::ScanTask, weak_ptr_factory_.GetWeakPtr()));
}

void WiFi::BSSAdded(const ::DBus::Path &path,
                    const map<string, ::DBus::Variant> &properties) {
  // Called from a D-Bus signal handler, and may need to send a D-Bus
  // message. So defer work to event loop.
  dispatcher()->PostTask(Bind(&WiFi::BSSAddedTask,
                              weak_ptr_factory_.GetWeakPtr(),
                              path, properties));
}

void WiFi::BSSRemoved(const ::DBus::Path &path) {
  // Called from a D-Bus signal handler, and may need to send a D-Bus
  // message. So defer work to event loop.
  dispatcher()->PostTask(Bind(&WiFi::BSSRemovedTask,
                              weak_ptr_factory_.GetWeakPtr(), path));
}

void WiFi::Certification(const map<string, ::DBus::Variant> &properties) {
  dispatcher()->PostTask(Bind(&WiFi::CertificationTask,
                              weak_ptr_factory_.GetWeakPtr(), properties));
}

void WiFi::EAPEvent(const string &status, const string &parameter) {
  dispatcher()->PostTask(Bind(&WiFi::EAPEventTask,
                              weak_ptr_factory_.GetWeakPtr(),
                              status,
                              parameter));
}

void WiFi::PropertiesChanged(const map<string, ::DBus::Variant> &properties) {
  SLOG(WiFi, 2) << __func__;
  // Called from D-Bus signal handler, but may need to send a D-Bus
  // message. So defer work to event loop.
  dispatcher()->PostTask(Bind(&WiFi::PropertiesChangedTask,
                              weak_ptr_factory_.GetWeakPtr(), properties));
}

void WiFi::ScanDone() {
  LOG(INFO) << __func__;

  // Defer handling of scan result processing, because that processing
  // may require the the registration of new D-Bus objects. And such
  // registration can't be done in the context of a D-Bus signal
  // handler.
  metrics()->NotifyDeviceScanFinished(interface_index());
  dispatcher()->PostTask(Bind(&WiFi::ScanDoneTask,
                              weak_ptr_factory_.GetWeakPtr()));
}

void WiFi::ConnectTo(WiFiService *service,
                     map<string, DBus::Variant> service_params) {
  CHECK(service) << "Can't connect to NULL service.";
  DBus::Path network_path;

  // TODO(quiche): Handle cases where already connected.
  if (pending_service_ && pending_service_ == service) {
    // TODO(quiche): Return an error to the caller. crosbug.com/23832
    LOG(INFO) << "WiFi " << link_name() << " ignoring ConnectTo service "
              << service->unique_name()
              << ", which is already pending.";
    return;
  }

  if (pending_service_ && pending_service_ != service) {
    DisconnectFrom(pending_service_);
  }

  Error unused_error;
  network_path = FindNetworkRpcidForService(service, &unused_error);
  if (network_path.empty()) {
    try {
      const uint32_t scan_ssid = 1;  // "True": Use directed probe.
      service_params[WPASupplicant::kNetworkPropertyScanSSID].writer().
          append_uint32(scan_ssid);
      AppendBgscan(service, &service_params);
      network_path = supplicant_interface_proxy_->AddNetwork(service_params);
      CHECK(!network_path.empty());  // No DBus path should be empty.
      rpcid_by_service_[service] = network_path;
    } catch (const DBus::Error &e) {  // NOLINT
      LOG(ERROR) << "exception while adding network: " << e.what();
      return;
    }
  }

  if (service->HasRecentConnectionIssues()) {
    SetConnectionDebugging(true);
  }
  supplicant_interface_proxy_->SelectNetwork(network_path);
  SetPendingService(service);
  CHECK(current_service_.get() != pending_service_.get());

  // SelectService here (instead of in LinkEvent, like Ethernet), so
  // that, if we fail to bring up L2, we can attribute failure correctly.
  //
  // TODO(quiche): When we add code for dealing with connection failures,
  // reconsider if this is the right place to change the selected service.
  // see discussion in crosbug.com/20191.
  SelectService(service);
}

void WiFi::DisconnectFrom(WiFiService *service) {
  if (service != current_service_ &&  service != pending_service_) {
    // TODO(quiche): Once we have asynchronous reply support, we should
    // generate a D-Bus error here. (crosbug.com/23832)
    LOG(WARNING) << "In " << __func__ << "(): "
                 << " ignoring request to disconnect from service "
                 << service->unique_name()
                 << " which is neither current nor pending";
    return;
  }

  if (pending_service_ && service != pending_service_) {
    // TODO(quiche): Once we have asynchronous reply support, we should
    // generate a D-Bus error here. (crosbug.com/23832)
    LOG(WARNING) << "In " << __func__ << "(): "
                 << " ignoring request to disconnect from service "
                 << service->unique_name()
                 << " which is not the pending service.";
    return;
  }

  if (!pending_service_ && service != current_service_) {
    // TODO(quiche): Once we have asynchronous reply support, we should
    // generate a D-Bus error here. (crosbug.com/23832)
    LOG(WARNING) << "In " << __func__ << "(): "
                 << " ignoring request to disconnect from service "
                 << service->unique_name()
                 << " which is not the current service.";
    return;
  }

  if (pending_service_) {
    // Since wpa_supplicant has not yet set CurrentBSS, we can't depend
    // on this to drive the service state back to idle.  Do that here.
    pending_service_->SetState(Service::kStateIdle);
  }

  SetPendingService(NULL);
  StopReconnectTimer();

  if (!supplicant_present_) {
    LOG(ERROR) << "In " << __func__ << "(): "
               << "wpa_supplicant is not present; silently resetting "
               << "current_service_.";
    if (current_service_ == selected_service()) {
      DropConnection();
    }
    current_service_ = NULL;
    return;
  }

  try {
    supplicant_interface_proxy_->Disconnect();
    // We'll call RemoveNetwork and reset |current_service_| after
    // supplicant notifies us that the CurrentBSS has changed.
  } catch (const DBus::Error &e) {  // NOLINT
    // Can't depend on getting a notification of CurrentBSS change.
    // So effect changes immediately.  For instance, this can happen when
    // a disconnect is triggered by a BSS going away.
    Error unused_error;
    RemoveNetworkForService(service, &unused_error);
    if (service == selected_service()) {
      DropConnection();
    }
    current_service_ = NULL;
  }

  CHECK(current_service_ == NULL ||
        current_service_.get() != pending_service_.get());
}

bool WiFi::DisableNetwork(const ::DBus::Path &network) {
  scoped_ptr<SupplicantNetworkProxyInterface> supplicant_network_proxy(
      proxy_factory_->CreateSupplicantNetworkProxy(
          network, WPASupplicant::kDBusAddr));
  try {
    supplicant_network_proxy->SetEnabled(false);
  } catch (const DBus::Error &e) {  // NOLINT
    LOG(ERROR) << "DisableNetwork for " << network << " failed.";
    return false;
  }
  return true;
}

bool WiFi::RemoveNetwork(const ::DBus::Path &network) {
  try {
    supplicant_interface_proxy_->RemoveNetwork(network);
  } catch (const DBus::Error &e) {  // NOLINT
    // RemoveNetwork can fail with three different errors.
    //
    // If RemoveNetwork fails with a NetworkUnknown error, supplicant has
    // already removed the network object, so return true as if
    // RemoveNetwork removes the network object successfully.
    //
    // As shill always passes a valid network object path, RemoveNetwork
    // should not fail with an InvalidArgs error. Return false in such case
    // as something weird may have happened. Similarly, return false in case
    // of an UnknownError.
    if (strcmp(e.name(), WPASupplicant::kErrorNetworkUnknown) != 0) {
      return false;
    }
  }
  return true;
}

bool WiFi::IsIdle() const {
  return !current_service_ && !pending_service_;
}

void WiFi::ClearCachedCredentials(const WiFiService *service) {
  Error unused_error;
  RemoveNetworkForService(service, &unused_error);
}

void WiFi::NotifyEndpointChanged(const WiFiEndpointConstRefPtr &endpoint) {
  provider_->OnEndpointUpdated(endpoint);
}

void WiFi::AppendBgscan(WiFiService *service,
                        map<string, DBus::Variant> *service_params) const {
  int scan_interval = kBackgroundScanIntervalSeconds;
  string method = bgscan_method_;
  if (method.empty()) {
    // If multiple APs are detected for this SSID, configure the default method.
    // Otherwise, disable background scanning completely.
    if (service->GetEndpointCount() > 1) {
      method = kDefaultBgscanMethod;
    } else {
      LOG(INFO) << "Background scan disabled -- single Endpoint for Service.";
      return;
    }
  } else if (method.compare(WPASupplicant::kNetworkBgscanMethodNone) == 0) {
      LOG(INFO) << "Background scan disabled -- chose None method.";
      return;
  } else {
    // If the background scan method was explicitly specified, honor the
    // configured background scan interval.
    scan_interval = scan_interval_seconds_;
  }
  DCHECK(!method.empty());
  string config_string = StringPrintf("%s:%d:%d:%d",
                                      method.c_str(),
                                      bgscan_short_interval_seconds_,
                                      bgscan_signal_threshold_dbm_,
                                      scan_interval);
  LOG(INFO) << "Background scan: " << config_string;
  (*service_params)[WPASupplicant::kNetworkPropertyBgscan].writer()
      .append_string(config_string.c_str());
}

string WiFi::GetBgscanMethod(const int &/*argument*/, Error */* error */) {
  return bgscan_method_.empty() ? kDefaultBgscanMethod : bgscan_method_;
}

void WiFi::SetBgscanMethod(
    const int &/*argument*/, const string &method, Error *error) {
  if (method != WPASupplicant::kNetworkBgscanMethodSimple &&
      method != WPASupplicant::kNetworkBgscanMethodLearn &&
      method != WPASupplicant::kNetworkBgscanMethodNone) {
    const string error_message =
        StringPrintf("Unrecognized bgscan method %s", method.c_str());
    LOG(WARNING) << error_message;
    error->Populate(Error::kInvalidArguments, error_message);
    return;
  }

  bgscan_method_ = method;
  // We do not update kNetworkPropertyBgscan for |pending_service_| or
  // |current_service_|, because supplicant does not allow for
  // reconfiguration without disconnect and reconnect.
}

void WiFi::SetBgscanShortInterval(const uint16 &seconds, Error */*error*/) {
  bgscan_short_interval_seconds_ = seconds;
  // We do not update kNetworkPropertyBgscan for |pending_service_| or
  // |current_service_|, because supplicant does not allow for
  // reconfiguration without disconnect and reconnect.
}

void WiFi::SetBgscanSignalThreshold(const int32 &dbm, Error */*error*/) {
  bgscan_signal_threshold_dbm_ = dbm;
  // We do not update kNetworkPropertyBgscan for |pending_service_| or
  // |current_service_|, because supplicant does not allow for
  // reconfiguration without disconnect and reconnect.
}

void WiFi::SetScanInterval(const uint16 &seconds, Error */*error*/) {
  scan_interval_seconds_ = seconds;
  if (running()) {
    StartScanTimer();
  }
  // The scan interval affects both foreground scans (handled by
  // |scan_timer_callback_|), and background scans (handled by
  // supplicant). However, we do not update |pending_service_| or
  // |current_service_|, because supplicant does not allow for
  // reconfiguration without disconnect and reconnect.
}

void WiFi::ClearBgscanMethod(const int &/*argument*/, Error */*error*/) {
  bgscan_method_.clear();
}

void WiFi::CurrentBSSChanged(const ::DBus::Path &new_bss) {
  SLOG(WiFi, 3) << "WiFi " << link_name() << " CurrentBSS "
                << supplicant_bss_ << " -> " << new_bss;
  supplicant_bss_ = new_bss;
  has_already_completed_ = false;

  // Any change in CurrentBSS means supplicant is actively changing our
  // connectivity.  We no longer need to track any previously pending
  // reconnect.
  StopReconnectTimer();

  if (new_bss == WPASupplicant::kCurrentBSSNull) {
    HandleDisconnect();
    if (!provider_->GetHiddenSSIDList().empty()) {
      // Before disconnecting, wpa_supplicant probably scanned for
      // APs. So, in the normal case, we defer to the timer for the next scan.
      //
      // However, in the case of hidden SSIDs, supplicant knows about
      // at most one of them. (That would be the hidden SSID we were
      // connected to, if applicable.)
      //
      // So, in this case, we initiate an immediate scan. This scan
      // will include the hidden SSIDs we know about (up to the limit of
      // kScanMAxSSIDsPerScan).
      //
      // We may want to reconsider this immediate scan, if/when shill
      // takes greater responsibility for scanning (vs. letting
      // supplicant handle most of it).
      Scan(kProgressiveScan, NULL);
    }
  } else {
    HandleRoam(new_bss);
  }

  // Reset the EAP handler only after calling HandleDisconnect() above
  // so our EAP state could be used to detect a failed authentication.
  eap_state_handler_->Reset();

  // If we are selecting a new service, or if we're clearing selection
  // of a something other than the pending service, call SelectService.
  // Otherwise skip SelectService, since this will cause the pending
  // service to be marked as Idle.
  if (current_service_ || selected_service() != pending_service_) {
    SelectService(current_service_);
  }

  // Invariant check: a Service can either be current, or pending, but
  // not both.
  CHECK(current_service_.get() != pending_service_.get() ||
        current_service_.get() == NULL);

  // If we are no longer debugging a problematic WiFi connection, return
  // to the debugging level indicated by the WiFi debugging scope.
  if ((!current_service_ || !current_service_->HasRecentConnectionIssues()) &&
      (!pending_service_ || !pending_service_->HasRecentConnectionIssues())) {
    SetConnectionDebugging(false);
  }
}

void WiFi::HandleDisconnect() {
  // Identify the affected service. We expect to get a disconnect
  // event when we fall off a Service that we were connected
  // to. However, we also allow for the case where we get a disconnect
  // event while attempting to connect from a disconnected state.
  WiFiService *affected_service =
      current_service_.get() ? current_service_.get() : pending_service_.get();

  current_service_ = NULL;
  if (!affected_service) {
    SLOG(WiFi, 2) << "WiFi " << link_name()
                  << " disconnected while not connected or connecting";
    return;
  }
  if (affected_service == selected_service()) {
    // If our selected service has disconnected, destroy IP configuration state.
    DropConnection();
  }

  Error error;
  if (!DisableNetworkForService(affected_service, &error)) {
    if (error.type() == Error::kNotFound) {
      SLOG(WiFi, 2) << "WiFi " << link_name() << " disconnected from "
                    << " (or failed to connect to) service "
                    << affected_service->unique_name() << ", "
                    << "but could not find supplicant network to disable.";
    } else {
      LOG(FATAL) << "DisableNetwork failed.";
    }
  }

  SLOG(WiFi, 2) << "WiFi " << link_name() << " disconnected from "
                << " (or failed to connect to) service "
                << affected_service->unique_name();
  Service::ConnectFailure failure;
  if (SuspectCredentials(*affected_service, &failure)) {
    // If we suspect bad credentials, set failure, to trigger an error
    // mole in Chrome.
    affected_service->SetFailure(failure);
    LOG(ERROR) << "Connection failure is due to suspect credentials: returning "
               << Service::ConnectFailureToString(failure);
  } else {
    affected_service->SetFailureSilent(Service::kFailureUnknown);
  }
  affected_service->NotifyCurrentEndpoint(NULL);
  metrics()->NotifyServiceDisconnect(affected_service);

  if (affected_service == pending_service_.get()) {
    // The attempt to connect to |pending_service_| failed. Clear
    // |pending_service_|, to indicate we're no longer in the middle
    // of a connect request.
    SetPendingService(NULL);
  } else if (pending_service_.get()) {
    // We've attributed the disconnection to what was the
    // |current_service_|, rather than the |pending_service_|.
    //
    // If we're wrong about that (i.e. supplicant reported this
    // CurrentBSS change after attempting to connect to
    // |pending_service_|), we're depending on supplicant to retry
    // connecting to |pending_service_|, and delivering another
    // CurrentBSS change signal in the future.
    //
    // Log this fact, to help us debug (in case our assumptions are
    // wrong).
    SLOG(WiFi, 2) << "WiFi " << link_name() << " pending connection to service "
                  << pending_service_->unique_name()
                  << " after disconnect";
  }

  // If we disconnect, initially scan at a faster frequency, to make sure
  // we've found all available APs.
  RestartFastScanAttempts();
}

// We use the term "Roam" loosely. In particular, we include the case
// where we "Roam" to a BSS from the disconnected state.
void WiFi::HandleRoam(const ::DBus::Path &new_bss) {
  EndpointMap::iterator endpoint_it = endpoint_by_rpcid_.find(new_bss);
  if (endpoint_it == endpoint_by_rpcid_.end()) {
    LOG(WARNING) << "WiFi " << link_name() << " connected to unknown BSS "
                 << new_bss;
    return;
  }

  const WiFiEndpointConstRefPtr endpoint(endpoint_it->second);
  WiFiServiceRefPtr service = provider_->FindServiceForEndpoint(endpoint);
  if (!service.get()) {
      LOG(WARNING) << "WiFi " << link_name()
                   << " could not find Service for Endpoint "
                   << endpoint->bssid_string()
                   << " (service will be unchanged)";
      return;
  }

  SLOG(WiFi, 2) << "WiFi " << link_name()
                << " roamed to Endpoint " << endpoint->bssid_string()
                << " " << LogSSID(endpoint->ssid_string());

  service->NotifyCurrentEndpoint(endpoint);
  provider_->IncrementConnectCount(endpoint->frequency());

  if (pending_service_.get() &&
      service.get() != pending_service_.get()) {
    // The Service we've roamed on to is not the one we asked for.
    // We assume that this is transient, and that wpa_supplicant
    // is trying / will try to connect to |pending_service_|.
    //
    // If it succeeds, we'll end up back here, but with |service|
    // pointing at the same service as |pending_service_|.
    //
    // If it fails, we'll process things in HandleDisconnect.
    //
    // So we leave |pending_service_| untouched.
    SLOG(WiFi, 2) << "WiFi " << link_name()
                  << " new current Endpoint "
                  << endpoint->bssid_string()
                  << " is not part of pending service "
                  << pending_service_->unique_name();

    // Sanity check: if we didn't roam onto |pending_service_|, we
    // should still be on |current_service_|.
    if (service.get() != current_service_.get()) {
      LOG(WARNING) << "WiFi " << link_name()
                   << " new current Endpoint "
                   << endpoint->bssid_string()
                   << " is neither part of pending service "
                   << pending_service_->unique_name()
                   << " nor part of current service "
                   << (current_service_ ?
                       current_service_->unique_name() :
                       "(NULL)");
      // Although we didn't expect to get here, we should keep
      // |current_service_| in sync with what supplicant has done.
      current_service_ = service;
    }
    return;
  }

  if (pending_service_.get()) {
    // We assume service.get() == pending_service_.get() here, because
    // of the return in the previous if clause.
    //
    // Boring case: we've connected to the service we asked
    // for. Simply update |current_service_| and |pending_service_|.
    current_service_ = service;
    SetPendingService(NULL);
    return;
  }

  // |pending_service_| was NULL, so we weren't attempting to connect
  // to a new Service. Sanity check that we're still on
  // |current_service_|.
  if (service.get() != current_service_.get()) {
    LOG(WARNING)
        << "WiFi " << link_name()
        << " new current Endpoint "
        << endpoint->bssid_string()
        << (current_service_.get() ?
            StringPrintf(" is not part of current service %s",
                         current_service_->unique_name().c_str()) :
            " with no current service");
    // We didn't expect to be here, but let's cope as well as we
    // can. Update |current_service_| to keep it in sync with
    // supplicant.
    current_service_ = service;

    // If this service isn't already marked as actively connecting (likely,
    // since this service is a bit of a surprise) set the service as
    // associating.
    if (!current_service_->IsConnecting()) {
      current_service_->SetState(Service::kStateAssociating);
    }

    return;
  }

  // At this point, we know that |pending_service_| was NULL, and that
  // we're still on |current_service_|. This is the most boring case
  // of all, because there's no state to update here.
  return;
}

string WiFi::FindNetworkRpcidForService(
    const WiFiService *service, Error *error) {
  ReverseServiceMap::const_iterator rpcid_it =
      rpcid_by_service_.find(service);
  if (rpcid_it == rpcid_by_service_.end()) {
    const string error_message =
        StringPrintf(
            "WiFi %s cannot find supplicant network rpcid for service %s",
            link_name().c_str(), service->unique_name().c_str());
    // There are contexts where this is not an error, such as when a service
    // is clearing whatever cached credentials may not exist.
    SLOG(WiFi, 2) << error_message;
    if (error) {
      error->Populate(Error::kNotFound, error_message);
    }
    return "";
  }

  return rpcid_it->second;
}

bool WiFi::DisableNetworkForService(const WiFiService *service, Error *error) {
  string rpcid = FindNetworkRpcidForService(service, error);
  if (rpcid.empty()) {
      // Error is already populated.
      return false;
  }

  if (!DisableNetwork(rpcid)) {
    const string error_message =
        StringPrintf("WiFi %s cannot disable network for service %s: "
                     "DBus operation failed for rpcid %s.",
                     link_name().c_str(), service->unique_name().c_str(),
                     rpcid.c_str());
    Error::PopulateAndLog(error, Error::kOperationFailed, error_message);

    // Make sure that such errored networks are removed, so problems do not
    // propogate to future connection attempts.
    RemoveNetwork(rpcid);
    rpcid_by_service_.erase(service);

    return false;
  }

  return true;
}

bool WiFi::RemoveNetworkForService(const WiFiService *service, Error *error) {
  string rpcid = FindNetworkRpcidForService(service, error);
  if (rpcid.empty()) {
      // Error is already populated.
      return false;
  }

  // Erase the rpcid from our tables regardless of failure below, since even
  // if in failure, we never want to use this network again.
  rpcid_by_service_.erase(service);

  // TODO(quiche): Reconsider giving up immediately. Maybe give
  // wpa_supplicant some time to retry, first.
  if (!RemoveNetwork(rpcid)) {
    const string error_message =
        StringPrintf("WiFi %s cannot remove network for service %s: "
                     "DBus operation failed for rpcid %s.",
                     link_name().c_str(), service->unique_name().c_str(),
                     rpcid.c_str());
    Error::PopulateAndLog(error, Error::kOperationFailed, error_message);
    return false;
  }

  return true;
}

void WiFi::BSSAddedTask(
    const ::DBus::Path &path,
    const map<string, ::DBus::Variant> &properties) {
  // Note: we assume that BSSIDs are unique across endpoints. This
  // means that if an AP reuses the same BSSID for multiple SSIDs, we
  // lose.
  WiFiEndpointRefPtr endpoint(
      new WiFiEndpoint(proxy_factory_, this, path, properties));
  SLOG(WiFi, 1) << "Found endpoint. "
                << "RPC path: " << path << ", "
                << LogSSID(endpoint->ssid_string()) << ", "
                << "bssid: " << endpoint->bssid_string() << ", "
                << "signal: " << endpoint->signal_strength() << ", "
                << "security: " << endpoint->security_mode() << ", "
                << "frequency: " << endpoint->frequency();

  if (endpoint->ssid_string().empty()) {
    // Don't bother trying to find or create a Service for an Endpoint
    // without an SSID. We wouldn't be able to connect to it anyway.
    return;
  }

  if (endpoint->ssid()[0] == 0) {
    // Assume that an SSID starting with NULL is bogus/misconfigured,
    // and filter it out.
    return;
  }

  provider_->OnEndpointAdded(endpoint);

  // Do this last, to maintain the invariant that any Endpoint we
  // know about has a corresponding Service.
  //
  // TODO(quiche): Write test to verify correct behavior in the case
  // where we get multiple BSSAdded events for a single endpoint.
  // (Old Endpoint's refcount should fall to zero, and old Endpoint
  // should be destroyed.)
  endpoint_by_rpcid_[path] = endpoint;
  endpoint->Start();
}

void WiFi::BSSRemovedTask(const ::DBus::Path &path) {
  EndpointMap::iterator i = endpoint_by_rpcid_.find(path);
  if (i == endpoint_by_rpcid_.end()) {
    LOG(WARNING) << "WiFi " << link_name()
                 << " could not find BSS " << path
                 << " to remove.";
    return;
  }

  WiFiEndpointRefPtr endpoint = i->second;
  CHECK(endpoint);
  endpoint_by_rpcid_.erase(i);

  WiFiServiceRefPtr service = provider_->OnEndpointRemoved(endpoint);
  if (!service) {
    return;
  }
  Error unused_error;
  RemoveNetworkForService(service, &unused_error);

  bool disconnect_service = !service->HasEndpoints() &&
      (service->IsConnecting() || service->IsConnected());

  if (disconnect_service) {
    LOG(INFO) << "Disconnecting from service " << service->unique_name()
              << ": BSSRemoved";
    DisconnectFrom(service);
  }
}

void WiFi::CertificationTask(
    const map<string, ::DBus::Variant> &properties) {
  if (!current_service_) {
    LOG(ERROR) << "WiFi " << link_name() << " " << __func__
               << " with no current service.";
    return;
  }

  string subject;
  uint32 depth;
  if (WPASupplicant::ExtractRemoteCertification(properties, &subject, &depth)) {
    current_service_->AddEAPCertification(subject, depth);
  }
}

void WiFi::EAPEventTask(const string &status, const string &parameter) {
  if (!current_service_) {
    LOG(ERROR) << "WiFi " << link_name() << " " << __func__
               << " with no current service.";
    return;
  }
  Service::ConnectFailure failure = Service::kFailureUnknown;
  eap_state_handler_->ParseStatus(status, parameter, &failure);
  if (failure != Service::kFailureUnknown) {
    // Avoid a reporting failure twice by resetting EAP state handler early.
    eap_state_handler_->Reset();
    Error unused_error;
    current_service_->DisconnectWithFailure(failure, &unused_error);
  }
}

void WiFi::PropertiesChangedTask(
    const map<string, ::DBus::Variant> &properties) {
  // TODO(quiche): Handle changes in other properties (e.g. signal
  // strength).

  // Note that order matters here. In particular, we want to process
  // changes in the current BSS before changes in state. This is so
  // that we update the state of the correct Endpoint/Service.

  map<string, ::DBus::Variant>::const_iterator properties_it =
      properties.find(WPASupplicant::kInterfacePropertyCurrentBSS);
  if (properties_it != properties.end()) {
    CurrentBSSChanged(properties_it->second.reader().get_path());
  }

  properties_it = properties.find(WPASupplicant::kInterfacePropertyState);
  if (properties_it != properties.end()) {
    StateChanged(properties_it->second.reader().get_string());
  }
}

void WiFi::ScanDoneTask() {
  SLOG(WiFi, 2) << __func__ << " need_bss_flush_ " << need_bss_flush_;
  if (need_bss_flush_) {
    CHECK(supplicant_interface_proxy_ != NULL);
    // Compute |max_age| relative to |resumed_at_|, to account for the
    // time taken to scan.
    struct timeval now;
    uint32_t max_age;
    time_->GetTimeMonotonic(&now);
    max_age = kMaxBSSResumeAgeSeconds + (now.tv_sec - resumed_at_.tv_sec);
    supplicant_interface_proxy_->FlushBSS(max_age);
    need_bss_flush_ = false;
  }
  SetScanPending(false);
  StartScanTimer();
}

void WiFi::ScanTask() {
  SLOG(WiFi, 2) << "WiFi " << link_name() << " scan requested.";
  if (!enabled()) {
    SLOG(WiFi, 2) << "Ignoring scan request while device is not enabled.";
    return;
  }
  if (!supplicant_present_ || !supplicant_interface_proxy_.get()) {
    SLOG(WiFi, 2) << "Ignoring scan request while supplicant is not present.";
    return;
  }
  if ((pending_service_.get() && pending_service_->IsConnecting()) ||
      (current_service_.get() && current_service_->IsConnecting())) {
    SLOG(WiFi, 2) << "Ignoring scan request while connecting to an AP.";
    return;
  }
  map<string, DBus::Variant> scan_args;
  scan_args[WPASupplicant::kPropertyScanType].writer().
      append_string(WPASupplicant::kScanTypeActive);

  ByteArrays hidden_ssids = provider_->GetHiddenSSIDList();
  if (!hidden_ssids.empty()) {
    // TODO(pstew): Devise a better method for time-sharing with SSIDs that do
    // not fit in.
    if (hidden_ssids.size() >= WPASupplicant::kScanMaxSSIDsPerScan) {
      hidden_ssids.erase(
          hidden_ssids.begin() + WPASupplicant::kScanMaxSSIDsPerScan - 1,
          hidden_ssids.end());
    }
    // Add Broadcast SSID, signified by an empty ByteArray.  If we specify
    // SSIDs to wpa_supplicant, we need to explicitly specify the default
    // behavior of doing a broadcast probe.
    hidden_ssids.push_back(ByteArray());

    scan_args[WPASupplicant::kPropertyScanSSIDs] =
        DBusAdaptor::ByteArraysToVariant(hidden_ssids);
  }

  try {
    supplicant_interface_proxy_->Scan(scan_args);
    SetScanPending(true);
  } catch (const DBus::Error &e) {  // NOLINT
    // A scan may fail if, for example, the wpa_supplicant vanishing
    // notification is posted after this task has already started running.
    LOG(WARNING) << "Scan failed: " << e.what();
  }
}

void WiFi::SetScanPending(bool pending) {
  if (scan_pending_ != pending) {
    scan_pending_ = pending;
    adaptor()->EmitBoolChanged(flimflam::kScanningProperty, pending);
  }
}

string WiFi::GetServiceLeaseName(const WiFiService &service) {
  return service.GetStorageIdentifier();
}

void WiFi::DestroyServiceLease(const WiFiService &service) {
  DestroyIPConfigLease(GetServiceLeaseName(service));
}

void WiFi::StateChanged(const string &new_state) {
  const string old_state = supplicant_state_;
  supplicant_state_ = new_state;
  LOG(INFO) << "WiFi " << link_name() << " " << __func__ << " "
            << old_state << " -> " << new_state;

  WiFiService *affected_service;
  // Identify the service to which the state change applies. If
  // |pending_service_| is non-NULL, then the state change applies to
  // |pending_service_|. Otherwise, it applies to |current_service_|.
  //
  // This policy is driven by the fact that the |pending_service_|
  // doesn't become the |current_service_| until wpa_supplicant
  // reports a CurrentBSS change to the |pending_service_|. And the
  // CurrentBSS change won't be reported until the |pending_service_|
  // reaches the WPASupplicant::kInterfaceStateCompleted state.
  affected_service =
      pending_service_.get() ? pending_service_.get() : current_service_.get();
  if (!affected_service) {
    SLOG(WiFi, 2) << "WiFi " << link_name() << " " << __func__
                  << " with no service";
    return;
  }

  if (new_state == WPASupplicant::kInterfaceStateCompleted) {
    if (affected_service->IsConnected()) {
      StopReconnectTimer();
      EnableHighBitrates();
    } else if (has_already_completed_) {
      LOG(INFO) << link_name() << " L3 configuration already started.";
    } else if (AcquireIPConfigWithLeaseName(
                   GetServiceLeaseName(*affected_service))) {
      LOG(INFO) << link_name() << " is up; started L3 configuration.";
      affected_service->SetState(Service::kStateConfiguring);
    } else {
      LOG(ERROR) << "Unable to acquire DHCP config.";
    }
    has_already_completed_ = true;
  } else if (new_state == WPASupplicant::kInterfaceStateAssociated) {
    affected_service->SetState(Service::kStateAssociating);
  } else if (new_state == WPASupplicant::kInterfaceStateAuthenticating ||
             new_state == WPASupplicant::kInterfaceStateAssociating ||
             new_state == WPASupplicant::kInterfaceState4WayHandshake ||
             new_state == WPASupplicant::kInterfaceStateGroupHandshake) {
    // Ignore transitions into these states from Completed, to avoid
    // bothering the user when roaming, or re-keying.
    if (old_state != WPASupplicant::kInterfaceStateCompleted)
      affected_service->SetState(Service::kStateAssociating);
    // TOOD(quiche): On backwards transitions, we should probably set
    // a timeout for getting back into the completed state. At present,
    // we depend on wpa_supplicant eventually reporting that CurrentBSS
    // has changed. But there may be cases where that signal is not sent.
    // (crosbug.com/23207)
  } else if (new_state == WPASupplicant::kInterfaceStateDisconnected &&
             affected_service == current_service_ &&
             affected_service->IsConnected()) {
    // This means that wpa_supplicant failed in a re-connect attempt, but
    // may still be reconnecting.  Give wpa_supplicant a limited amount of
    // time to transition out this condition by either connecting or changing
    // CurrentBSS.
    StartReconnectTimer();
  } else {
    // Other transitions do not affect Service state.
    //
    // Note in particular that we ignore a State change into
    // kInterfaceStateDisconnected, in favor of observing the corresponding
    // change in CurrentBSS.
  }
}

bool WiFi::SuspectCredentials(
    const WiFiService &service, Service::ConnectFailure *failure) const {
  if (service.IsSecurityMatch(flimflam::kSecurityPsk)) {
    if (supplicant_state_ == WPASupplicant::kInterfaceState4WayHandshake &&
        !service.has_ever_connected()) {
      if (failure) {
        *failure = Service::kFailureBadPassphrase;
      }
      return true;
    }
  } else if (service.IsSecurityMatch(flimflam::kSecurity8021x)) {
    if (eap_state_handler_->is_eap_in_progress() &&
        !service.has_ever_connected()) {
      if (failure) {
        *failure = Service::kFailureEAPAuthentication;
      }
      return true;
    }
  }

  return false;
}

// static
bool WiFi::SanitizeSSID(string *ssid) {
  CHECK(ssid);

  size_t ssid_len = ssid->length();
  size_t i;
  bool changed = false;

  for (i = 0; i < ssid_len; ++i) {
    if (!g_ascii_isprint((*ssid)[i])) {
      (*ssid)[i] = '?';
      changed = true;
    }
  }

  return changed;
}

// static
string WiFi::LogSSID(const string &ssid) {
  string out;
  for (string::const_iterator it = ssid.begin(); it != ssid.end(); ++it) {
    // Replace '[' and ']' (in addition to non-printable characters) so that
    // it's easy to match the right substring through a non-greedy regex.
    if (*it == '[' || *it == ']' || !g_ascii_isprint(*it)) {
      base::StringAppendF(&out, "\\x%02x", *it);
    } else {
      out += *it;
    }
  }
  return StringPrintf("[SSID=%s]", out.c_str());
}

void WiFi::OnLinkMonitorFailure() {
  // If we have never found the gateway, let's be conservative and not
  // do anything, in case this network topology does not have a gateway.
  if (!link_monitor()->IsGatewayFound()) {
    LOG(INFO) << "In " << __func__ << "(): "
              << "Skipping reassociate since gateway was never found.";
    return;
  }

  if (!supplicant_present_) {
    LOG(ERROR) << "In " << __func__ << "(): "
               << "wpa_supplicant is not present.  Cannot reassociate.";
    return;
  }

  try {
    // This will force a transition out of connected, if we are actually
    // connected.
    supplicant_interface_proxy_->Reassociate();
    // If we don't eventually get a transition back into a connected state,
    // there is something wrong.
    StartReconnectTimer();
    LOG(INFO) << "In " << __func__ << "(): Called Reassociate().";
  } catch (const DBus::Error &e) {  // NOLINT
    LOG(ERROR) << "In " << __func__ << "(): failed to call Reassociate().";
    return;
  }
}

bool WiFi::ShouldUseArpGateway() const {
  return true;
}

void WiFi::DisassociateFromService(const WiFiServiceRefPtr &service) {
  DisconnectFrom(service);
  if (service == selected_service()) {
    DropConnection();
  }
  Error unused_error;
  RemoveNetworkForService(service, &unused_error);
}

vector<GeolocationInfo> WiFi::GetGeolocationObjects() const {
  vector<GeolocationInfo> objects;
  for (EndpointMap::const_iterator it = endpoint_by_rpcid_.begin();
      it != endpoint_by_rpcid_.end();
      ++it) {
    GeolocationInfo geoinfo;
    WiFiEndpointRefPtr endpoint = it->second;
    geoinfo.AddField(kGeoMacAddressProperty, endpoint->bssid_string());
    geoinfo.AddField(kGeoSignalStrengthProperty,
                StringPrintf("%d", endpoint->signal_strength()));
    geoinfo.AddField(
        kGeoChannelProperty,
        StringPrintf("%d",
                     Metrics::WiFiFrequencyToChannel(endpoint->frequency())));
    // TODO(gauravsh): Include age field. crosbug.com/35445
    objects.push_back(geoinfo);
  }
  return objects;
}

void WiFi::HelpRegisterDerivedInt32(
    PropertyStore *store,
    const string &name,
    int32(WiFi::*get)(Error *error),
    void(WiFi::*set)(const int32 &value, Error *error)) {
  store->RegisterDerivedInt32(
      name,
      Int32Accessor(new CustomAccessor<WiFi, int32>(this, get, set)));
}

void WiFi::HelpRegisterDerivedUint16(
    PropertyStore *store,
    const string &name,
    uint16(WiFi::*get)(Error *error),
    void(WiFi::*set)(const uint16 &value, Error *error)) {
  store->RegisterDerivedUint16(
      name,
      Uint16Accessor(new CustomAccessor<WiFi, uint16>(this, get, set)));
}

void WiFi::OnAfterResume() {
  LOG(INFO) << __func__;
  Device::OnAfterResume();  // May refresh ipconfig_

  // We want to flush the BSS cache, but we don't want to conflict
  // with a running scan or an active connection attempt. So record
  // the need to flush, and take care of flushing when the next scan
  // completes.
  //
  // Note that supplicant will automatically expire old cache
  // entries (after, e.g., a BSS is not found in two consecutive
  // scans). However, our explicit flush accelerates re-association
  // in cases where a BSS disappeared while we were asleep. (See,
  // e.g. WiFiRoaming.005SuspendRoam.)
  time_->GetTimeMonotonic(&resumed_at_);
  need_bss_flush_ = true;

  if (!scan_pending_ && IsIdle()) {
    // Not scanning/connecting/connected, so let's get things rolling.
    Scan(kProgressiveScan, NULL);
  } else {
    SLOG(WiFi, 1) << __func__
                  << " skipping scan, already scanning or connected.";
  }
}

void WiFi::OnConnected() {
  Device::OnConnected();
  EnableHighBitrates();
}

void WiFi::RestartFastScanAttempts() {
  fast_scans_remaining_ = kNumFastScanAttempts;
  StartScanTimer();
}

void WiFi::StartScanTimer() {
  if (scan_interval_seconds_ == 0) {
    StopScanTimer();
    return;
  }
  scan_timer_callback_.Reset(
      Bind(&WiFi::ScanTimerHandler, weak_ptr_factory_.GetWeakPtr()));
  // Repeat the first few scans after disconnect relatively quickly so we
  // have reasonable trust that no APs we are looking for are present.
  dispatcher()->PostDelayedTask(scan_timer_callback_.callback(),
      fast_scans_remaining_ > 0 ?
          kFastScanIntervalSeconds * 1000 : scan_interval_seconds_ * 1000);
}

void WiFi::StopScanTimer() {
  scan_timer_callback_.Cancel();
}

void WiFi::ScanTimerHandler() {
  SLOG(WiFi, 2) << "WiFi Device " << link_name() << ": " << __func__;
  if (IsIdle() && !scan_pending_) {
    Scan(kProgressiveScan, NULL);
    if (fast_scans_remaining_ > 0) {
      --fast_scans_remaining_;
    }
  }
  StartScanTimer();
}

void WiFi::StartPendingTimer() {
  pending_timeout_callback_.Reset(
      Bind(&WiFi::PendingTimeoutHandler, weak_ptr_factory_.GetWeakPtr()));
  dispatcher()->PostDelayedTask(pending_timeout_callback_.callback(),
                                kPendingTimeoutSeconds * 1000);
}

void WiFi::StopPendingTimer() {
  pending_timeout_callback_.Cancel();
}

void WiFi::SetPendingService(const WiFiServiceRefPtr &service) {
  SLOG(WiFi, 2) << "WiFi " << link_name() << " setting pending service to "
                << (service ? service->unique_name(): "NULL");
  if (service) {
    service->SetState(Service::kStateAssociating);
    StartPendingTimer();
  } else if (pending_service_) {
    StopPendingTimer();
  }
  pending_service_ = service;
}

void WiFi::PendingTimeoutHandler() {
  Error unused_error;
  LOG(INFO) << "WiFi Device " << link_name() << ": " << __func__;
  CHECK(pending_service_);
  pending_service_->DisconnectWithFailure(
      Service::kFailureOutOfRange, &unused_error);

  // A hidden service may have no endpoints, since wpa_supplicant
  // failed to attain a CurrentBSS.  If so, the service has no
  // reference to |this| device and cannot call WiFi::DisconnectFrom()
  // to reset pending_service_.  In this case, we must perform the
  // disconnect here ourselves.
  if (pending_service_) {
    CHECK(!pending_service_->HasEndpoints());
    LOG(INFO) << "Hidden service was not found.";
    DisconnectFrom(pending_service_);
  }
}

void WiFi::StartReconnectTimer() {
  if (!reconnect_timeout_callback_.IsCancelled()) {
    LOG(INFO) << "WiFi Device " << link_name() << ": " << __func__
              << ": reconnect timer already running.";
    return;
  }
  LOG(INFO) << "WiFi Device " << link_name() << ": " << __func__;
  reconnect_timeout_callback_.Reset(
      Bind(&WiFi::ReconnectTimeoutHandler, weak_ptr_factory_.GetWeakPtr()));
  dispatcher()->PostDelayedTask(reconnect_timeout_callback_.callback(),
                                kReconnectTimeoutSeconds * 1000);
}

void WiFi::StopReconnectTimer() {
  SLOG(WiFi, 2) << "WiFi Device " << link_name() << ": " << __func__;
  reconnect_timeout_callback_.Cancel();
}

void WiFi::ReconnectTimeoutHandler() {
  LOG(INFO) << "WiFi Device " << link_name() << ": " << __func__;
  reconnect_timeout_callback_.Cancel();
  CHECK(current_service_);
  current_service_->SetFailureSilent(Service::kFailureConnect);
  DisconnectFrom(current_service_);
}

void WiFi::OnSupplicantAppear(const string &/*owner*/) {
  LOG(INFO) << "WPA supplicant appeared.";
  if (supplicant_present_) {
    // Restart the WiFi device if it's started already. This will reset the
    // state and connect the device to the new WPA supplicant instance.
    if (enabled()) {
      Restart();
    }
    return;
  }
  supplicant_present_ = true;
  ConnectToSupplicant();
}

void WiFi::OnSupplicantVanish() {
  LOG(INFO) << "WPA supplicant vanished.";
  if (!supplicant_present_) {
    return;
  }
  supplicant_present_ = false;
  // Restart the WiFi device if it's started already. This will effectively
  // suspend the device until the WPA supplicant reappears.
  if (enabled()) {
    Restart();
  }
}

void WiFi::OnWiFiDebugScopeChanged(bool enabled) {
  SLOG(WiFi, 2) << "WiFi debug scope changed; enable is now " << enabled;
  if (!supplicant_process_proxy_.get()) {
    SLOG(WiFi, 2) << "Suplicant process proxy not present.";
    return;
  }
  string current_level;
  try {
    current_level = supplicant_process_proxy_->GetDebugLevel();
  } catch (const DBus::Error &e) {  // NOLINT
    LOG(ERROR) << __func__ << ": Failed to get wpa_supplicant debug level.";
    return;
  }

  if (current_level != WPASupplicant::kDebugLevelInfo &&
      current_level != WPASupplicant::kDebugLevelDebug) {
    SLOG(WiFi, 2) << "WiFi debug level is currently "
                  << current_level
                  << "; assuming that it is being controlled elsewhere.";
    return;
  }
  string new_level = enabled ? WPASupplicant::kDebugLevelDebug :
      WPASupplicant::kDebugLevelInfo;

  if (new_level == current_level) {
    SLOG(WiFi, 2) << "WiFi debug level is already the desired level "
                  << current_level;
    return;
  }

  try {
    supplicant_process_proxy_->SetDebugLevel(new_level);
  } catch (const DBus::Error &e) {  // NOLINT
    LOG(ERROR) << __func__ << ": Failed to set wpa_supplicant debug level.";
  }
}

void WiFi::SetConnectionDebugging(bool enabled) {
  if (is_debugging_connection_ == enabled) {
    return;
  }
  OnWiFiDebugScopeChanged(
      enabled ||
      ScopeLogger::GetInstance()->IsScopeEnabled(ScopeLogger::kWiFi));
  is_debugging_connection_ = enabled;
}

void WiFi::ConnectToSupplicant() {
  LOG(INFO) << link_name() << ": " << (enabled() ? "enabled" : "disabled")
            << " supplicant: "
            << (supplicant_present_ ? "present" : "absent")
            << " proxy: "
            << (supplicant_process_proxy_.get() ? "non-null" : "null");
  if (!enabled() || !supplicant_present_ || supplicant_process_proxy_.get()) {
    return;
  }
  supplicant_process_proxy_.reset(
      proxy_factory_->CreateSupplicantProcessProxy(
          WPASupplicant::kDBusPath, WPASupplicant::kDBusAddr));
  OnWiFiDebugScopeChanged(
      ScopeLogger::GetInstance()->IsScopeEnabled(ScopeLogger::kWiFi));
  ::DBus::Path interface_path;
  try {
    map<string, DBus::Variant> create_interface_args;
    create_interface_args[WPASupplicant::kInterfacePropertyName].writer().
        append_string(link_name().c_str());
    create_interface_args[WPASupplicant::kInterfacePropertyDriver].writer().
        append_string(WPASupplicant::kDriverNL80211);
    create_interface_args[
        WPASupplicant::kInterfacePropertyConfigFile].writer().
        append_string(WPASupplicant::kSupplicantConfPath);
    interface_path =
        supplicant_process_proxy_->CreateInterface(create_interface_args);
  } catch (const DBus::Error &e) {  // NOLINT
    if (!strcmp(e.name(), WPASupplicant::kErrorInterfaceExists)) {
      interface_path =
          supplicant_process_proxy_->GetInterface(link_name());
      // TODO(quiche): Is it okay to crash here, if device is missing?
    } else {
      LOG(ERROR) << __func__ << ": Failed to create interface with supplicant.";
      return;
    }
  }

  supplicant_interface_proxy_.reset(
      proxy_factory_->CreateSupplicantInterfaceProxy(
          this, interface_path, WPASupplicant::kDBusAddr));

  RTNLHandler::GetInstance()->SetInterfaceFlags(interface_index(), IFF_UP,
                                                IFF_UP);
  // TODO(quiche) Set ApScan=1 and BSSExpireAge=190, like flimflam does?

  // Clear out any networks that might previously have been configured
  // for this interface.
  supplicant_interface_proxy_->RemoveAllNetworks();

  // Flush interface's BSS cache, so that we get BSSAdded signals for
  // all BSSes (not just new ones since the last scan).
  supplicant_interface_proxy_->FlushBSS(0);

  try {
    // TODO(pstew): Disable fast_reauth until supplicant can properly deal
    // with RADIUS servers that respond strangely to such requests.
    // crosbug.com/25630
    supplicant_interface_proxy_->SetFastReauth(false);
  } catch (const DBus::Error &e) {  // NOLINT
    LOG(ERROR) << "Failed to disable fast_reauth. "
               << "May be running an older version of wpa_supplicant.";
  }

  try {
    // Helps with passing WiFiRomaing.001SSIDSwitchBack.
    supplicant_interface_proxy_->SetScanInterval(kRescanIntervalSeconds);
  } catch (const DBus::Error &e) {  // NOLINT
    LOG(ERROR) << "Failed to set scan_interval. "
               << "May be running an older version of wpa_supplicant.";
  }

  try {
    supplicant_interface_proxy_->SetDisableHighBitrates(true);
  } catch (const DBus::Error &e) {  // NOLINT
    LOG(ERROR) << "Failed to disable high bitrates. "
               << "May be running an older version of wpa_supplicant.";
  }

  Scan(kProgressiveScan, NULL);
  StartScanTimer();
}

void WiFi::EnableHighBitrates() {
  LOG(INFO) << "Enabling high bitrates.";
  try {
    supplicant_interface_proxy_->EnableHighBitrates();
  } catch (const DBus::Error &e) {  // NOLINT
    LOG(ERROR) << "exception while enabling high rates: " << e.what();
  }
}

void WiFi::Restart() {
  LOG(INFO) << link_name() << " restarting.";
  WiFiRefPtr me = this;  // Make sure we don't get destructed.
  // Go through the manager rather than starting and stopping the device
  // directly so that the device can be configured with the profile.
  manager()->DeregisterDevice(me);
  manager()->RegisterDevice(me);
}

}  // namespace shill
