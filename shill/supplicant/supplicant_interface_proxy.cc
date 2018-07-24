// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/supplicant/supplicant_interface_proxy.h"

#include <map>
#include <string>

#include <dbus-c++/dbus.h>

#include "shill/dbus_properties.h"
#include "shill/logging.h"
#include "shill/supplicant/supplicant_event_delegate_interface.h"
#include "shill/supplicant/wpa_supplicant.h"

using std::map;
using std::string;

namespace shill {

namespace Logging {
static auto kModuleLogScope = ScopeLogger::kDBus;
static string ObjectID(const DBus::Path* p) { return *p; }
}

SupplicantInterfaceProxy::SupplicantInterfaceProxy(
    SupplicantEventDelegateInterface* delegate,
    DBus::Connection* bus,
    const ::DBus::Path& object_path,
    const char* dbus_addr)
    : proxy_(delegate, bus, object_path, dbus_addr) {}

SupplicantInterfaceProxy::~SupplicantInterfaceProxy() {}

bool SupplicantInterfaceProxy::AddNetwork(const KeyValueStore& args,
                                          string* network) {
  SLOG(&proxy_.path(), 2) << __func__;
  DBusPropertiesMap dbus_args;
  DBusProperties::ConvertKeyValueStoreToMap(args, &dbus_args);
  try {
    *network = proxy_.AddNetwork(dbus_args);
  } catch (const DBus::Error& e) {
    LOG(ERROR) << "DBus exception: " << e.name() << ": " << e.what()
               << " args keys are: " << DBusProperties::KeysToString(dbus_args);
    return false;
  }
  return true;
}

bool SupplicantInterfaceProxy::EnableHighBitrates() {
  SLOG(&proxy_.path(), 2) << __func__;
  try {
    proxy_.EnableHighBitrates();
  } catch (const DBus::Error& e) {
    LOG(ERROR) << "DBus exception: " << e.name() << ": " << e.what();
    return false;
  }
  return true;
}

bool SupplicantInterfaceProxy::EAPLogoff() {
  SLOG(&proxy_.path(), 2) << __func__;
  try {
    proxy_.EAPLogoff();
  } catch (const DBus::Error& e) {
    LOG(ERROR) << "DBus exception: " << e.name() << ": " << e.what();
    return false;
  }
  return true;
}

bool SupplicantInterfaceProxy::EAPLogon() {
  SLOG(&proxy_.path(), 2) << __func__;
  try {
    proxy_.EAPLogon();
  } catch (const DBus::Error& e) {
    LOG(ERROR) << "DBus exception: " << e.name() << ": " << e.what();
    return false;
  }
  return true;
}

bool SupplicantInterfaceProxy::Disconnect() {
  SLOG(&proxy_.path(), 2) << __func__;
  try {
    proxy_.Disconnect();
  } catch (const DBus::Error& e) {
    LOG(ERROR) << "DBus exception: " << e.name() << ": " << e.what();
    return false;
  }
  return true;
}

bool SupplicantInterfaceProxy::FlushBSS(const uint32_t& age) {
  SLOG(&proxy_.path(), 2) << __func__;
  try {
    proxy_.FlushBSS(age);
  } catch (const DBus::Error& e) {
    LOG(FATAL) << "DBus exception: " << e.name() << ": " << e.what()
               << " age: " << age;
    return false;  // Make the compiler happy.
  }
  return true;
}

bool SupplicantInterfaceProxy::NetworkReply(const string& network,
                                            const string& field,
                                            const string& value) {
  SLOG(&proxy_.path(), 2) << __func__;
  try {
    proxy_.NetworkReply(::DBus::Path(network), field, value);
  } catch (const DBus::Error& e) {
    LOG(ERROR) << "DBus exception: " << e.name() << ": " << e.what();
    return false;
  }
  return true;
}

bool SupplicantInterfaceProxy::Roam(const string& addr) {
  SLOG(&proxy_.path(), 2) << __func__;
  try {
    proxy_.Roam(addr);
  } catch (const DBus::Error& e) {
    LOG(ERROR) << "DBus exception: " << e.name() << ": " << e.what();
    return false;
  }
  return true;
}

bool SupplicantInterfaceProxy::Reassociate() {
  SLOG(&proxy_.path(), 2) << __func__;
  try {
    proxy_.Reassociate();
  } catch (const DBus::Error& e) {
    LOG(ERROR) << "DBus exception: " << e.name() << ": " << e.what();
    return false;
  }
  return true;
}

bool SupplicantInterfaceProxy::Reattach() {
  SLOG(&proxy_.path(), 2) << __func__;
  try {
    proxy_.Reattach();
  } catch (const DBus::Error& e) {
    LOG(ERROR) << "DBus exception: " << e.name() << ": " << e.what();
    return false;
  }
  return true;
}

bool SupplicantInterfaceProxy::RemoveAllNetworks() {
  SLOG(&proxy_.path(), 2) << __func__;
  try {
    proxy_.RemoveAllNetworks();
  } catch (const DBus::Error& e) {
    LOG(FATAL) << "DBus exception: " << e.name() << ": " << e.what();
    return false;  // Make the compiler happy.
  }
  return true;
}

bool SupplicantInterfaceProxy::RemoveNetwork(const string& network) {
  SLOG(&proxy_.path(), 2) << __func__;
  try {
    proxy_.RemoveNetwork(::DBus::Path(network));
  } catch (const DBus::Error& e) {
    LOG(ERROR) << "DBus exception: " << e.name() << ": " << e.what();
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

bool SupplicantInterfaceProxy::Scan(const KeyValueStore& args) {
  SLOG(&proxy_.path(), 2) << __func__;
  DBusPropertiesMap dbus_args;
  DBusProperties::ConvertKeyValueStoreToMap(args, &dbus_args);
  try {
    proxy_.Scan(dbus_args);
  } catch (const DBus::Error& e) {
    LOG(ERROR) << "DBus exception: " << e.name() << ": " << e.what()
               << " args keys are: " << DBusProperties::KeysToString(dbus_args);
    return false;
  }
  return true;
}

bool SupplicantInterfaceProxy::SelectNetwork(const string& network) {
  SLOG(&proxy_.path(), 2) << __func__;
  try {
    proxy_.SelectNetwork(::DBus::Path(network));
  } catch (const DBus::Error& e) {
    LOG(FATAL) << "DBus exception: " << e.name() << ": " << e.what();
    return false;  // Make the compiler happy.
  }
  return true;
}

bool SupplicantInterfaceProxy::SetHT40Enable(const string& network,
                                             bool enable) {
  SLOG(&proxy_.path(), 2) << __func__;
  try {
    proxy_.SetHT40Enable(::DBus::Path(network), enable);
  } catch (const DBus::Error& e) {
    LOG(FATAL) << "DBus exception: " << e.name() << ": " << e.what();
    return false;  // Make the compiler happy.
  }
  return true;
}

bool SupplicantInterfaceProxy::SetFastReauth(bool enabled) {
  SLOG(&proxy_.path(), 2) << __func__;
  try {
    proxy_.FastReauth(enabled);
  } catch (const DBus::Error& e) {
    LOG(ERROR) << "DBus exception: " << e.name() << ": " << e.what()
               << "enabled: " << enabled;
    return false;  // Make the compiler happy.
  }
  return true;
}

bool SupplicantInterfaceProxy::SetRoamThreshold(uint16_t threshold) {
  SLOG(&proxy_.path(), 2) << __func__;
  try {
    proxy_.RoamThreshold(threshold);
  } catch (const DBus::Error& e) {
    LOG(ERROR) << "DBus exception: " << e.name() << ": " << e.what()
               << " threshold: " << threshold;
    return false;
  }
  return true;
}

bool SupplicantInterfaceProxy::SetScanInterval(int32_t scan_interval) {
  SLOG(&proxy_.path(), 2) << __func__;
  try {
    proxy_.ScanInterval(scan_interval);
  } catch (const DBus::Error& e) {
    LOG(ERROR) << "DBus exception: " << e.name() << ": " << e.what()
               << " scan interval: " << scan_interval;
    return false;
  }
  return true;
}

bool SupplicantInterfaceProxy::SetDisableHighBitrates(
    bool disable_high_bitrates) {
  SLOG(&proxy_.path(), 2) << __func__;
  try {
    proxy_.DisableHighBitrates(disable_high_bitrates);
  } catch (const DBus::Error& e) {
    LOG(ERROR) << "DBus exception: " << e.name() << ": " << e.what()
               << "disable_high_bitrates: " << disable_high_bitrates;
    return false;
  }
  return true;
}

bool SupplicantInterfaceProxy::SetSchedScan(bool enable) {
  SLOG(&proxy_.path(), 2) << __func__;
  try {
    proxy_.SchedScan(enable);
  } catch (const DBus::Error& e) {
    LOG(ERROR) << "DBus exception: " << e.name() << ": " << e.what()
               << "enable: " << enable;
    return false;
  }
  return true;
}

bool SupplicantInterfaceProxy::SetScan(bool enable) {
  SLOG(&proxy_.path(), 2) << __func__;
  try {
    proxy_.Scan(enable);
  } catch (const DBus::Error& e) {
    LOG(ERROR) << "DBus exception: " << e.name() << ": " << e.what()
               << "enable: " << enable;
    return false;
  }
  return true;
}

bool SupplicantInterfaceProxy::TDLSDiscover(const string& peer) {
  SLOG(&proxy_.path(), 2) << __func__;
  try {
    proxy_.TDLSDiscover(peer);
  } catch (const DBus::Error& e) {
    LOG(ERROR) << "DBus exception: " << e.name() << ": " << e.what();
    return false;
  }
  return true;
}

bool SupplicantInterfaceProxy::TDLSSetup(const string& peer) {
  SLOG(&proxy_.path(), 2) << __func__;
  try {
    proxy_.TDLSSetup(peer);
  } catch (const DBus::Error& e) {
    LOG(ERROR) << "DBus exception: " << e.name() << ": " << e.what();
    return false;
  }
  return true;
}

bool SupplicantInterfaceProxy::TDLSStatus(const string& peer, string* status) {
  SLOG(&proxy_.path(), 2) << __func__;
  try {
    *status = proxy_.TDLSStatus(peer);
  } catch (const DBus::Error& e) {
    LOG(ERROR) << "DBus exception: " << e.name() << ": " << e.what();
    return false;
  }
  return true;
}

bool SupplicantInterfaceProxy::TDLSTeardown(const string& peer) {
  SLOG(&proxy_.path(), 2) << __func__;
  try {
    proxy_.TDLSTeardown(peer);
  } catch (const DBus::Error& e) {
    LOG(ERROR) << "DBus exception: " << e.name() << ": " << e.what();
    return false;
  }
  return true;
}

// definitions for private class SupplicantInterfaceProxy::Proxy

SupplicantInterfaceProxy::Proxy::Proxy(
    SupplicantEventDelegateInterface* delegate, DBus::Connection* bus,
    const DBus::Path& dbus_path, const char* dbus_addr)
    : DBus::ObjectProxy(*bus, dbus_path, dbus_addr),
      delegate_(delegate) {}

SupplicantInterfaceProxy::Proxy::~Proxy() {}

void SupplicantInterfaceProxy::Proxy::BlobAdded(const string& /*blobname*/) {
  SLOG(&path(), 2) << __func__;
  // XXX
}

void SupplicantInterfaceProxy::Proxy::BlobRemoved(const string& /*blobname*/) {
  SLOG(&path(), 2) << __func__;
  // XXX
}

void SupplicantInterfaceProxy::Proxy::BSSAdded(
    const ::DBus::Path& BSS,
    const std::map<string, ::DBus::Variant>& properties) {
  SLOG(&path(), 2) << __func__;
  KeyValueStore properties_store;
  Error error;
  DBusProperties::ConvertMapToKeyValueStore(properties,
                                            &properties_store,
                                            &error);
  if (error.IsFailure()) {
    LOG(ERROR) << "Failed to parse BSS properties";
    return;
  }
  delegate_->BSSAdded(BSS, properties_store);
}

void SupplicantInterfaceProxy::Proxy::Certification(
    const std::map<string, ::DBus::Variant>& properties) {
  SLOG(&path(), 2) << __func__;
  KeyValueStore properties_store;
  Error error;
  DBusProperties::ConvertMapToKeyValueStore(properties,
                                            &properties_store,
                                            &error);
  if (error.IsFailure()) {
    LOG(ERROR) << "Failed to parse Certification properties";
    return;
  }
  delegate_->Certification(properties_store);
}

void SupplicantInterfaceProxy::Proxy::EAP(
    const string& status, const string& parameter) {
  SLOG(&path(), 2) << __func__ << ": status " << status
                << ", parameter " << parameter;
  delegate_->EAPEvent(status, parameter);
}

void SupplicantInterfaceProxy::Proxy::BSSRemoved(const ::DBus::Path& BSS) {
  SLOG(&path(), 2) << __func__;
  delegate_->BSSRemoved(BSS);
}

void SupplicantInterfaceProxy::Proxy::NetworkAdded(
    const ::DBus::Path& /*network*/,
    const std::map<string, ::DBus::Variant>& /*properties*/) {
  SLOG(&path(), 2) << __func__;
  // XXX
}

void SupplicantInterfaceProxy::Proxy::NetworkRemoved(
    const ::DBus::Path& /*network*/) {
  SLOG(&path(), 2) << __func__;
  // TODO(quiche): Pass this up to the delegate, so that it can clean its
  // rpcid_by_service_ map. crbug.com/207648
}

void SupplicantInterfaceProxy::Proxy::NetworkSelected(
    const ::DBus::Path& /*network*/) {
  SLOG(&path(), 2) << __func__;
  // XXX
}

void SupplicantInterfaceProxy::Proxy::PropertiesChanged(
    const std::map<string, ::DBus::Variant>& properties) {
  SLOG(&path(), 2) << __func__;
  KeyValueStore properties_store;
  Error error;
  DBusProperties::ConvertMapToKeyValueStore(properties,
                                            &properties_store,
                                            &error);
  if (error.IsFailure()) {
    LOG(ERROR) << "Error encountered while parsing properties";
    return;
  }
  delegate_->PropertiesChanged(properties_store);
}

void SupplicantInterfaceProxy::Proxy::ScanDone(const bool& success) {
  SLOG(&path(), 2) << __func__ << ": " << success;
  delegate_->ScanDone(success);
}

void SupplicantInterfaceProxy::Proxy::TDLSDiscoverResponse(
    const std::string& peer_address) {
  SLOG(&path(), 2) << __func__ << ": " << peer_address;
  delegate_->TDLSDiscoverResponse(peer_address);
}

}  // namespace shill
