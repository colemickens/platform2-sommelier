// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/supplicant_interface_proxy.h"

#include <map>
#include <string>

#include <dbus-c++/dbus.h>

#include "shill/logging.h"
#include "shill/supplicant_event_delegate_interface.h"

using std::map;
using std::string;

namespace shill {

SupplicantInterfaceProxy::SupplicantInterfaceProxy(
    SupplicantEventDelegateInterface *delegate,
    DBus::Connection *bus,
    const ::DBus::Path &object_path,
    const char *dbus_addr)
    : proxy_(delegate, bus, object_path, dbus_addr) {}

SupplicantInterfaceProxy::~SupplicantInterfaceProxy() {}

::DBus::Path SupplicantInterfaceProxy::AddNetwork(
    const map<string, ::DBus::Variant> &args) {
  SLOG(DBus, 2) << __func__;
  try {
    return proxy_.AddNetwork(args);
  } catch (const DBus::Error &e) {
    LOG(ERROR) << "DBus exception: " << e.name() << ": " << e.what()
               << " args keys are: " << DBusProperties::KeysToString(args);
    throw;  // Re-throw the exception.
  }
}

void SupplicantInterfaceProxy::EnableHighBitrates() {
  SLOG(DBus, 2) << __func__;
  try {
    return proxy_.EnableHighBitrates();
  } catch (const DBus::Error &e) {
    LOG(ERROR) << "DBus exception: " << e.name() << ": " << e.what();
    throw;  // Re-throw the exception.
  }
}

void SupplicantInterfaceProxy::EAPLogoff() {
  SLOG(DBus, 2) << __func__;
  try {
    return proxy_.EAPLogoff();
  } catch (const DBus::Error &e) {
    LOG(ERROR) << "DBus exception: " << e.name() << ": " << e.what();
    throw;  // Re-throw the exception.
  }
}

void SupplicantInterfaceProxy::EAPLogon() {
  SLOG(DBus, 2) << __func__;
  try {
    return proxy_.EAPLogon();
  } catch (const DBus::Error &e) {
    LOG(ERROR) << "DBus exception: " << e.name() << ": " << e.what();
    throw;  // Re-throw the exception.
  }
}

void SupplicantInterfaceProxy::Disconnect() {
  SLOG(DBus, 2) << __func__;
  try {
    return proxy_.Disconnect();
  } catch (const DBus::Error &e) {
    LOG(ERROR) << "DBus exception: " << e.name() << ": " << e.what();
    throw;  // Re-throw the exception.
  }
}

void SupplicantInterfaceProxy::FlushBSS(const uint32_t &age) {
  SLOG(DBus, 2) << __func__;
  try {
    return proxy_.FlushBSS(age);
  } catch (const DBus::Error &e) {
    LOG(FATAL) << "DBus exception: " << e.name() << ": " << e.what()
               << " age: " << age;
  }
}

void SupplicantInterfaceProxy::NetworkReply(const ::DBus::Path &network,
                                            const string &field,
                                            const string &value) {
  SLOG(DBus, 2) << __func__;
  try {
    return proxy_.NetworkReply(network, field, value);
  } catch (const DBus::Error &e) {
    LOG(ERROR) << "DBus exception: " << e.name() << ": " << e.what();
    throw;  // Re-throw the exception.
  }
}

void SupplicantInterfaceProxy::Reassociate() {
  SLOG(DBus, 2) << __func__;
  try {
    return proxy_.Reassociate();
  } catch (const DBus::Error &e) {
    LOG(ERROR) << "DBus exception: " << e.name() << ": " << e.what();
    throw;  // Re-throw the exception.
  }
}

void SupplicantInterfaceProxy::Reattach() {
  SLOG(DBus, 2) << __func__;
  try {
    return proxy_.Reattach();
  } catch (const DBus::Error &e) {
    LOG(ERROR) << "DBus exception: " << e.name() << ": " << e.what();
    throw;  // Re-throw the exception.
  }
}

void SupplicantInterfaceProxy::RemoveAllNetworks() {
  SLOG(DBus, 2) << __func__;
  try {
    return proxy_.RemoveAllNetworks();
  } catch (const DBus::Error &e) {
    LOG(FATAL) << "DBus exception: " << e.name() << ": " << e.what();
  }
}

void SupplicantInterfaceProxy::RemoveNetwork(const ::DBus::Path &network) {
  SLOG(DBus, 2) << __func__;
  try {
    return proxy_.RemoveNetwork(network);
  } catch (const DBus::Error &e) {
    LOG(ERROR) << "DBus exception: " << e.name() << ": " << e.what();
    throw;  // Re-throw the exception.
  }
}

void SupplicantInterfaceProxy::Scan(const map<string, ::DBus::Variant> &args) {
  SLOG(DBus, 2) << __func__;
  try {
    return proxy_.Scan(args);
  } catch (const DBus::Error &e) {
    LOG(ERROR) << "DBus exception: " << e.name() << ": " << e.what()
               << " args keys are: " << DBusProperties::KeysToString(args);
    throw;  // Re-throw the exception.
  }
}

void SupplicantInterfaceProxy::SelectNetwork(const ::DBus::Path &network) {
  SLOG(DBus, 2) << __func__;
  try {
    return proxy_.SelectNetwork(network);
  } catch (const DBus::Error &e) {
    LOG(FATAL) << "DBus exception: " << e.name() << ": " << e.what();
  }
}

void SupplicantInterfaceProxy::SetFastReauth(bool enabled) {
  SLOG(DBus, 2) << __func__;
  try {
    return proxy_.FastReauth(enabled);
  } catch (const DBus::Error &e) {
    LOG(ERROR) << "DBus exception: " << e.name() << ": " << e.what()
               << "enabled: " << enabled;
    throw;  // Re-throw the exception.
  }
}

void SupplicantInterfaceProxy::SetRoamThreshold(uint16_t threshold) {
  SLOG(DBus, 2) << __func__;
  try {
    proxy_.RoamThreshold(threshold);
    return;
  } catch (const DBus::Error &e) {
    LOG(ERROR) << "DBus exception: " << e.name() << ": " << e.what()
               << " threshold: " << threshold;
    throw;  // Re-throw the exception.
  }
}

void SupplicantInterfaceProxy::SetScanInterval(int32_t scan_interval) {
  SLOG(DBus, 2) << __func__;
  try {
    return proxy_.ScanInterval(scan_interval);
  } catch (const DBus::Error &e) {
    LOG(ERROR) << "DBus exception: " << e.name() << ": " << e.what()
               << " scan interval: " << scan_interval;
    throw;  // Re-throw the exception.
  }
}

void SupplicantInterfaceProxy::SetDisableHighBitrates(
    bool disable_high_bitrates) {
  SLOG(DBus, 2) << __func__;
  try {
    return proxy_.DisableHighBitrates(disable_high_bitrates);
  } catch (const DBus::Error &e) {
    LOG(ERROR) << "DBus exception: " << e.name() << ": " << e.what()
               << "disable_high_bitrates: " << disable_high_bitrates;
    throw;  // Re-throw the exception.
  }
}

void SupplicantInterfaceProxy::TDLSDiscover(const string &peer) {
  SLOG(DBus, 2) << __func__;
  try {
    return proxy_.TDLSDiscover(peer);
  } catch (const DBus::Error &e) {
    LOG(ERROR) << "DBus exception: " << e.name() << ": " << e.what();
    throw;  // Re-throw the exception.
  }
}

void SupplicantInterfaceProxy::TDLSSetup(const string &peer) {
  SLOG(DBus, 2) << __func__;
  try {
    return proxy_.TDLSSetup(peer);
  } catch (const DBus::Error &e) {
    LOG(ERROR) << "DBus exception: " << e.name() << ": " << e.what();
    throw;  // Re-throw the exception.
  }
}

string SupplicantInterfaceProxy::TDLSStatus(const string &peer) {
  SLOG(DBus, 2) << __func__;
  try {
    return proxy_.TDLSStatus(peer);
  } catch (const DBus::Error &e) {
    LOG(ERROR) << "DBus exception: " << e.name() << ": " << e.what();
    throw;  // Re-throw the exception.
  }
}

void SupplicantInterfaceProxy::TDLSTeardown(const string &peer) {
  SLOG(DBus, 2) << __func__;
  try {
    return proxy_.TDLSTeardown(peer);
  } catch (const DBus::Error &e) {
    LOG(ERROR) << "DBus exception: " << e.name() << ": " << e.what();
    throw;  // Re-throw the exception.
  }
}

// definitions for private class SupplicantInterfaceProxy::Proxy

SupplicantInterfaceProxy::Proxy::Proxy(
    SupplicantEventDelegateInterface *delegate, DBus::Connection *bus,
    const DBus::Path &dbus_path, const char *dbus_addr)
    : DBus::ObjectProxy(*bus, dbus_path, dbus_addr),
      delegate_(delegate) {}

SupplicantInterfaceProxy::Proxy::~Proxy() {}

void SupplicantInterfaceProxy::Proxy::BlobAdded(const string &/*blobname*/) {
  SLOG(DBus, 2) << __func__;
  // XXX
}

void SupplicantInterfaceProxy::Proxy::BlobRemoved(const string &/*blobname*/) {
  SLOG(DBus, 2) << __func__;
  // XXX
}

void SupplicantInterfaceProxy::Proxy::BSSAdded(
    const ::DBus::Path &BSS,
    const std::map<string, ::DBus::Variant> &properties) {
  SLOG(DBus, 2) << __func__;
  delegate_->BSSAdded(BSS, properties);
}

void SupplicantInterfaceProxy::Proxy::Certification(
    const std::map<string, ::DBus::Variant> &properties) {
  SLOG(DBus, 2) << __func__;
  delegate_->Certification(properties);
}

void SupplicantInterfaceProxy::Proxy::EAP(
    const string &status, const string &parameter) {
  SLOG(DBus, 2) << __func__ << ": status " << status
                << ", parameter " << parameter;
  delegate_->EAPEvent(status, parameter);
}

void SupplicantInterfaceProxy::Proxy::BSSRemoved(const ::DBus::Path &BSS) {
  SLOG(DBus, 2) << __func__;
  delegate_->BSSRemoved(BSS);
}

void SupplicantInterfaceProxy::Proxy::NetworkAdded(
    const ::DBus::Path &/*network*/,
    const std::map<string, ::DBus::Variant> &/*properties*/) {
  SLOG(DBus, 2) << __func__;
  // XXX
}

void SupplicantInterfaceProxy::Proxy::NetworkRemoved(
    const ::DBus::Path &/*network*/) {
  SLOG(DBus, 2) << __func__;
  // TODO(quiche): Pass this up to the delegate, so that it can clean its
  // rpcid_by_service_ map. crbug.com/207648
}

void SupplicantInterfaceProxy::Proxy::NetworkSelected(
    const ::DBus::Path &/*network*/) {
  SLOG(DBus, 2) << __func__;
  // XXX
}

void SupplicantInterfaceProxy::Proxy::PropertiesChanged(
    const std::map<string, ::DBus::Variant> &properties) {
  SLOG(DBus, 2) << __func__;
  delegate_->PropertiesChanged(properties);
}

void SupplicantInterfaceProxy::Proxy::ScanDone(const bool& success) {
  SLOG(DBus, 2) << __func__ << ": " << success;
  if (success) {
    delegate_->ScanDone();
  }
}

}  // namespace shill
