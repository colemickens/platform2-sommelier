// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_WPA_SUPPLICANT_H
#define SHILL_WPA_SUPPLICANT_H

#include <map>
#include <string>
#include <vector>

#include <base/basictypes.h>
#include <dbus-c++/dbus.h>

namespace shill {

class CertificateFile;
class EapCredentials;
class NSS;

class WPASupplicant {
 public:
  static const char kBSSPropertyBSSID[];
  static const char kBSSPropertyFrequency[];
  static const char kBSSPropertyIEs[];
  static const char kBSSPropertyMode[];
  static const char kBSSPropertyRates[];
  static const char kBSSPropertySSID[];
  static const char kBSSPropertySignal[];
  static const char kCaPath[];
  static const char kCurrentBSSNull[];
  static const char kDBusAddr[];
  static const char kDBusPath[];
  static const char kDebugLevelDebug[];
  static const char kDebugLevelError[];
  static const char kDebugLevelExcessive[];
  static const char kDebugLevelInfo[];
  static const char kDebugLevelMsgDump[];
  static const char kDebugLevelWarning[];
  static const char kDriverNL80211[];
  static const char kEAPParameterAlertUnknownCA[];
  static const char kEAPParameterFailure[];
  static const char kEAPParameterSuccess[];
  static const char kEAPStatusAcceptProposedMethod[];
  static const char kEAPStatusCompletion[];
  static const char kEAPStatusLocalTLSAlert[];
  static const char kEAPStatusParameterNeeded[];
  static const char kEAPStatusRemoteCertificateVerification[];
  static const char kEAPStatusRemoteTLSAlert[];
  static const char kEAPStatusStarted[];
  static const char kEnginePKCS11[];
  static const char kErrorNetworkUnknown[];
  static const char kErrorInterfaceExists[];
  static const char kInterfacePropertyConfigFile[];
  static const char kInterfacePropertyCurrentBSS[];
  static const char kInterfacePropertyDepth[];
  static const char kInterfacePropertyDriver[];
  static const char kInterfacePropertyName[];
  static const char kInterfacePropertyState[];
  static const char kInterfacePropertySubject[];
  static const char kInterfaceState4WayHandshake[];
  static const char kInterfaceStateAssociated[];
  static const char kInterfaceStateAssociating[];
  static const char kInterfaceStateAuthenticating[];
  static const char kInterfaceStateCompleted[];
  static const char kInterfaceStateDisconnected[];
  static const char kInterfaceStateGroupHandshake[];
  static const char kInterfaceStateInactive[];
  static const char kInterfaceStateScanning[];
  static const char kKeyManagementMethodSuffixEAP[];
  static const char kKeyManagementMethodSuffixPSK[];
  static const char kKeyModeNone[];
  static const char kNetworkBgscanMethodLearn[];
// None is not a real method name, but we interpret 'none' as a request that
// no background scan parameter should be supplied to wpa_supplicant.
  static const char kNetworkBgscanMethodNone[];
  static const char kNetworkBgscanMethodSimple[];
  static const char kNetworkModeInfrastructure[];
  static const char kNetworkModeAdHoc[];
  static const char kNetworkModeAccessPoint[];
  static const char kNetworkPropertyBgscan[];
  static const char kNetworkPropertyCaPath[];
  static const char kNetworkPropertyEapKeyManagement[];
  static const char kNetworkPropertyEapIdentity[];
  static const char kNetworkPropertyEapEap[];
  static const char kNetworkPropertyEapInnerEap[];
  static const char kNetworkPropertyEapAnonymousIdentity[];
  static const char kNetworkPropertyEapClientCert[];
  static const char kNetworkPropertyEapPrivateKey[];
  static const char kNetworkPropertyEapPrivateKeyPassword[];
  static const char kNetworkPropertyEapCaCert[];
  static const char kNetworkPropertyEapCaPassword[];
  static const char kNetworkPropertyEapCertId[];
  static const char kNetworkPropertyEapKeyId[];
  static const char kNetworkPropertyEapCaCertId[];
  static const char kNetworkPropertyEapPin[];
  static const char kNetworkPropertyEapSubjectMatch[];
  static const char kNetworkPropertyEngine[];
  static const char kNetworkPropertyEngineId[];
  static const char kNetworkPropertyFrequency[];
  static const char kNetworkPropertyIeee80211w[];
  static const char kNetworkPropertyMode[];
  static const char kNetworkPropertySSID[];
  static const char kNetworkPropertyScanSSID[];
// TODO(quiche): Make the naming scheme more consistent, by adding the
// object type to the property names below. (crosbug.com/23656)
  static const char kPropertyAuthAlg[];
  static const char kPropertyBSSID[];
  static const char kPropertyMode[];
  static const char kPropertyPreSharedKey[];
  static const char kPropertyPrivacy[];
  static const char kPropertyRSN[];
  static const char kPropertyScanSSIDs[];
  static const char kPropertyScanType[];
  static const char kPropertySecurityProtocol[];
  static const char kPropertySignal[];
  static const char kPropertyWEPKey[];
  static const char kPropertyWEPTxKeyIndex[];
  static const char kPropertyWPA[];
  static const char kScanTypeActive[];
  static const char kSecurityAuthAlg[];
  static const char kSecurityMethodPropertyKeyManagement[];
  static const char kSecurityModeRSN[];
  static const char kSecurityModeWPA[];

  static const uint32_t kDefaultEngine;
  static const uint32_t kNetworkIeee80211wDisabled;
  static const uint32_t kNetworkIeee80211wEnabled;
  static const uint32_t kNetworkIeee80211wRequired;
  static const uint32_t kNetworkModeInfrastructureInt;
  static const uint32_t kNetworkModeAdHocInt;
  static const uint32_t kNetworkModeAccessPointInt;
  static const uint32_t kScanMaxSSIDsPerScan;

  // Populate the wpa_supplicant DBus parameter map |params| with the
  // credentials in |eap|.  To do so, this function may use |certificate_file|
  // or |nss| to export CA certificates to be passed to wpa_supplicant.
  static void Populate8021xProperties(
      const EapCredentials &eap, CertificateFile *certificate_file,
      NSS *nss, const std::vector<char> nss_identifier,
      std::map<std::string, DBus::Variant> *params);
};

}  // namespace shill

#endif  // SHILL_WPA_SUPPLICANT_H
