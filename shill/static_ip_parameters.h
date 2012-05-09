// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_STATIC_IP_PARAMETERS_
#define SHILL_STATIC_IP_PARAMETERS_

#include <string>
#include <vector>

#include <base/memory/ref_counted.h>

#include "shill/ipconfig.h"
#include "shill/key_value_store.h"
#include "shill/property_store.h"

namespace shill {
class StoreInterface;

// Holder for static IP parameters.  Includes methods for reading and
// displaying values over a control API, methods for loading and
// storing this to a persistent store, as well as applying these
// parameters to an IPConfig object.
class StaticIPParameters {
 public:
  StaticIPParameters();
  virtual ~StaticIPParameters();

  // Take a property store and add static IP parameters to them.
  void PlumbPropertyStore(PropertyStore *store);

  // Load static IP parameters from a persistent store with id |storage_id|.
  void Load(StoreInterface *storage, const std::string &storage_id);

  // Save static IP parameters to a persistent store with id |storage_id|.
  void Save(StoreInterface *storage, const std::string &storage_id);

  // Apply static IP parameters to an IPConfig properties object.
  void ApplyTo(IPConfig::Properties *props) const;

  // Return whether configuration parameters contain an address property.
  bool ContainsAddress() const;

 private:
  friend class StaticIPParametersTest;
  FRIEND_TEST(DeviceTest, IPConfigUpdatedFailureWithStatic);

  struct Property {
    enum Type {
      kTypeInt32,
      kTypeString,
      // Properties of type "Strings" are stored as a comma-separated list
      // in the control interface and in the profile, but are stored as a
      // vector of strings in the IPConfig properties.
      kTypeStrings
    };

    const char *name;
    Type type;
  };

  static const char kConfigKeyPrefix[];
  static const Property kProperties[];

  // These functions try to retrieve the argument |property| out of the
  // KeyValueStore in |args_|.  If that value exists, overwrite |value_out|
  // with its contents.
  void ApplyInt(const std::string &property, int32 *value_out) const;
  void ApplyString(const std::string &property, std::string *value_out) const;
  void ApplyStrings(const std::string &property,
                    std::vector<std::string> *value_out) const;

  void ClearMappedProperty(const size_t &index, Error *error);
  int32 GetMappedInt32Property(const size_t &index, Error *error);
  std::string GetMappedStringProperty(const size_t &index, Error *error);
  void SetMappedInt32Property(
      const size_t &index, const int32 &value, Error *error);
  void SetMappedStringProperty(
      const size_t &index, const std::string &value, Error *error);

  KeyValueStore args_;

  DISALLOW_COPY_AND_ASSIGN(StaticIPParameters);
};

}  // namespace shill

#endif  // SHILL_STATIC_IP_PARAMETERS_
