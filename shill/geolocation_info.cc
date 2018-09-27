//
// Copyright (C) 2017 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include "shill/geolocation_info.h"

#include <inttypes.h>

#include <base/strings/string_number_conversions.h>
#include <base/strings/stringprintf.h>
#include <chromeos/dbus/service_constants.h>

namespace {

// This key is special, because we will look for it and transform it into
// an up-to-date age property when D-Bus calls are made asking for geolocation
// objects. It should not be exported outside of shill.
constexpr char kLastSeenKey[] = "lastSeen";

}  // namespace

namespace shill {

void AddLastSeenTime(GeolocationInfo* info, const base::TimeTicks& time) {
  if (time.is_null())
    return;

  DCHECK(info);
  (*info)[kLastSeenKey] = base::StringPrintf(
      "%" PRId64, (time - base::TimeTicks()).InSeconds());
}

GeolocationInfo PrepareGeolocationInfoForExport(const GeolocationInfo& info) {
  const auto& it = info.find(kLastSeenKey);
  if (it == info.end())
    return info;

  int64_t last_seen;
  if (!base::StringToInt64(it->second, &last_seen)) {
    DLOG(ERROR) << "Invalid last seen time: " << it->second;
    return GeolocationInfo();
  }

  // Calculate the age based on the current time. We have to
  // reconstitute last_seen into a TimeTicks so we can get a TimeDelta.
  base::TimeDelta age = base::TimeTicks::Now() -
      (base::TimeTicks() + base::TimeDelta::FromSeconds(last_seen));

  GeolocationInfo new_info(info);
  new_info.erase(kLastSeenKey);
  new_info[kGeoAgeProperty] = base::StringPrintf("%" PRId64, age.InSeconds());
  return new_info;
}

}  // namespace shill
