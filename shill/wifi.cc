// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/wifi.h"

#include <time.h>
#include <stdio.h>
#include <string.h>
#include <netinet/ether.h>
#include <linux/if.h>  // Needs definitions from netinet/ether.h

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <vector>

#include <base/logging.h>
#include <base/stringprintf.h>
#include <base/string_number_conversions.h>
#include <base/string_util.h>
#include <chromeos/dbus/service_constants.h>

#include "shill/control_interface.h"
#include "shill/dbus_adaptor.h"
#include "shill/device.h"
#include "shill/error.h"
#include "shill/event_dispatcher.h"
#include "shill/key_value_store.h"
#include "shill/ieee80211.h"
#include "shill/manager.h"
#include "shill/profile.h"
#include "shill/property_accessor.h"
#include "shill/proxy_factory.h"
#include "shill/store_interface.h"
#include "shill/supplicant_interface_proxy_interface.h"
#include "shill/supplicant_process_proxy_interface.h"
#include "shill/technology.h"
#include "shill/wifi_endpoint.h"
#include "shill/wifi_service.h"
#include "shill/wpa_supplicant.h"

using base::StringPrintf;
using std::map;
using std::set;
using std::string;
using std::vector;

namespace shill {

// statics
const char *WiFi::kDefaultBgscanMethod =
    wpa_supplicant::kNetworkBgscanMethodSimple;
const uint16 WiFi::kDefaultBgscanShortIntervalSeconds = 30;
const int32 WiFi::kDefaultBgscanSignalThresholdDbm = -50;
const uint16 WiFi::kDefaultScanIntervalSeconds = 180;
// Note that WiFi generates some manager-level errors, because it implements
// the Manager.GetWiFiService flimflam API. The API is implemented here,
// rather than in manager, to keep WiFi-specific logic in the right place.
const char WiFi::kManagerErrorPassphraseRequired[] = "must specify passphrase";
const char WiFi::kManagerErrorSSIDRequired[] = "must specify SSID";
const char WiFi::kManagerErrorSSIDTooLong[]  = "SSID is too long";
const char WiFi::kManagerErrorSSIDTooShort[] = "SSID is too short";
const char WiFi::kManagerErrorTypeRequired[] = "must specify service type";
const char WiFi::kManagerErrorUnsupportedSecurityMode[] =
    "security mode is unsupported";
const char WiFi::kManagerErrorUnsupportedServiceType[] =
    "service type is unsupported";
const char WiFi::kManagerErrorUnsupportedServiceMode[] =
    "service mode is unsupported";
const char WiFi::kInterfaceStateUnknown[] = "shill-unknown";

WiFi::WiFi(ControlInterface *control_interface,
           EventDispatcher *dispatcher,
           Manager *manager,
           const string& link,
           const string &address,
           int interface_index)
    : Device(control_interface,
             dispatcher,
             manager,
             link,
             address,
             interface_index,
             Technology::kWifi),
      proxy_factory_(ProxyFactory::GetInstance()),
      task_factory_(this),
      link_up_(false),
      supplicant_state_(kInterfaceStateUnknown),
      supplicant_bss_("(unknown)"),
      bgscan_method_(kDefaultBgscanMethod),
      bgscan_short_interval_seconds_(kDefaultBgscanShortIntervalSeconds),
      bgscan_signal_threshold_dbm_(kDefaultBgscanSignalThresholdDbm),
      scan_pending_(false),
      scan_interval_seconds_(kDefaultScanIntervalSeconds) {
  PropertyStore *store = this->mutable_store();
  HelpRegisterDerivedString(store,
                            flimflam::kBgscanMethodProperty,
                            &WiFi::GetBgscanMethod,
                            &WiFi::SetBgscanMethod);
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
  VLOG(2) << "WiFi device " << link_name() << " initialized.";
}

WiFi::~WiFi() {}

void WiFi::Start() {
  ::DBus::Path interface_path;

  supplicant_process_proxy_.reset(
      proxy_factory_->CreateSupplicantProcessProxy(
          wpa_supplicant::kDBusPath, wpa_supplicant::kDBusAddr));
  try {
    map<string, DBus::Variant> create_interface_args;
    create_interface_args["Ifname"].writer().
        append_string(link_name().c_str());
    create_interface_args["Driver"].writer().
        append_string(wpa_supplicant::kDriverNL80211);
    // TODO(quiche) create_interface_args["ConfigFile"].writer().append_string
    // (file with pkcs config info)
    interface_path =
        supplicant_process_proxy_->CreateInterface(create_interface_args);
  } catch (const DBus::Error e) {  // NOLINT
    if (!strcmp(e.name(), wpa_supplicant::kErrorInterfaceExists)) {
      interface_path =
          supplicant_process_proxy_->GetInterface(link_name());
      // TODO(quiche): Is it okay to crash here, if device is missing?
    } else {
      // TODO(quiche): Log error.
    }
  }

  supplicant_interface_proxy_.reset(
      proxy_factory_->CreateSupplicantInterfaceProxy(
          this, interface_path, wpa_supplicant::kDBusAddr));

  // TODO(quiche) Set ApScan=1 and BSSExpireAge=190, like flimflam does?

  // Clear out any networks that might previously have been configured
  // for this interface.
  supplicant_interface_proxy_->RemoveAllNetworks();

  // Flush interface's BSS cache, so that we get BSSAdded signals for
  // all BSSes (not just new ones since the last scan).
  supplicant_interface_proxy_->FlushBSS(0);

  Scan(NULL);
  Device::Start();
}

void WiFi::Stop() {
  VLOG(2) << "WiFi " << link_name() << " stopping.";
  // TODO(quiche): Remove interface from supplicant.
  supplicant_interface_proxy_.reset();  // breaks a reference cycle
  supplicant_process_proxy_.reset();
  endpoint_by_rpcid_.clear();
  rpcid_by_service_.clear();

  for (vector<WiFiServiceRefPtr>::const_iterator it = services_.begin();
       it != services_.end();
       ++it) {
    manager()->DeregisterService(*it);
  }
  services_.clear();                  // breaks reference cycles
  pending_service_ = NULL;            // breaks a reference cycle

  Device::Stop();
  // TODO(quiche): Anything else to do?

  VLOG(3) << "WiFi " << link_name() << " task_factory_ "
          << (task_factory_.empty() ? "is empty." : "is not empty.");
  VLOG(3) << "WiFi " << link_name() << " supplicant_process_proxy_ "
          << (supplicant_process_proxy_.get() ? "is set." : "is not set.");
  VLOG(3) << "WiFi " << link_name() << " supplicant_interface_proxy_ "
          << (supplicant_interface_proxy_.get() ? "is set." : "is not set.");
  VLOG(3) << "WiFi " << link_name() << " pending_service_ "
          << (pending_service_.get() ? "is set." : "is not set.");
  VLOG(3) << "WiFi " << link_name() << " has " << endpoint_by_rpcid_.size()
          << " EndpointMap entries.";
  VLOG(3) << "WiFi " << link_name() << " has " << services_.size()
          << " Services.";
}

bool WiFi::Load(StoreInterface *storage) {
  LoadHiddenServices(storage);
  return Device::Load(storage);
}

void WiFi::Scan(Error */*error*/) {
  LOG(INFO) << __func__;

  // Needs to send a D-Bus message, but may be called from D-Bus
  // signal handler context (via Manager::RequestScan). So defer work
  // to event loop.
  dispatcher()->PostTask(
      task_factory_.NewRunnableMethod(&WiFi::ScanTask));
}

bool WiFi::TechnologyIs(const Technology::Identifier type) const {
  return type == Technology::kWifi;
}

bool WiFi::IsConnectingTo(const WiFiService &service) const {
  return pending_service_ == &service ||
      (current_service_ == &service &&
       service.state() != Service::kStateConnected);
}

void WiFi::LinkEvent(unsigned int flags, unsigned int change) {
  // TODO(quiche): Figure out how to relate these events to supplicant
  // events. E.g., may be we can ignore LinkEvent, in favor of events
  // from SupplicantInterfaceProxy?
  Device::LinkEvent(flags, change);
  if ((flags & IFF_LOWER_UP) != 0 && !link_up_) {
    LOG(INFO) << link_name() << " is up; should start L3!";
    link_up_ = true;
    if (AcquireIPConfig()) {
      SetServiceState(Service::kStateConfiguring);
    } else {
      LOG(ERROR) << "Unable to acquire DHCP config.";
    }
  } else if ((flags & IFF_LOWER_UP) == 0 && link_up_) {
    LOG(INFO) << link_name() << " is down";
    link_up_ = false;
    // TODO(quiche): Attempt to reconnect to current SSID, another SSID,
    // or initiate a scan.
  }
}

void WiFi::BSSAdded(const ::DBus::Path &path,
                    const map<string, ::DBus::Variant> &properties) {
  // Called from a D-Bus signal handler, and nay need to send a D-Bus
  // message. So defer work to event loop.
  dispatcher()->PostTask(
      task_factory_.NewRunnableMethod(&WiFi::BSSAddedTask, path, properties));
}

void WiFi::BSSRemoved(const ::DBus::Path &path) {
  // Called from a D-Bus signal handler, and nay need to send a D-Bus
  // message. So defer work to event loop.
  dispatcher()->PostTask(
      task_factory_.NewRunnableMethod(&WiFi::BSSRemovedTask, path));
}

void WiFi::PropertiesChanged(const map<string, ::DBus::Variant> &properties) {
  LOG(INFO) << "In " << __func__ << "(): called";
  // Called from D-Bus signal handler, but may need to send a D-Bus
  // message. So defer work to event loop.
  dispatcher()->PostTask(
      task_factory_.NewRunnableMethod(&WiFi::PropertiesChangedTask,
                                      properties));
}

void WiFi::ScanDone() {
  LOG(INFO) << __func__;

  // Defer handling of scan result processing, because that processing
  // may require the the registration of new D-Bus objects. And such
  // registration can't be done in the context of a D-Bus signal
  // handler.
  dispatcher()->PostTask(
      task_factory_.NewRunnableMethod(&WiFi::ScanDoneTask));
}

void WiFi::ConnectTo(WiFiService *service,
                     map<string, DBus::Variant> service_params) {
  CHECK(service);
  DBus::Path network_path;

  // TODO(quiche): Handle cases where already connected.
  if (pending_service_ && pending_service_ == service) {
    // TODO(quiche): Return an error to the caller. crosbug.com/23832
    LOG(INFO) << "WiFi " << link_name() << " ignoring ConnectTo "
              << service->friendly_name()
              << ", which is already pending.";
    return;
  }

  if (pending_service_ && pending_service_ != service) {
    DisconnectFrom(pending_service_);
  }

  try {
    // TODO(quiche): Set a timeout here. In the normal case, we expect
    // that, if wpa_supplicant fails to connect, it will eventually send
    // a signal that its CurrentBSS has changed. But there may be cases
    // where the signal is not sent. (crosbug.com/23206)
    const uint32_t scan_ssid = 1;  // "True": Use directed probe.
    service_params[wpa_supplicant::kNetworkPropertyScanSSID].writer().
        append_uint32(scan_ssid);
    service_params[wpa_supplicant::kNetworkPropertyBgscan].writer().
        append_string(CreateBgscanConfigString().c_str());
    network_path =
        supplicant_interface_proxy_->AddNetwork(service_params);
    rpcid_by_service_[service] = network_path;
  } catch (const DBus::Error e) {  // NOLINT
    LOG(ERROR) << "exception while adding network: " << e.what();
    return;
  }

  supplicant_interface_proxy_->SelectNetwork(network_path);

  pending_service_ = service;
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
                 << service->friendly_name()
                 << " which is neither current nor pending";
    return;
  }

  if (pending_service_ && service != pending_service_) {
    // TODO(quiche): Once we have asynchronous reply support, we should
    // generate a D-Bus error here. (crosbug.com/23832)
    LOG(WARNING) << "In " << __func__ << "(): "
                 << " ignoring request to disconnect from service "
                 << service->friendly_name()
                 << " which is not the pending service.";
    return;
  }

  if (!pending_service_ && service != current_service_) {
    // TODO(quiche): Once we have asynchronous reply support, we should
    // generate a D-Bus error here. (crosbug.com/23832)
    LOG(WARNING) << "In " << __func__ << "(): "
                 << " ignoring request to disconnect from service "
                 << service->friendly_name()
                 << " which is not the current service.";
    return;
  }

  pending_service_ = NULL;
  try {
    supplicant_interface_proxy_->Disconnect();
    // We'll call RemoveNetwork and reset |current_service_| after
    // supplicant notifies us that the CurrentBSS has changed.
  } catch (const DBus::Error e) {  // NOLINT
    // Can't depend on getting a notification of CurrentBSS change.
    // So effect changes immediately.
    ReverseServiceMap::const_iterator rpcid_it =
        rpcid_by_service_.find(service);
    DCHECK(rpcid_it != rpcid_by_service_.end());
    if (rpcid_it == rpcid_by_service_.end()) {
      LOG(WARNING) << "WiFi " << link_name() << " can not disconnect from "
                   << service->friendly_name() << ": "
                   << "could not find supplicant network to disable.";
    } else {
      supplicant_interface_proxy_->RemoveNetwork(rpcid_it->second);
    }
    current_service_ = NULL;
  }

  CHECK(current_service_ == NULL ||
        current_service_.get() != pending_service_.get());
}

bool WiFi::IsIdle() const {
  return !current_service_ && !pending_service_;
}

string WiFi::CreateBgscanConfigString() {
  return StringPrintf("%s:%d:%d:%d",
                      bgscan_method_.c_str(),
                      bgscan_short_interval_seconds_,
                      bgscan_signal_threshold_dbm_,
                      scan_interval_seconds_);
}

void WiFi::SetBgscanMethod(const string &method, Error *error) {
  if (method != wpa_supplicant::kNetworkBgscanMethodSimple &&
      method != wpa_supplicant::kNetworkBgscanMethodLearn) {
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
  // We do not update |pending_service_| or |current_service_|, because
  // supplicant does not allow for reconfiguration without disconnect
  // and reconnect.

  // TODO(quiche): Update scan timer. crosbug.com/24337
}

// To avoid creating duplicate services, call FindServiceForEndpoint
// before calling this method.
WiFiServiceRefPtr WiFi::CreateServiceForEndpoint(const WiFiEndpoint &endpoint,
                                                 bool hidden_ssid) {
  WiFiServiceRefPtr service =
      new WiFiService(control_interface(),
                      dispatcher(),
                      manager(),
                      this,
                      endpoint.ssid(),
                      endpoint.network_mode(),
                      endpoint.security_mode(),
                      hidden_ssid);
  services_.push_back(service);
  return service;
}

void WiFi::CurrentBSSChanged(const ::DBus::Path &new_bss) {
  VLOG(3) << "WiFi " << link_name() << " CurrentBSS "
          << supplicant_bss_ << " -> " << new_bss;
  supplicant_bss_ = new_bss;
  if (new_bss == wpa_supplicant::kCurrentBSSNull) {
    HandleDisconnect();
  } else {
    HandleRoam(new_bss);
  }

  SelectService(current_service_);
  // Invariant check: a Service can either be current, or pending, but
  // not both.
  CHECK(current_service_.get() != pending_service_.get() ||
        current_service_.get() == NULL);

  // TODO(quiche): Update BSSID property on the Service
  // (crosbug.com/22377).
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
    VLOG(2) << "WiFi " << link_name()
            << " disconnected while not connected or connecting";
    return;
  }

  ReverseServiceMap::const_iterator rpcid_it =
      rpcid_by_service_.find(affected_service);
  if (rpcid_it == rpcid_by_service_.end()) {
    VLOG(2) << "WiFi " << link_name() << " disconnected from "
            << " (or failed to connect to) "
            << affected_service->friendly_name() << ", "
            << "but could not find supplicant network to disable.";
    return;
  }

  VLOG(2) << "WiFi " << link_name() << " disconnected from "
          << " (or failed to connect to) "
          << affected_service->friendly_name();
  // TODO(quiche): Reconsider giving up immediately. Maybe give
  // wpa_supplicant some time to retry, first.
  supplicant_interface_proxy_->RemoveNetwork(rpcid_it->second);
  // TODO(quiche): If we initated the disconnect, we should probably
  // go to the idle state instead. crosbug.com/24700
  affected_service->SetFailure(Service::kFailureUnknown);

  if (affected_service == pending_service_.get()) {
    // The attempt to connect to |pending_service_| failed. Clear
    // |pending_service_|, to indicate we're no longer in the middle
    // of a connect request.
    pending_service_ = NULL;
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
    VLOG(2) << "WiFi " << link_name() << " pending connection to "
            << pending_service_->friendly_name()
            << " after disconnect";
  }
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

  const WiFiEndpoint &endpoint(*endpoint_it->second);
  WiFiServiceRefPtr service = FindServiceForEndpoint(endpoint);
  if (!service.get()) {
      LOG(WARNING) << "WiFi " << link_name()
                   << " could not find Service for Endpoint "
                   << endpoint.bssid_string()
                   << " (service will be unchanged)";
      return;
  }

  VLOG(2) << "WiFi " << link_name()
          << " roamed to Endpoint " << endpoint.bssid_string()
          << " (SSID " << endpoint.ssid_string() << ")";

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
    VLOG(2) << "WiFi " << link_name()
            << " new current Endpoint "
            << endpoint.bssid_string()
            << " is not part of pending service "
            << pending_service_->friendly_name();

    // Sanity check: if we didn't roam onto |pending_service_|, we
    // should still be on |current_service_|.
    if (service.get() != current_service_.get()) {
      LOG(WARNING) << "WiFi " << link_name()
                   << " new current Endpoint "
                   << endpoint.bssid_string()
                   << " is neither part of pending service "
                   << pending_service_->friendly_name()
                   << " nor part of current service "
                   << (current_service_.get() ?
                       current_service_->friendly_name() :
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
    pending_service_ = NULL;
    return;
  }

  // |pending_service_| was NULL, so we weren't attempting to connect
  // to a new Service. Sanity check that we're still on
  // |current_service_|.
  if (service.get() != current_service_.get()) {
    LOG(WARNING)
        << "WiFi " << link_name()
        << " new current Endpoint "
        << endpoint.bssid_string()
        << (current_service_.get() ?
            StringPrintf(" is not part of current service %s",
                         current_service_->friendly_name().c_str()) :
            " with no current service");
    // We didn't expect to be here, but let's cope as well as we
    // can. Update |current_service_| to keep it in sync with
    // supplicant.
    current_service_ = service;
    return;
  }

  // At this point, we know that |pending_service_| was NULL, and that
  // we're still on |current_service_|. This is the most boring case
  // of all, because there's no state to update here.
  return;
}

WiFiServiceRefPtr WiFi::FindService(const vector<uint8_t> &ssid,
                                    const string &mode,
                                    const string &security) const {
  for (vector<WiFiServiceRefPtr>::const_iterator it = services_.begin();
       it != services_.end();
       ++it) {
    if ((*it)->ssid() == ssid && (*it)->mode() == mode &&
        (*it)->IsSecurityMatch(security)) {
      return *it;
    }
  }
  return NULL;
}

WiFiServiceRefPtr WiFi::FindServiceForEndpoint(const WiFiEndpoint &endpoint) {
  return FindService(endpoint.ssid(),
                     endpoint.network_mode(),
                     endpoint.security_mode());
}

ByteArrays WiFi::GetHiddenSSIDList() {
  // Create a unique set of hidden SSIDs.
  set<ByteArray> hidden_ssids_set;
  for (vector<WiFiServiceRefPtr>::const_iterator it = services_.begin();
       it != services_.end();
       ++it) {
    if ((*it)->hidden_ssid() && (*it)->favorite()) {
      hidden_ssids_set.insert((*it)->ssid());
    }
  }
  VLOG(2) << "Found " << hidden_ssids_set.size() << " hidden services";
  ByteArrays hidden_ssids(hidden_ssids_set.begin(), hidden_ssids_set.end());
  if (!hidden_ssids.empty()) {
    // TODO(pstew): Devise a better method for time-sharing with SSIDs that do
    // not fit in.
    if (hidden_ssids.size() >= wpa_supplicant::kScanMaxSSIDsPerScan) {
      hidden_ssids.erase(
          hidden_ssids.begin() + wpa_supplicant::kScanMaxSSIDsPerScan - 1,
          hidden_ssids.end());
    }
    // Add Broadcast SSID, signified by an empty ByteArray.  If we specify
    // SSIDs to wpa_supplicant, we need to explicitly specify the default
    // behavior of doing a broadcast probe.
    hidden_ssids.push_back(ByteArray());
  }
  return hidden_ssids;
}

bool WiFi::LoadHiddenServices(StoreInterface *storage) {
  bool created_hidden_service = false;
  set<string> groups = storage->GetGroupsWithKey(flimflam::kWifiHiddenSsid);
  for (set<string>::iterator it = groups.begin(); it != groups.end(); ++it) {
    bool is_hidden = false;
    if (!storage->GetBool(*it, flimflam::kWifiHiddenSsid, &is_hidden)) {
      VLOG(2) << "Storage group " << *it << " returned by GetGroupsWithKey "
              << "failed for GetBool(" << flimflam::kWifiHiddenSsid
              << ") -- possible non-bool key";
      continue;
    }
    if (!is_hidden) {
      continue;
    }
    string ssid_hex;
    vector<uint8_t> ssid_bytes;
    if (!storage->GetString(*it, flimflam::kSSIDProperty, &ssid_hex) ||
        !base::HexStringToBytes(ssid_hex, &ssid_bytes)) {
      VLOG(2) << "Hidden network is missing/invalid \""
              << flimflam::kSSIDProperty << "\" property";
      continue;
    }
    string device_address;
    string network_mode;
    string security;
    // It is gross that we have to do this, but the only place we can
    // get information about the service is from its storage name.
    if (!WiFiService::ParseStorageIdentifier(*it, &device_address,
                                             &network_mode, &security) ||
        device_address != address()) {
      VLOG(2) << "Hidden network has unparsable storage identifier \""
              << *it << "\"";
      continue;
    }
    if (FindService(ssid_bytes, network_mode, security).get()) {
      // If service already exists, we have nothing to do, since the
      // service has already loaded its configuration from storage.
      // This is guaranteed to happen in both cases where Load() is
      // called on a device (via a ConfigureDevice() call on the
      // profile):
      //  - In RegisterDevice() the Device hasn't been started yet,
      //    so it has no services registered, except for those
      //    created by previous iterations of LoadHiddenService().
      //    The latter can happen if another profile in the Manager's
      //    stack defines the same ssid/mode/security.  Even this
      //    case is okay, since even if the profiles differ
      //    materially on configuration and credentials, the "right"
      //    one will be configured in the course of the
      //    RegisterService() call below.
      // - In PushProfile(), all registered services have been
      //   introduced to the profile via ConfigureService() prior
      //   to calling ConfigureDevice() on the profile.
      continue;
    }
    WiFiServiceRefPtr service(new WiFiService(control_interface(),
                                              dispatcher(),
                                              manager(),
                                              this,
                                              ssid_bytes,
                                              network_mode,
                                              security,
                                              true));
    services_.push_back(service);

    // By registering the service, the rest of the configuration
    // will be loaded from the profile into the service via ConfigureService().
    manager()->RegisterService(service);

    created_hidden_service = true;
  }

  // If we are idle and we created a service as a result of opening the
  // profile, we should initiate a scan for our new hidden SSID.
  if (running() && created_hidden_service &&
      supplicant_state_ == wpa_supplicant::kInterfaceStateInactive) {
    Scan(NULL);
  }

  return created_hidden_service;
}

void WiFi::BSSAddedTask(
    const ::DBus::Path &path,
    const map<string, ::DBus::Variant> &properties) {
  // TODO(quiche): Write test to verify correct behavior in the case
  // where we get multiple BSSAdded events for a single endpoint.
  // (Old Endpoint's refcount should fall to zero, and old Endpoint
  // should be destroyed.)
  //
  // Note: we assume that BSSIDs are unique across endpoints. This
  // means that if an AP reuses the same BSSID for multiple SSIDs, we
  // lose.
  WiFiEndpointRefPtr endpoint(new WiFiEndpoint(properties));
  endpoint_by_rpcid_[path] = endpoint;
  LOG(INFO) << "Found endpoint. "
            << "ssid: " << endpoint->ssid_string() << ", "
            << "bssid: " << endpoint->bssid_string() << ", "
            << "signal: " << endpoint->signal_strength() << ", "
            << "security: " << endpoint->security_mode();

  if (endpoint->ssid_string().empty()) {
    // Don't bother trying to find or create a Service for an Endpoint
    // without an SSID. We wouldn't be able to connect to it anyway.
    return;
  }

  WiFiServiceRefPtr service = FindServiceForEndpoint(*endpoint);
  if (service) {
    LOG(INFO) << "Assigned endpoint " << endpoint->bssid_string()
              << " to service " << service->friendly_name() << ".";
    service->AddEndpoint(endpoint);

    if (manager()->HasService(service)) {
      manager()->UpdateService(service);
    } else {
      DCHECK_EQ(1, service->NumEndpoints());  // Expect registered by now if >1.
      manager()->RegisterService(service);
    }

    return;
  }

  const bool hidden_ssid = false;
  service = CreateServiceForEndpoint(*endpoint, hidden_ssid);
  LOG(INFO) << "New service " << service->GetRpcIdentifier()
            << " (" << service->friendly_name() << ")";
  service->AddEndpoint(endpoint);
  manager()->RegisterService(service);
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

  if (endpoint->ssid_string().empty()) {
    // In BSSAdded, we don't create Services for Endpoints with empty
    // SSIDs. So don't bother looking for a Service to update.
    return;
  }

  WiFiServiceRefPtr service = FindServiceForEndpoint(*endpoint);
  CHECK(service);
  VLOG(2) << "Removing Endpoint " << endpoint->bssid_string()
          << " from Service " << service->friendly_name();
  service->RemoveEndpoint(endpoint);

  bool disconnect_service = !service->HasEndpoints() &&
      (service->IsConnecting() || service->IsConnected());
  bool forget_service =
      // Forget Services without Endpoints, except that we always keep
      // hidden services around. (We need them around to populate the
      // hidden SSIDs list.)
      !service->HasEndpoints() && !service->hidden_ssid();
  bool deregister_service =
      // Only deregister a Service if we're forgetting it. Otherwise,
      // Manager can't keep our configuration up-to-date (as Profiles
      // change).
      forget_service;

  if (disconnect_service) {
    DisconnectFrom(service);
  }

  if (deregister_service) {
    manager()->DeregisterService(service);
  } else {
    manager()->UpdateService(service);
  }

  if (forget_service) {
    vector<WiFiServiceRefPtr>::iterator it;
    it = std::find(services_.begin(), services_.end(), service);
    if (it != services_.end()) {
      services_.erase(it);
    }
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
      properties.find(wpa_supplicant::kInterfacePropertyCurrentBSS);
  if (properties_it != properties.end()) {
    CurrentBSSChanged(properties_it->second.reader().get_path());
  }

  properties_it = properties.find(wpa_supplicant::kInterfacePropertyState);
  if (properties_it != properties.end()) {
    StateChanged(properties_it->second.reader().get_string());
  }
}

void WiFi::ScanDoneTask() {
  LOG(INFO) << __func__;
  scan_pending_ = false;
}

void WiFi::ScanTask() {
  VLOG(2) << "WiFi " << link_name() << " scan requested.";
  map<string, DBus::Variant> scan_args;
  scan_args[wpa_supplicant::kPropertyScanType].writer().
      append_string(wpa_supplicant::kScanTypeActive);

  ByteArrays hidden_ssids = GetHiddenSSIDList();
  if (!hidden_ssids.empty()) {
    scan_args[wpa_supplicant::kPropertyScanSSIDs] =
        DBusAdaptor::ByteArraysToVariant(hidden_ssids);
  }

  // TODO(quiche): Indicate scanning in UI. crosbug.com/14887
  supplicant_interface_proxy_->Scan(scan_args);
  scan_pending_ = true;
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
  // CurrentBSS change won't we reported until the |pending_service_|
  // reaches the wpa_supplicant::kInterfaceStateCompleted state.
  affected_service =
      pending_service_.get() ? pending_service_.get() : current_service_.get();
  if (!affected_service) {
    VLOG(2) << "WiFi " << link_name() << " " << __func__
            << " with no service";
    return;
  }

  if (new_state == wpa_supplicant::kInterfaceStateCompleted) {
    // TODO(quiche): Check if we have a race with LinkEvent and/or
    // IPConfigUpdatedCallback here.

    // After 802.11 negotiation is Completed, we start Configuring
    // IP connectivity.
    affected_service->SetState(Service::kStateConfiguring);
  } else if (new_state == wpa_supplicant::kInterfaceStateAssociated) {
    // TODO(quiche): Resolve race with LinkEvent. It's possible for us
    // to receive this state notification after LinkEvent. In that case,
    // our state transitions are Associating -> Configuring ->
    // Associating -> Configuring -> Ready. (crosbug.com/22831)
    affected_service->SetState(Service::kStateAssociating);
  } else if (new_state == wpa_supplicant::kInterfaceStateAuthenticating ||
             new_state == wpa_supplicant::kInterfaceStateAssociating ||
             new_state == wpa_supplicant::kInterfaceState4WayHandshake ||
             new_state == wpa_supplicant::kInterfaceStateGroupHandshake) {
    // Ignore transitions into these states from Completed, to avoid
    // bothering the user when roaming, or re-keying.
    if (old_state != wpa_supplicant::kInterfaceStateCompleted)
      affected_service->SetState(Service::kStateAssociating);
    // TOOD(quiche): On backwards transitions, we should probably set
    // a timeout for getting back into the completed state. At present,
    // we depend on wpa_supplicant eventually reporting that CurrentBSS
    // has changed. But there may be cases where that signal is not sent.
    // (crosbug.com/23207)
  } else {
    // Other transitions do not affect Service state.
    //
    // Note in particular that we ignore a State change into
    // kInterfaceStateDisconnected, in favor of observing the corresponding
    // change in CurrentBSS.
  }
}

// Used by Manager.
WiFiServiceRefPtr WiFi::GetService(const KeyValueStore &args, Error *error) {
  if (!args.ContainsString(flimflam::kTypeProperty)) {
    error->Populate(Error::kInvalidArguments, kManagerErrorTypeRequired);
    return NULL;
  }

  if (args.GetString(flimflam::kTypeProperty) != flimflam::kTypeWifi) {
    error->Populate(Error::kNotSupported, kManagerErrorUnsupportedServiceType);
    return NULL;
  }

  if (args.ContainsString(flimflam::kModeProperty) &&
      args.GetString(flimflam::kModeProperty) !=
      flimflam::kModeManaged) {
    error->Populate(Error::kNotSupported, kManagerErrorUnsupportedServiceMode);
    return NULL;
  }

  if (!args.ContainsString(flimflam::kSSIDProperty)) {
    error->Populate(Error::kInvalidArguments, kManagerErrorSSIDRequired);
    return NULL;
  }

  string ssid = args.GetString(flimflam::kSSIDProperty);
  if (ssid.length() < 1) {
    error->Populate(Error::kInvalidNetworkName, kManagerErrorSSIDTooShort);
    return NULL;
  }

  if (ssid.length() > IEEE_80211::kMaxSSIDLen) {
    error->Populate(Error::kInvalidNetworkName, kManagerErrorSSIDTooLong);
    return NULL;
  }

  string security_method;
  if (args.ContainsString(flimflam::kSecurityProperty)) {
    security_method = args.GetString(flimflam::kSecurityProperty);
  } else {
    security_method = flimflam::kSecurityNone;
  }

  if (security_method != flimflam::kSecurityNone &&
      security_method != flimflam::kSecurityWep &&
      security_method != flimflam::kSecurityPsk &&
      security_method != flimflam::kSecurityWpa &&
      security_method != flimflam::kSecurityRsn &&
      security_method != flimflam::kSecurity8021x) {
    error->Populate(Error::kNotSupported,
                    kManagerErrorUnsupportedSecurityMode);
    return NULL;
  }

  if ((security_method == flimflam::kSecurityWep ||
       security_method == flimflam::kSecurityPsk ||
       security_method == flimflam::kSecurityWpa ||
       security_method == flimflam::kSecurityRsn) &&
      !args.ContainsString(flimflam::kPassphraseProperty)) {
    error->Populate(Error::kInvalidArguments,
                    kManagerErrorPassphraseRequired);
    return NULL;
  }

  bool hidden_ssid;
  if (args.ContainsBool(flimflam::kWifiHiddenSsid)) {
    hidden_ssid = args.GetBool(flimflam::kWifiHiddenSsid);
  } else {
    // If the service is not found, and the caller hasn't specified otherwise,
    // we assume this is is a hidden network.
    hidden_ssid = true;
  }

  vector<uint8_t> ssid_bytes(ssid.begin(), ssid.end());
  WiFiServiceRefPtr service(FindService(ssid_bytes, flimflam::kModeManaged,
                                        security_method));
  if (!service.get()) {
    service = new WiFiService(control_interface(),
                              dispatcher(),
                              manager(),
                              this,
                              ssid_bytes,
                              flimflam::kModeManaged,
                              security_method,
                              hidden_ssid);
    services_.push_back(service);
    // NB: We do not register the newly created Service with the Manager.
    // The Service will be registered if/when we find Endpoints for it.
  }

  if (security_method == flimflam::kSecurityWep ||
      security_method == flimflam::kSecurityPsk ||
      security_method == flimflam::kSecurityWpa ||
      security_method == flimflam::kSecurityRsn) {
    service->SetPassphrase(args.GetString(flimflam::kPassphraseProperty),
                           error);
    if (error->IsFailure()) {
      return NULL;
    }
  }

  // TODO(quiche): Apply any other configuration parameters.

  return service;
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

void WiFi::HelpRegisterDerivedString(
    PropertyStore *store,
    const string &name,
    string(WiFi::*get)(Error *error),
    void(WiFi::*set)(const string &value, Error *error)) {
  store->RegisterDerivedString(
      name,
      StringAccessor(new CustomAccessor<WiFi, string>(this, get, set)));
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

}  // namespace shill
