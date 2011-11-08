// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_WPA_SUPPLICANT_H
#define SHILL_WPA_SUPPLICANT_H

#include <base/basictypes.h>

namespace shill {

namespace wpa_supplicant {
extern const char kBSSPropertyBSSID[];
extern const char kBSSPropertySSID[];
extern const char kBSSPropertyMode[];
extern const char kBSSPropertySignal[];
extern const char kDBusAddr[];
extern const char kDBusPath[];
extern const char kDriverNL80211[];
extern const char kErrorInterfaceExists[];
extern const char kKeyManagementMethodSuffixEAP[];
extern const char kKeyManagementMethodSuffixPSK[];
extern const char kKeyModeNone[];
extern const char kNetworkModeInfrastructure[];
extern const char kNetworkModeAdHoc[];
extern const char kNetworkModeAccessPoint[];
extern const char kNetworkPropertyMode[];
extern const char kNetworkPropertySSID[];
extern const char kPropertyBSSID[];
extern const char kPropertyKeyManagement[];
extern const char kPropertyMode[];
extern const char kPropertyPreSharedKey[];
extern const char kPropertyPrivacy[];
extern const char kPropertyRSN[];
extern const char kPropertyScanSSIDs[];
extern const char kPropertyScanType[];
extern const char kPropertySecurityProtocol[];
extern const char kPropertySignal[];
extern const char kPropertyWPA[];
extern const char kScanTypeActive[];
extern const char kSecurityMethodPropertyKeyManagement[];
extern const char kSecurityModeRSN[];
extern const char kSecurityModeWPA[];

extern const uint32_t kNetworkModeInfrastructureInt;
extern const uint32_t kNetworkModeAdHocInt;
extern const uint32_t kNetworkModeAccessPointInt;
extern const uint32_t kScanMaxSSIDsPerScan;
};

}  // namespace shill

#endif  // SHILL_WPA_SUPPLICANT_H
