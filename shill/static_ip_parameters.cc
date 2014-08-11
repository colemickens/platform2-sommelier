// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/static_ip_parameters.h"

#include <string.h>

#include <base/strings/string_split.h>
#include <base/strings/string_util.h>
#include <chromeos/dbus/service_constants.h>

#include "shill/error.h"
#include "shill/ip_address.h"
#include "shill/logging.h"
#include "shill/property_accessor.h"
#include "shill/property_store.h"
#include "shill/store_interface.h"

using std::string;
using std::vector;

namespace shill {

// static
const char StaticIPParameters::kConfigKeyPrefix[] = "StaticIP.";
// static
const char StaticIPParameters::kSavedConfigKeyPrefix[] = "SavedIP.";
// static
const StaticIPParameters::Property StaticIPParameters::kProperties[] = {
  { kAddressProperty, Property::kTypeString },
  { kGatewayProperty, Property::kTypeString },
  { kMtuProperty, Property::kTypeInt32 },
  { kNameServersProperty, Property::kTypeStrings },
  { kPeerAddressProperty, Property::kTypeString },
  { kPrefixlenProperty, Property::kTypeInt32 }
};

StaticIPParameters::StaticIPParameters() {}

StaticIPParameters::~StaticIPParameters() {}

void StaticIPParameters::PlumbPropertyStore(PropertyStore *store) {
  for (size_t i = 0; i < arraysize(kProperties); ++i) {
    const Property &property = kProperties[i];
    const string name(string(kConfigKeyPrefix) + property.name);
    const string saved_name(string(kSavedConfigKeyPrefix) + property.name);
    switch (property.type) {
      case Property::kTypeInt32:
        store->RegisterDerivedInt32(
            name,
            Int32Accessor(
                new CustomMappedAccessor<StaticIPParameters, int32_t, size_t>(
                    this,
                    &StaticIPParameters::ClearMappedProperty,
                    &StaticIPParameters::GetMappedInt32Property,
                    &StaticIPParameters::SetMappedInt32Property,
                    i)));
        store->RegisterDerivedInt32(
            saved_name,
            Int32Accessor(
                new CustomMappedAccessor<StaticIPParameters, int32_t, size_t>(
                    this,
                    &StaticIPParameters::ClearMappedSavedProperty,
                    &StaticIPParameters::GetMappedSavedInt32Property,
                    &StaticIPParameters::SetMappedSavedInt32Property,
                    i)));
         break;
      case Property::kTypeString:
      case Property::kTypeStrings:
        store->RegisterDerivedString(
            name,
            StringAccessor(
                new CustomMappedAccessor<StaticIPParameters, string, size_t>(
                    this,
                    &StaticIPParameters::ClearMappedProperty,
                    &StaticIPParameters::GetMappedStringProperty,
                    &StaticIPParameters::SetMappedStringProperty,
                    i)));
        store->RegisterDerivedString(
            saved_name,
            StringAccessor(
                new CustomMappedAccessor<StaticIPParameters, string, size_t>(
                    this,
                    &StaticIPParameters::ClearMappedSavedProperty,
                    &StaticIPParameters::GetMappedSavedStringProperty,
                    &StaticIPParameters::SetMappedSavedStringProperty,
                    i)));
        break;
      default:
        NOTIMPLEMENTED();
        break;
    }
  }
}

void StaticIPParameters::Load(
    StoreInterface *storage, const string &storage_id) {
  for (size_t i = 0; i < arraysize(kProperties); ++i) {
    const Property &property = kProperties[i];
    const string name(string(kConfigKeyPrefix) + property.name);
    switch (property.type) {
      case Property::kTypeInt32:
        {
          int32_t value;
          if (storage->GetInt(storage_id, name, &value)) {
            args_.SetInt(property.name, value);
          } else {
            args_.RemoveInt(property.name);
          }
        }
        break;
      case Property::kTypeString:
      case Property::kTypeStrings:
        {
          string value;
          if (storage->GetString(storage_id, name, &value)) {
            args_.SetString(property.name, value);
          } else {
            args_.RemoveString(property.name);
          }
        }
        break;
      default:
        NOTIMPLEMENTED();
        break;
    }
  }
}

void StaticIPParameters::Save(
    StoreInterface *storage, const string &storage_id) {
  for (size_t i = 0; i < arraysize(kProperties); ++i) {
    const Property &property = kProperties[i];
    const string name(string(kConfigKeyPrefix) + property.name);
    bool property_exists = false;
    switch (property.type) {
      case Property::kTypeInt32:
        if (args_.ContainsInt(property.name)) {
          property_exists = true;
          storage->SetInt(storage_id, name, args_.GetInt(property.name));
        }
        break;
      case Property::kTypeString:
      case Property::kTypeStrings:
        if (args_.ContainsString(property.name)) {
          property_exists = true;
          storage->SetString(storage_id, name, args_.GetString(property.name));
        }
        break;
      default:
        NOTIMPLEMENTED();
        break;
    }
    if (!property_exists) {
      storage->DeleteKey(storage_id, name);
    }
  }
}

void StaticIPParameters::ApplyInt(
    const string &property, int32_t *value_out) {
  saved_args_.SetInt(property, *value_out);
  if (args_.ContainsInt(property)) {
    *value_out = args_.GetInt(property);
  }
}

void StaticIPParameters::ApplyString(
    const string &property, string *value_out) {
  saved_args_.SetString(property, *value_out);
  if (args_.ContainsString(property)) {
    *value_out = args_.GetString(property);
  }
}

void StaticIPParameters::ApplyStrings(
    const string &property, vector<string> *value_out) {
  saved_args_.SetString(property, JoinString(*value_out, ','));
  if (args_.ContainsString(property)) {
    vector<string> values;
    string value(args_.GetString(property));
    if (!value.empty()) {
      base::SplitString(value, ',', &values);
    }
    *value_out = values;
  }
}


void StaticIPParameters::ApplyTo(IPConfig::Properties *props) {
  if (props->address_family == IPAddress::kFamilyUnknown) {
    // In situations where no address is supplied (bad or missing DHCP config)
    // supply an address family ourselves.
    // TODO(pstew): Guess from the address values.
    props->address_family = IPAddress::kFamilyIPv4;
  }
  ClearSavedParameters();
  ApplyString(kAddressProperty, &props->address);
  ApplyString(kGatewayProperty, &props->gateway);
  ApplyInt(kMtuProperty, &props->mtu);
  ApplyStrings(kNameServersProperty, &props->dns_servers);
  ApplyString(kPeerAddressProperty, &props->peer_address);
  ApplyInt(kPrefixlenProperty, &props->subnet_prefix);
}

void StaticIPParameters::RestoreTo(IPConfig::Properties *props) {
  props->address = saved_args_.LookupString(kAddressProperty, "");
  props->gateway = saved_args_.LookupString(kGatewayProperty, "");
  props->mtu = saved_args_.LookupInt(kMtuProperty, 0);
  props->dns_servers.clear();
  string saved_dns_servers = saved_args_.LookupString(kNameServersProperty, "");
  if (!saved_dns_servers.empty()) {
    base::SplitString(saved_dns_servers, ',', &props->dns_servers);
  }
  props->peer_address = saved_args_.LookupString(kPeerAddressProperty, "");
  props->subnet_prefix = saved_args_.LookupInt(kPrefixlenProperty, 0);
  ClearSavedParameters();
}

void StaticIPParameters::ClearSavedParameters() {
  saved_args_.Clear();
}

bool StaticIPParameters::ContainsAddress() const {
  return args_.ContainsString(kAddressProperty) &&
      args_.ContainsInt(kPrefixlenProperty);
}

void StaticIPParameters::ClearMappedProperty(
    const size_t &index, Error *error) {
  CHECK(index < arraysize(kProperties));

  const Property &property = kProperties[index];
  switch (property.type) {
    case Property::kTypeInt32:
      if (args_.ContainsInt(property.name)) {
        args_.RemoveInt(property.name);
      } else {
        error->Populate(Error::kNotFound, "Property is not set");
      }
      break;
    case Property::kTypeString:
    case Property::kTypeStrings:
      if (args_.ContainsString(property.name)) {
        args_.RemoveString(property.name);
      } else {
        error->Populate(Error::kNotFound, "Property is not set");
      }
      break;
    default:
      NOTIMPLEMENTED();
      break;
  }
}

void StaticIPParameters::ClearMappedSavedProperty(
    const size_t &index, Error *error) {
  error->Populate(Error::kInvalidArguments, "Property is read-only");
}

int32_t StaticIPParameters::GetMappedInt32Property(
    const size_t &index, Error *error) {
  CHECK(index < arraysize(kProperties));

  const string &key = kProperties[index].name;
  if (!args_.ContainsInt(key)) {
    error->Populate(Error::kNotFound, "Property is not set");
    return 0;
  }
  return args_.GetInt(key);
}

int32_t StaticIPParameters::GetMappedSavedInt32Property(
    const size_t &index, Error *error) {
  CHECK(index < arraysize(kProperties));

  const string &key = kProperties[index].name;
  if (!saved_args_.ContainsInt(key)) {
    error->Populate(Error::kNotFound, "Property is not set");
    return 0;
  }
  return saved_args_.GetInt(key);
}

string StaticIPParameters::GetMappedStringProperty(
    const size_t &index, Error *error) {
  CHECK(index < arraysize(kProperties));

  const string &key = kProperties[index].name;
  if (!args_.ContainsString(key)) {
    error->Populate(Error::kNotFound, "Property is not set");
    return string();
  }
  return args_.GetString(key);
}

string StaticIPParameters::GetMappedSavedStringProperty(
    const size_t &index, Error *error) {
  CHECK(index < arraysize(kProperties));

  const string &key = kProperties[index].name;
  if (!saved_args_.ContainsString(key)) {
    error->Populate(Error::kNotFound, "Property is not set");
    return string();
  }
  return saved_args_.GetString(key);
}

bool StaticIPParameters::SetMappedInt32Property(
    const size_t &index, const int32_t &value, Error *error) {
  CHECK(index < arraysize(kProperties));
  if (args_.ContainsInt(kProperties[index].name) &&
      args_.GetInt(kProperties[index].name) == value) {
    return false;
  }
  args_.SetInt(kProperties[index].name, value);
  return true;
}

bool StaticIPParameters::SetMappedSavedInt32Property(
    const size_t &index, const int32_t &value, Error *error) {
  error->Populate(Error::kInvalidArguments, "Property is read-only");
  return false;
}

bool StaticIPParameters::SetMappedStringProperty(
    const size_t &index, const string &value, Error *error) {
  CHECK(index < arraysize(kProperties));
  if (args_.ContainsString(kProperties[index].name) &&
      args_.GetString(kProperties[index].name) == value) {
    return false;
  }
  args_.SetString(kProperties[index].name, value);
  return true;
}

bool StaticIPParameters::SetMappedSavedStringProperty(
    const size_t &index, const string &value, Error *error) {
  error->Populate(Error::kInvalidArguments, "Property is read-only");
  return false;
}

}  // namespace shill
