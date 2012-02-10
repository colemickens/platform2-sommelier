// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_WPA_SUPPLICANT_H
#define SHILL_WPA_SUPPLICANT_H

#include <base/basictypes.h>

namespace shill {

namespace wpa_supplicant {
extern const char kBSSPropertyBSSID[];
extern const char kBSSPropertyFrequency[];
extern const char kBSSPropertyIEs[];
extern const char kBSSPropertyMode[];
extern const char kBSSPropertyRates[];
extern const char kBSSPropertySSID[];
extern const char kBSSPropertySignal[];
extern const char kCaPath[];
extern const char kCurrentBSSNull[];
extern const char kDBusAddr[];
extern const char kDBusPath[];
extern const char kDriverNL80211[];
extern const char kErrorInterfaceExists[];
extern const char kInterfacePropertyCurrentBSS[];
extern const char kInterfacePropertyState[];
extern const char kInterfaceState4WayHandshake[];
extern const char kInterfaceStateAssociated[];
extern const char kInterfaceStateAssociating[];
extern const char kInterfaceStateAuthenticating[];
extern const char kInterfaceStateCompleted[];
extern const char kInterfaceStateDisconnected[];
extern const char kInterfaceStateGroupHandshake[];
extern const char kInterfaceStateInactive[];
extern const char kInterfaceStateScanning[];
extern const char kKeyManagementMethodSuffixEAP[];
extern const char kKeyManagementMethodSuffixPSK[];
extern const char kKeyModeNone[];
extern const char kNetworkBgscanMethodLearn[];
extern const char kNetworkBgscanMethodSimple[];
extern const char kNetworkModeInfrastructure[];
extern const char kNetworkModeAdHoc[];
extern const char kNetworkModeAccessPoint[];
extern const char kNetworkPropertyBgscan[];
extern const char kNetworkPropertyCaPath[];
extern const char kNetworkPropertyEapKeyManagement[];
extern const char kNetworkPropertyEapIdentity[];
extern const char kNetworkPropertyEapEap[];
extern const char kNetworkPropertyEapInnerEap[];
extern const char kNetworkPropertyEapAnonymousIdentity[];
extern const char kNetworkPropertyEapClientCert[];
extern const char kNetworkPropertyEapPrivateKey[];
extern const char kNetworkPropertyEapPrivateKeyPassword[];
extern const char kNetworkPropertyEapCaCert[];
extern const char kNetworkPropertyEapCaPassword[];
extern const char kNetworkPropertyEapCertId[];
extern const char kNetworkPropertyEapKeyId[];
extern const char kNetworkPropertyEapCaCertId[];
extern const char kNetworkPropertyEapPin[];
extern const char kNetworkPropertyMode[];
extern const char kNetworkPropertySSID[];
extern const char kNetworkPropertyScanSSID[];
// TODO(quiche): Make the naming scheme more consistent, by adding the
// object type to the property names below. (crosbug.com/23656)
extern const char kPropertyAuthAlg[];
extern const char kPropertyBSSID[];
extern const char kPropertyMode[];
extern const char kPropertyPreSharedKey[];
extern const char kPropertyPrivacy[];
extern const char kPropertyRSN[];
extern const char kPropertyScanSSIDs[];
extern const char kPropertyScanType[];
extern const char kPropertySecurityProtocol[];
extern const char kPropertySignal[];
extern const char kPropertyWEPKey[];
extern const char kPropertyWEPTxKeyIndex[];
extern const char kPropertyWPA[];
extern const char kScanTypeActive[];
extern const char kSecurityAuthAlg[];
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
