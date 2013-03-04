// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/wifi_provider.h"

#include <set>
#include <string>
#include <vector>

#include <base/bind.h>
#include <base/string_number_conversions.h>

#include "shill/error.h"
#include "shill/event_dispatcher.h"
#include "shill/ieee80211.h"
#include "shill/key_value_store.h"
#include "shill/logging.h"
#include "shill/manager.h"
#include "shill/metrics.h"
#include "shill/profile.h"
#include "shill/store_interface.h"
#include "shill/technology.h"
#include "shill/wifi_endpoint.h"
#include "shill/wifi_service.h"

using base::Bind;
using std::set;
using std::string;
using std::vector;

namespace shill {

// Note that WiFiProvider generates some manager-level errors, because it
// implements the WiFi portion of the Manager.GetService flimflam API. The
// API is implemented here, rather than in manager, to keep WiFi-specific
// logic in the right place.
const char WiFiProvider::kManagerErrorSSIDRequired[] = "must specify SSID";
const char WiFiProvider::kManagerErrorSSIDTooLong[]  = "SSID is too long";
const char WiFiProvider::kManagerErrorSSIDTooShort[] = "SSID is too short";
const char WiFiProvider::kManagerErrorUnsupportedSecurityMode[] =
    "security mode is unsupported";
const char WiFiProvider::kManagerErrorUnsupportedServiceMode[] =
    "service mode is unsupported";

WiFiProvider::WiFiProvider(ControlInterface *control_interface,
                           EventDispatcher *dispatcher,
                           Metrics *metrics,
                           Manager *manager)
    : control_interface_(control_interface),
      dispatcher_(dispatcher),
      metrics_(metrics),
      manager_(manager),
      running_(false) {}

WiFiProvider::~WiFiProvider() {}

void WiFiProvider::Start() {
  running_ = true;
}

void WiFiProvider::Stop() {
  SLOG(WiFi, 2) << __func__;
  while (!services_.empty()) {
    WiFiServiceRefPtr service = services_.back();
    ForgetService(service);
    SLOG(WiFi, 3) << "WiFiProvider deregistering service "
                  << service->unique_name();
    manager_->DeregisterService(service);
  }
  running_ = false;
}

void WiFiProvider::CreateServicesFromProfile(const ProfileRefPtr &profile) {
  const StoreInterface *storage = profile->GetConstStorage();
  KeyValueStore args;
  args.SetString(flimflam::kTypeProperty, flimflam::kTypeWifi);
  set<string> groups = storage->GetGroupsWithProperties(args);
  bool created_hidden_service = false;
  for (set<string>::const_iterator it = groups.begin(); it != groups.end();
       ++it) {
    string ssid_hex;
    vector<uint8_t> ssid_bytes;
    if (!storage->GetString(*it, WiFiService::kStorageSSID, &ssid_hex) ||
        !base::HexStringToBytes(ssid_hex, &ssid_bytes)) {
      SLOG(WiFi, 2) << "Storage group " << *it << " is missing valid \""
                    << WiFiService::kStorageSSID << "\" property";
      continue;
    }
    string network_mode;
    if (!storage->GetString(*it, WiFiService::kStorageMode, &network_mode) ||
        network_mode.empty()) {
      SLOG(WiFi, 2) << "Storage group " << *it << " is missing \""
                    <<  WiFiService::kStorageMode << "\" property";
      continue;
    }
    string security;
    if (!storage->GetString(*it, WiFiService::kStorageSecurity, &security) ||
        !WiFiService::IsValidSecurityMethod(security)) {
      SLOG(WiFi, 2) << "Storage group " << *it << " has missing or invalid \""
                    <<  WiFiService::kStorageSecurity << "\" property";
      continue;
    }
    bool is_hidden = false;
    if (!storage->GetBool(*it, WiFiService::kStorageHiddenSSID, &is_hidden)) {
      SLOG(WiFi, 2) << "Storage group " << *it << " is missing \""
                    << WiFiService::kStorageHiddenSSID << "\" property";
      continue;
    }

    if (FindService(ssid_bytes, network_mode, security)) {
      // If service already exists, we have nothing to do, since the
      // service has already loaded its configuration from storage.
      // This is guaranteed to happen in the single case where
      // CreateServicesFromProfile() is called on a WiFiProvider from
      // Manager::PushProfile():
      continue;
    }

    AddService(ssid_bytes, network_mode, security, is_hidden);

    // By registering the service in AddService, the rest of the configuration
    // will be loaded from the profile into the service via ConfigureService().

    if (is_hidden) {
      created_hidden_service = true;
    }
  }

  // If WiFi is unconnected and we created a hidden service as a result
  // of opening the profile, we should initiate a WiFi scan, which will
  // allow us to find any hidden services that we may have created.
  if (created_hidden_service &&
      !manager_->IsTechnologyConnected(Technology::kWifi)) {
    Error unused_error;
    manager_->RequestScan(flimflam::kTypeWifi, &unused_error);
  }
}

WiFiServiceRefPtr WiFiProvider::FindSimilarService(
    const KeyValueStore &args, Error *error) const {
  vector<uint8_t> ssid;
  string mode;
  string security;
  bool hidden_ssid;

  if (!GetServiceParametersFromArgs(
          args, &ssid, &mode, &security, &hidden_ssid, error)) {
    return NULL;
  }

  WiFiServiceRefPtr service(FindService(ssid, mode, security));
  if (!service) {
    error->Populate(Error::kNotFound, "Matching service was not found");
  }

  return service;
}

WiFiServiceRefPtr WiFiProvider::CreateTemporaryService(
    const KeyValueStore &args, Error *error) {
  vector<uint8_t> ssid;
  string mode;
  string security;
  bool hidden_ssid;

  if (!GetServiceParametersFromArgs(
          args, &ssid, &mode, &security, &hidden_ssid, error)) {
    return NULL;
  }

  return new WiFiService(control_interface_,
                         dispatcher_,
                         metrics_,
                         manager_,
                         this,
                         ssid,
                         mode,
                         security,
                         hidden_ssid);
}

WiFiServiceRefPtr WiFiProvider::GetService(
    const KeyValueStore &args, Error *error) {
  vector<uint8_t> ssid_bytes;
  string mode;
  string security_method;
  bool hidden_ssid;

  if (!GetServiceParametersFromArgs(
          args, &ssid_bytes, &mode, &security_method, &hidden_ssid, error)) {
    return NULL;
  }

  WiFiServiceRefPtr service(FindService(ssid_bytes, mode, security_method));
  if (!service) {
    service = AddService(ssid_bytes,
                         mode,
                         security_method,
                         hidden_ssid);
  }

  return service;
}

WiFiServiceRefPtr WiFiProvider::FindServiceForEndpoint(
    const WiFiEndpointConstRefPtr &endpoint) {
  return FindService(endpoint->ssid(),
                     endpoint->network_mode(),
                     endpoint->security_mode());
}

void WiFiProvider::OnEndpointAdded(const WiFiEndpointConstRefPtr &endpoint) {
  if (!running_) {
    return;
  }

  WiFiServiceRefPtr service = FindServiceForEndpoint(endpoint);
  if (!service) {
    const bool hidden_ssid = false;
    service = AddService(
        endpoint->ssid(),
        endpoint->network_mode(),
        WiFiService::GetSecurityClass(endpoint->security_mode()),
        hidden_ssid);
  }

  service->AddEndpoint(endpoint);

  SLOG(WiFi, 1) << "Assigned endpoint " << endpoint->bssid_string()
                << " to service " << service->unique_name() << ".";

  manager_->UpdateService(service);
}

WiFiServiceRefPtr WiFiProvider::OnEndpointRemoved(
    const WiFiEndpointConstRefPtr &endpoint) {
  if (!running_) {
    return NULL;
  }

  WiFiServiceRefPtr service = FindServiceForEndpoint(endpoint);

  CHECK(service) << "Can't find Service for Endpoint "
                 << "(with BSSID " << endpoint->bssid_string() << ").";
  SLOG(WiFi, 1) << "Removing endpoint " << endpoint->bssid_string()
                << " from Service " << service->unique_name();
  service->RemoveEndpoint(endpoint);

  if (service->HasEndpoints() || service->IsRemembered()) {
    // Keep services around if they are in a profile or have remaining
    // endpoints.
    manager_->UpdateService(service);
    return NULL;
  }

  ForgetService(service);
  manager_->DeregisterService(service);

  return service;
}

bool WiFiProvider::OnServiceUnloaded(const WiFiServiceRefPtr &service) {
  // If the service still has endpoints, it should remain in the service list.
  if (service->HasEndpoints()) {
    return false;
  }

  // This is the one place where we forget the service but do not also
  // deregister the service with the manager.  However, by returning
  // true below, the manager will do so itself.
  ForgetService(service);
  return true;
}

void WiFiProvider::FixupServiceEntries(
    StoreInterface *storage, bool is_default_profile) {
  if (WiFiService::FixupServiceEntries(storage)) {
    storage->Flush();
    Metrics::ServiceFixupProfileType profile_type =
        is_default_profile ?
            Metrics::kMetricServiceFixupDefaultProfile :
            Metrics::kMetricServiceFixupUserProfile;
    metrics_->SendEnumToUMA(
        metrics_->GetFullMetricName(Metrics::kMetricServiceFixupEntries,
                                    Technology::kWifi),
        profile_type,
        Metrics::kMetricServiceFixupMax);
  }
}

WiFiServiceRefPtr WiFiProvider::AddService(const vector<uint8_t> &ssid,
                                           const string &mode,
                                           const string &security,
                                           bool is_hidden) {
  WiFiServiceRefPtr service = new WiFiService(control_interface_,
                                              dispatcher_,
                                              metrics_,
                                              manager_,
                                              this,
                                              ssid,
                                              mode,
                                              security,
                                              is_hidden);

  services_.push_back(service);
  manager_->RegisterService(service);
  return service;
}

WiFiServiceRefPtr WiFiProvider::FindService(const vector<uint8_t> &ssid,
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

ByteArrays WiFiProvider::GetHiddenSSIDList() {
  // Create a unique set of hidden SSIDs.
  set<ByteArray> hidden_ssids_set;
  for (vector<WiFiServiceRefPtr>::const_iterator it = services_.begin();
       it != services_.end();
       ++it) {
    if ((*it)->hidden_ssid() && (*it)->IsRemembered()) {
      hidden_ssids_set.insert((*it)->ssid());
    }
  }
  SLOG(WiFi, 2) << "Found " << hidden_ssids_set.size() << " hidden services";
  return ByteArrays(hidden_ssids_set.begin(), hidden_ssids_set.end());
}

void WiFiProvider::ForgetService(const WiFiServiceRefPtr &service) {
  vector<WiFiServiceRefPtr>::iterator it;
  it = std::find(services_.begin(), services_.end(), service);
  if (it == services_.end()) {
    return;
  }
  (*it)->ResetWiFi();
  services_.erase(it);
}

// static
bool WiFiProvider::GetServiceParametersFromArgs(const KeyValueStore &args,
                                                vector<uint8_t> *ssid_bytes,
                                                string *mode,
                                                string *security_method,
                                                bool *hidden_ssid,
                                                Error *error) {
  CHECK_EQ(args.LookupString(flimflam::kTypeProperty, ""), flimflam::kTypeWifi);

  string mode_test =
      args.LookupString(flimflam::kModeProperty, flimflam::kModeManaged);
  if (!WiFiService::IsValidMode(mode_test)) {
    Error::PopulateAndLog(error, Error::kNotSupported,
                          kManagerErrorUnsupportedServiceMode);
    return false;
  }

  if (!args.ContainsString(flimflam::kSSIDProperty)) {
    Error::PopulateAndLog(error, Error::kInvalidArguments,
                          kManagerErrorSSIDRequired);
    return false;
  }

  string ssid = args.GetString(flimflam::kSSIDProperty);

  if (ssid.length() < 1) {
    Error::PopulateAndLog(error, Error::kInvalidNetworkName,
                          kManagerErrorSSIDTooShort);
    return false;
  }

  if (ssid.length() > IEEE_80211::kMaxSSIDLen) {
    Error::PopulateAndLog(error, Error::kInvalidNetworkName,
                          kManagerErrorSSIDTooLong);
    return false;
  }

  string security_method_test = args.LookupString(flimflam::kSecurityProperty,
                                                  flimflam::kSecurityNone);

  if (!WiFiService::IsValidSecurityMethod(security_method_test)) {
    Error::PopulateAndLog(error, Error::kNotSupported,
                          kManagerErrorUnsupportedSecurityMode);
    return false;
  }

  *ssid_bytes = vector<uint8_t>(ssid.begin(), ssid.end());
  *mode = mode_test;
  *security_method = security_method_test;

  // If the caller hasn't specified otherwise, we assume it is a hidden service.
  *hidden_ssid = args.LookupBool(flimflam::kWifiHiddenSsid, true);

  return true;
}

}  // namespace shill
