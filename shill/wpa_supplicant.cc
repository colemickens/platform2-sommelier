// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/wpa_supplicant.h"

namespace shill {

namespace wpa_supplicant {
const char kBSSPropertyBSSID[]      = "BSSID";
const char kBSSPropertyFrequency[]  = "Frequency";
const char kBSSPropertyIEs[]        = "IEs";
const char kBSSPropertyMode[]       = "Mode";
const char kBSSPropertyRates[]      = "Rates";
const char kBSSPropertySSID[]       = "SSID";
const char kBSSPropertySignal[]     = "Signal";
// TODO(gauravsh): Make this path be a configurable option. crosbug.com/25661
// Location of the system root CA certificates.
const char kCaPath[] = "/etc/ssl/certs";
const char kCurrentBSSNull[] = "/";
const char kDBusAddr[]              = "fi.w1.wpa_supplicant1";
const char kDBusPath[]              = "/fi/w1/wpa_supplicant1";
const char kDriverNL80211[]         = "nl80211";
const char kEnginePKCS11[]          = "pkcs11";
const char kErrorInterfaceExists[]  = "fi.w1.wpa_supplicant1.InterfaceExists";
const char kInterfacePropertyConfigFile[] = "ConfigFile";
const char kInterfacePropertyCurrentBSS[] = "CurrentBSS";
const char kInterfacePropertyDepth[] = "depth";
const char kInterfacePropertyDriver[] = "Driver";
const char kInterfacePropertyName[] = "Ifname";
const char kInterfacePropertyState[] = "State";
const char kInterfacePropertySubject[] = "subject";
const char kInterfaceState4WayHandshake[]  = "4way_handshake";
const char kInterfaceStateAssociated[]     = "associated";
const char kInterfaceStateAssociating[]    = "associating";
const char kInterfaceStateAuthenticating[] = "authenticating";
const char kInterfaceStateCompleted[]      = "completed";
const char kInterfaceStateDisconnected[]   = "disconnected";
const char kInterfaceStateGroupHandshake[] = "group_handshake";
const char kInterfaceStateInactive[]       = "inactive";
const char kInterfaceStateScanning[]       = "scanning";
const char kKeyManagementMethodSuffixEAP[] = "-eap";
const char kKeyManagementMethodSuffixPSK[] = "-psk";
const char kKeyModeNone[]           = "NONE";
const char kNetworkBgscanMethodLearn[]  = "learn";
// None is not a real method name, but we interpret 'none' as a request that
// no background scan parameter should be supplied to wpa_supplicant.
const char kNetworkBgscanMethodNone[]   = "none";
const char kNetworkBgscanMethodSimple[] = "simple";
const char kNetworkModeInfrastructure[] = "infrastructure";
const char kNetworkModeAdHoc[]       = "ad-hoc";
const char kNetworkModeAccessPoint[] = "ap";
const char kNetworkPropertyBgscan[] = "bgscan";
const char kNetworkPropertyCaPath[] = "ca_path";
const char kNetworkPropertyEapKeyManagement[] = "key_mgmt";
const char kNetworkPropertyEapIdentity[] = "identity";
const char kNetworkPropertyEapEap[] = "eap";
const char kNetworkPropertyEapInnerEap[] = "phase2";
const char kNetworkPropertyEapAnonymousIdentity[] = "anonymous_identity";
const char kNetworkPropertyEapClientCert[] = "client_cert";
const char kNetworkPropertyEapPrivateKey[] = "private_key";
const char kNetworkPropertyEapPrivateKeyPassword[] = "private_key_passwd";
const char kNetworkPropertyEapCaCert[] = "ca_cert";
const char kNetworkPropertyEapCaPassword[] = "password";
const char kNetworkPropertyEapCertId[] = "cert_id";
const char kNetworkPropertyEapKeyId[] = "key_id";
const char kNetworkPropertyEapCaCertId[] = "ca_cert_id";
const char kNetworkPropertyEapPin[] = "pin";
const char kNetworkPropertyEapSubjectMatch[] = "subject_match";
const char kNetworkPropertyEngine[] = "engine";
const char kNetworkPropertyEngineId[] = "engine_id";
const char kNetworkPropertyMode[]   = "mode";
const char kNetworkPropertyScanSSID[] = "scan_ssid";
const char kNetworkPropertySSID[]   = "ssid";
const char kPropertyAuthAlg[]       = "auth_alg";
const char kPropertyPreSharedKey[]  = "psk";
const char kPropertyPrivacy[]       = "Privacy";
const char kPropertyRSN[]           = "RSN";
const char kPropertyScanSSIDs[]     = "SSIDs";
const char kPropertyScanType[]      = "Type";
const char kPropertySecurityProtocol[] = "proto";
const char kPropertyWEPKey[]        = "wep_key";
const char kPropertyWEPTxKeyIndex[] = "wep_tx_keyidx";
const char kPropertyWPA[]           = "WPA";
const char kScanTypeActive[]        = "active";
const char kSecurityAuthAlg[]       = "OPEN SHARED";
const char kSecurityMethodPropertyKeyManagement[] = "KeyMgmt";
const char kSecurityModeRSN[]       = "RSN";
const char kSecurityModeWPA[]       = "WPA";

const uint32_t kDefaultEngine = 1;
const uint32_t kNetworkModeInfrastructureInt = 0;
const uint32_t kNetworkModeAdHocInt          = 1;
const uint32_t kNetworkModeAccessPointInt    = 2;
const uint32_t kScanMaxSSIDsPerScan          = 4;
};

}  // namespace shill
