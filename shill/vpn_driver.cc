// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/vpn_driver.h"

#include <base/string_util.h>
#include <chromeos/dbus/service_constants.h>

#include "shill/connection.h"
#include "shill/manager.h"
#include "shill/property_accessor.h"
#include "shill/property_store.h"
#include "shill/scope_logger.h"
#include "shill/store_interface.h"

using std::string;

namespace shill {

VPNDriver::VPNDriver(Manager *manager,
                     const Property *properties,
                     size_t property_count)
    : manager_(manager),
      properties_(properties),
      property_count_(property_count) {}

VPNDriver::~VPNDriver() {}

bool VPNDriver::Load(StoreInterface *storage, const string &storage_id) {
  SLOG(VPN, 2) << __func__;
  for (size_t i = 0; i < property_count_; i++) {
    if ((properties_[i].flags & Property::kEphemeral)) {
      continue;
    }
    const string property = properties_[i].property;
    string value;
    bool loaded = (properties_[i].flags & Property::kCrypted) ?
        storage->GetCryptedString(storage_id, property, &value) :
        storage->GetString(storage_id, property, &value);
    if (loaded) {
      args_.SetString(property, value);
    } else {
      args_.RemoveString(property);
    }
  }
  return true;
}

bool VPNDriver::Save(StoreInterface *storage, const string &storage_id) {
  SLOG(VPN, 2) << __func__;
  for (size_t i = 0; i < property_count_; i++) {
    if ((properties_[i].flags & Property::kEphemeral)) {
      continue;
    }
    const string property = properties_[i].property;
    const string value = args_.LookupString(property, "");
    if (value.empty()) {
      storage->DeleteKey(storage_id, property);
    } else if ((properties_[i].flags & Property::kCrypted)) {
      storage->SetCryptedString(storage_id, property, value);
    } else {
      storage->SetString(storage_id, property, value);
    }
  }
  return true;
}

void VPNDriver::InitPropertyStore(PropertyStore *store) {
  SLOG(VPN, 2) << __func__;
  for (size_t i = 0; i < property_count_; i++) {
    store->RegisterDerivedString(
        properties_[i].property,
        StringAccessor(
            new CustomMappedAccessor<VPNDriver, string, size_t>(
                this,
                &VPNDriver::ClearMappedProperty,
                &VPNDriver::GetMappedProperty,
                &VPNDriver::SetMappedProperty,
                i)));
  }

  store->RegisterDerivedStringmap(
      flimflam::kProviderProperty,
      StringmapAccessor(
          new CustomAccessor<VPNDriver, Stringmap>(
              this, &VPNDriver::GetProvider, NULL)));
}

void VPNDriver::ClearMappedProperty(const size_t &index, Error *error) {
  CHECK(index < property_count_);
  if (args_.ContainsString(properties_[index].property)) {
    args_.RemoveString(properties_[index].property);
  } else {
    error->Populate(Error::kNotFound, "Property is not set");
  }
}

string VPNDriver::GetMappedProperty(const size_t &index, Error *error) {
  // Provider properties are set via SetProperty calls to "Provider.XXX",
  // however, they are retrieved via a GetProperty call, which returns all
  // properties in a single "Provider" dict.  Therefore, none of the individual
  // properties in the kProperties are available for enumeration in
  // GetProperties.  Instead, they are retrieved via GetProvider below.
  error->Populate(Error::kInvalidArguments,
                  "Provider properties are not read back in this manner");
  return string();
}

void VPNDriver::SetMappedProperty(
    const size_t &index, const string &value, Error *error) {
  CHECK(index < property_count_);
  args_.SetString(properties_[index].property, value);
}

Stringmap VPNDriver::GetProvider(Error *error) {
  SLOG(VPN, 2) << __func__;
  string provider_prefix = string(flimflam::kProviderProperty) + ".";
  Stringmap provider_properties;

  for (size_t i = 0; i < property_count_; i++) {
    // Never return any encrypted properties.
    if ((properties_[i].flags & Property::kCrypted)) {
      continue;
    }

    string prop = properties_[i].property;

    // Chomp off leading "Provider." from properties that have this prefix.
    if (StartsWithASCII(prop, provider_prefix, false)) {
      prop = prop.substr(provider_prefix.length());
    }
    if (args_.ContainsString(properties_[i].property)) {
      provider_properties[prop] = args_.GetString(properties_[i].property);
    }
  }

  return provider_properties;
}

}  // namespace shill
