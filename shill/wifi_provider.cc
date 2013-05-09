// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/wifi_provider.h"

#include <stdlib.h>

#include <limits>
#include <set>
#include <string>
#include <vector>

#include <base/bind.h>
#include <base/format_macros.h>
#include <base/string_number_conversions.h>
#include <base/string_split.h>
#include <base/string_util.h>

#include "shill/error.h"
#include "shill/event_dispatcher.h"
#include "shill/ieee80211.h"
#include "shill/key_value_store.h"
#include "shill/logging.h"
#include "shill/manager.h"
#include "shill/metrics.h"
#include "shill/profile.h"
#include "shill/shill_time.h"
#include "shill/store_interface.h"
#include "shill/technology.h"
#include "shill/wifi_endpoint.h"
#include "shill/wifi_service.h"

using base::Bind;
using base::SplitString;
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
const char WiFiProvider::kFrequencyDelimiter = ':';
const char WiFiProvider::kStartWeekHeader[] = "@";
const time_t WiFiProvider::kIllegalStartWeek =
    std::numeric_limits<time_t>::max();
const char WiFiProvider::kStorageId[] = "provider_of_wifi";
const char WiFiProvider::kStorageFrequencies[] = "Frequencies";
const int WiFiProvider::kMaxStorageFrequencies = 20;
const time_t WiFiProvider::kWeeksToKeepFrequencyCounts = 3;
const time_t WiFiProvider::kSecondsPerWeek = 60 * 60 * 24 * 7;

WiFiProvider::WiFiProvider(ControlInterface *control_interface,
                           EventDispatcher *dispatcher,
                           Metrics *metrics,
                           Manager *manager)
    : control_interface_(control_interface),
      dispatcher_(dispatcher),
      metrics_(metrics),
      manager_(manager),
      running_(false),
      total_frequency_connections_(-1L),
      time_(Time::GetInstance()) {}

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
  service_by_endpoint_.clear();
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
    manager_->RequestScan(Device::kProgressiveScan, flimflam::kTypeWifi,
                          &unused_error);
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
  EndpointServiceMap::iterator service_it =
      service_by_endpoint_.find(endpoint);
  if (service_it == service_by_endpoint_.end())
    return NULL;
  return service_it->second;
}

void WiFiProvider::OnEndpointAdded(const WiFiEndpointConstRefPtr &endpoint) {
  if (!running_) {
    return;
  }

  WiFiServiceRefPtr service = FindService(endpoint->ssid(),
                                          endpoint->network_mode(),
                                          endpoint->security_mode());
  if (!service) {
    const bool hidden_ssid = false;
    service = AddService(
        endpoint->ssid(),
        endpoint->network_mode(),
        WiFiService::GetSecurityClass(endpoint->security_mode()),
        hidden_ssid);
  }

  service->AddEndpoint(endpoint);
  service_by_endpoint_[endpoint] = service;

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
  service_by_endpoint_.erase(endpoint);

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

void WiFiProvider::OnEndpointUpdated(const WiFiEndpointConstRefPtr &endpoint) {
  WiFiService *service = FindServiceForEndpoint(endpoint);
  CHECK(service);

  // If the service still matches the endpoint in its new configuration,
  // we need only to update the service.
  if (service->ssid() == endpoint->ssid() &&
      service->mode() == endpoint->network_mode() &&
      service->IsSecurityMatch(endpoint->security_mode())) {
    service->NotifyEndpointUpdated(endpoint);
    return;
  }

  // The endpoint no longer matches the associated service.  Remove the
  // endpoint, so current references to the endpoint are reset, then add
  // it again so it can be associated with a new service.
  OnEndpointRemoved(endpoint);
  OnEndpointAdded(endpoint);
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

void WiFiProvider::LoadAndFixupServiceEntries(
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
  // TODO(wdg): Determine how this should be structured for, currently
  // non-existant, autotests.  |kStorageFrequencies| should only exist in the
  // default profile except for autotests where a test_profile is pushed.  This
  // may need to be modified for that case.
  if (is_default_profile) {
    COMPILE_ASSERT(kMaxStorageFrequencies > kWeeksToKeepFrequencyCounts,
                   persistently_storing_more_frequencies_than_we_can_hold);
    total_frequency_connections_ = 0L;
    connect_count_by_frequency_.clear();
    time_t this_week = time_->GetSecondsSinceEpoch() / kSecondsPerWeek;
    for (int freq = 0; freq < kMaxStorageFrequencies; ++freq) {
      ConnectFrequencyMap connect_count_by_frequency;
      string freq_string = StringPrintf("%s%d", kStorageFrequencies, freq);
      vector<string> frequencies;
      if (!storage->GetStringList(kStorageId, freq_string, &frequencies)) {
        SLOG(WiFi, 7) << "Frequency list " << freq_string << " not found";
        break;
      }
      time_t start_week = StringListToFrequencyMap(frequencies,
                                                  &connect_count_by_frequency);
      if (start_week == kIllegalStartWeek) {
        continue;  // |StringListToFrequencyMap| will have output an error msg.
      }

      if (start_week > this_week) {
        LOG(WARNING) << "Discarding frequency count info from the future";
        continue;
      }
      connect_count_by_frequency_dated_[start_week] =
          connect_count_by_frequency;

      for (const auto &freq_count :
           connect_count_by_frequency_dated_[start_week]) {
        connect_count_by_frequency_[freq_count.first] += freq_count.second;
        total_frequency_connections_ += freq_count.second;
      }
    }
    SLOG(WiFi, 7) << __func__ << " - total count="
                  << total_frequency_connections_;
  }
}

bool WiFiProvider::Save(StoreInterface *storage) const {
  int freq = 0;
  // Iterating backwards since I want to make sure that I get the newest data.
  ConnectFrequencyMapDated::const_reverse_iterator freq_count;
  for (freq_count = connect_count_by_frequency_dated_.crbegin();
       freq_count != connect_count_by_frequency_dated_.crend();
       ++freq_count) {
    vector<string> frequencies;
    FrequencyMapToStringList(freq_count->first, freq_count->second,
                             &frequencies);
    string freq_string = StringPrintf("%s%d", kStorageFrequencies, freq);
    storage->SetStringList(kStorageId, freq_string, frequencies);
    if (++freq >= kMaxStorageFrequencies) {
      LOG(WARNING) << "Internal frequency count list has more entries than the "
                   << "string list we had allocated for it.";
      break;
    }
  }
  return true;
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

// static
time_t WiFiProvider::StringListToFrequencyMap(const vector<string> &strings,
                                            ConnectFrequencyMap *numbers) {
  if (!numbers) {
    LOG(ERROR) << "Null |numbers| parameter";
    return kIllegalStartWeek;
  }

  // Extract the start week from the first string.
  vector<string>::const_iterator strings_it = strings.begin();
  if (strings_it == strings.end()) {
    SLOG(WiFi, 7) << "Empty |strings|.";
    return kIllegalStartWeek;
  }
  time_t start_week = GetStringListStartWeek(*strings_it);
  if (start_week == kIllegalStartWeek) {
    return kIllegalStartWeek;
  }

  // Extract the frequency:count values from the remaining strings.
  for (++strings_it; strings_it != strings.end(); ++strings_it) {
    ParseStringListFreqCount(*strings_it, numbers);
  }
  return start_week;
}

// static
time_t WiFiProvider::GetStringListStartWeek(const string &week_string) {
  if (!StartsWithASCII(week_string, kStartWeekHeader, false)) {
    LOG(ERROR) << "Found no leading '" << kStartWeekHeader << "' in '"
               << week_string << "'";
    return kIllegalStartWeek;
  }
  return atoll(week_string.c_str() + 1);
}

// static
void WiFiProvider::ParseStringListFreqCount(const string &freq_count_string,
                                            ConnectFrequencyMap *numbers) {
  vector<string> freq_count;
  SplitString(freq_count_string, kFrequencyDelimiter, &freq_count);
  if (freq_count.size() != 2) {
    LOG(WARNING) << "Found " << freq_count.size() - 1 << " '"
                 << kFrequencyDelimiter << "' in '" << freq_count_string
                 << "'.  Expected 1.";
    return;
  }
  uint16 freq = atoi(freq_count[0].c_str());
  uint64 connections = atoll(freq_count[1].c_str());
  (*numbers)[freq] = connections;
}

// static
void WiFiProvider::FrequencyMapToStringList(time_t start_week,
                                            const ConnectFrequencyMap &numbers,
                                            vector<string> *strings) {
  if (!strings) {
    LOG(ERROR) << "Null |strings| parameter";
    return;
  }

  strings->push_back(StringPrintf("%s%" PRIu64, kStartWeekHeader,
                                  static_cast<uint64_t>(start_week)));

  for (const auto &freq_conn : numbers) {
    // Use base::Int64ToString() instead of using something like "%llu"
    // (not correct for native 64 bit architectures) or PRId64 (does not
    // work correctly using cros_workon_make due to include intricacies).
    strings->push_back(StringPrintf("%u%c%s",
        freq_conn.first, kFrequencyDelimiter,
        base::Int64ToString(freq_conn.second).c_str()));
  }
}

void WiFiProvider::IncrementConnectCount(uint16 frequency_mhz) {
  CHECK(total_frequency_connections_ < std::numeric_limits<int64_t>::max());

  ++connect_count_by_frequency_[frequency_mhz];
  ++total_frequency_connections_;

  time_t this_week = time_->GetSecondsSinceEpoch() / kSecondsPerWeek;
  ++connect_count_by_frequency_dated_[this_week][frequency_mhz];

  ConnectFrequencyMapDated::iterator oldest =
      connect_count_by_frequency_dated_.begin();
  time_t oldest_legal_week = this_week - kWeeksToKeepFrequencyCounts;
  while (oldest->first < oldest_legal_week) {
    SLOG(WiFi, 7) << "Discarding frequency count info that's "
                  << this_week - oldest->first << " weeks old";
    for (const auto &freq_count : oldest->second) {
      connect_count_by_frequency_[freq_count.first] -= freq_count.second;
      if (connect_count_by_frequency_[freq_count.first] <= 0) {
        connect_count_by_frequency_.erase(freq_count.first);
      }
      total_frequency_connections_ -= freq_count.second;
    }
    connect_count_by_frequency_dated_.erase(oldest);
    oldest = connect_count_by_frequency_dated_.begin();
  }

  manager_->UpdateWiFiProvider();
  metrics_->SendToUMA(
      Metrics::kMetricFrequenciesConnectedEver,
      connect_count_by_frequency_.size(),
      Metrics::kMetricFrequenciesConnectedMin,
      Metrics::kMetricFrequenciesConnectedMax,
      Metrics::kMetricFrequenciesConnectedNumBuckets);
}

WiFiProvider::FrequencyCountList WiFiProvider::GetScanFrequencies() const {
  FrequencyCountList freq_connects_list;
  for (const auto freq_count : connect_count_by_frequency_) {
    freq_connects_list.push_back(FrequencyCount(freq_count.first,
                                                freq_count.second));
  }
  return freq_connects_list;
}

}  // namespace shill
