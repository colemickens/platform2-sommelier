// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/geolocation_info.h"

#include <string>

namespace shill {

using std::string;

GeolocationInfo::GeolocationInfo() {
}

GeolocationInfo::~GeolocationInfo() {
}

void GeolocationInfo::AddField(const string& key,
                               const string& value) {
  properties_[key] = value;
}

const string& GeolocationInfo::GetFieldValue(
    const string& key) const {
  return properties_.find(key)->second;
}

bool GeolocationInfo::Equals(const GeolocationInfo& info) const {
  return properties_ == info.properties_;
}

}  // namespace shill
