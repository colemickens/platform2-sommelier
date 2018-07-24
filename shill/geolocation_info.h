// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_GEOLOCATION_INFO_H_
#define SHILL_GEOLOCATION_INFO_H_

#include <map>
#include <string>
#include <vector>

#include <gtest/gtest_prod.h>  // for FRIEND_TEST

namespace shill {

class WiFiMainTest;

// This class stores properties (key-value pairs) for a single entity
// (e.g. a WiFi access point) that may be used for geolocation.
class GeolocationInfo {
 public:
  GeolocationInfo();
  ~GeolocationInfo();

  void AddField(const std::string& key, const std::string& value);
  const std::string& GetFieldValue(const std::string& key) const;

  const std::map<std::string, std::string> properties() const {
    return properties_;
  }

 private:
  FRIEND_TEST(WiFiMainTest, GetGeolocationObjects);

  // An equality testing helper for unit tests.
  bool Equals(const GeolocationInfo& info) const;

  std::map<std::string, std::string> properties_;
};

typedef std::vector<GeolocationInfo> GeolocationInfos;

}  // namespace shill

#endif  // SHILL_GEOLOCATION_INFO_H_
