//
// Copyright (C) 2015 The Android Open Source Project
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

#ifndef PROXY_RPC_SECURITY_TYPES_H
#define PROXY_RPC_SECURITY_TYPES_H

#include <stdio.h>
#include <stdlib.h>
#include <sysexits.h>

#include <string>

// TODO: Only creating the datatypes. Need to figure out how to handle it.
class ProxyRpcSecurityType {
 public:
  ProxyRpcSecurityType() {}
};

// Abstracts the security configuration for a WiFi network.
// This bundle of credentials can be passed to both HostapConfig and
// AssociationParameters so that both shill and hostapd can set up and connect
// to an encrypted WiFi network.  By default, we"ll assume we"re connecting
// to an open network.
class SecurityConfig : public ProxyRpcSecurityType {
 public:
  enum WpaModeType {
    MODE_PURE_WPA = 1,
    MODE_PURE_WPA2 = 2,
    MODE_MIXED_WPA = WPA_MODE_PURE | WPA_MODE_PURE_2,
    MODE_DEFAULT = WPA_MODE_MIXED,
  };
  enum AuthAlgorithmType {
    AUTH_ALGORITHM_TYPE_OPEN = 1,
    AUTH_ALGORITHM_TYPE_SHARED = 2,
    AUTH_ALGORITHM_TYPE_DEFAULT = AUTH_ALGORITHM_TYPE_OPEN
  };
};

// Abstracts security configuration for a WiFi network using static WEP.
// Open system authentication means that we don"t do a 4 way AUTH handshake,
// and simply start using the WEP keys after association finishes.
class WEPConfig : public SecurityConfig {
 public:

 protected:
  std::vector<std::string> wep_keys_;
  int wep_default_key_;
  AuthAlgorithmType auth_algorithm_;
};

// Abstracts security configuration for a WPA encrypted WiFi network.
class WPAConfig : public SecurityConfig {
 public:
  const char kCipherCCMP[] = "CCMP";
  const char kCipherTKIP[] = "TKIP";

 protected:
  std::string psk_;
  WpaModeType wpa_mode_;
  std::vector<std::string> wpa_ciphers_;
  std::vector<std::string> wpa2_ciphers_;
  int wpa_ptk_rekey_period_;
  int wpa_gtk_rekey_period_;
  int wpa_gmk_rekey_period_;
  bool use_strict_rekey_;
};

// Abstract superclass that implements certificate/key installation.
class EAPConfig : public SecurityConfig {
 public:
  const char kDefaultEapUsers[] = "* TLS";
  const char kDefaultEAPIdentity[] = "chromeos";
  const char kServicePropertyCACertPem[] = "EAP.CACertPEM";
  const char kServicePropertyClientCertID[] = "EAP.CertID";
  const char kServicePropertyEAPIdentity[] = "EAP.Identity";
  const char kServicePropertyEAPKeyMgmt[] = "EAP.KeyMgmt";
  const char kServicePropertyEAPPassword[] = "EAP.Password";
  const char kServicePropertyEAPPIN[] = "EAP.PIN";
  const char kServicePropertyInnerEAP[] = "EAP.InnerEAP";
  const char kServicePropertyPrivateKeyID[] = "EAP.KeyID";
  const char kServicePropertyUseSystemCAs[] = "EAP.UseSystemCAs";
  const int last_tmp_id = 8800;

 protected:
  bool use_system_cas_;
  std::string server_ca_cert_;
  std::string server_cert_;
  std::string server_key_;
  std::string server_eap_users;
  std::string client_ca_cert_;
  std::string client_cert_;
  std::string client_key_;
  std::string server_ca_cert_file_path_;
  std::string server_cert_file_path_;
  std::string server_key_file_path_;
  std::string server_eap_user_file_path_;
  std::string file_path_suffix_;
  std::string client_cert_id_;
  std::string client_key_id_;
  std::string pin_;
  std::string client_cert_slot_id_;
  std::string client_key_slot_id_;
  std::string eap_identity_;
};

// Configuration settings bundle for dynamic WEP.
// This is a WEP encrypted connection where the keys are negotiated after the
// client authenticates via 802.1x.
class DynamicWEPConfig : public EAPConfig {
 public:
  const int kDefaultKeyPeriod = 20;

 protected:
  bool use_short_keys_;
  int wep_rekey_period_;
};

// Security type to set up a WPA tunnel via EAP-TLS negotiation.
class WPAEAPConfig : public EAPConfig {
 public:

 protected:
  bool use_short_keys_;
  WpaModeType wpa_mode_;
};

// Security type to set up a TTLS/PEAP connection.
// Both PEAP and TTLS are tunneled protocols which use EAP inside of a TLS
// secured tunnel.  The secured tunnel is a symmetric key encryption scheme
// negotiated under the protection of a public key in the server certificate.
// Thus, we"ll see server credentials in the form of certificates, but client
// credentials in the form of passwords and a CA Cert to root the trust chain.
class Tunneled1xConfig : public WPAEAPConfig {
 public:
   const char kTTLSPrefix[] = "TTLS-";
   const char kLayer1TypePEAP[] = "PEAP";
   const char kLayer1TypeTTLS[] = "TTLS";
   const char kLayer2TypeGTC[] = "GTC";
   const char kLayer2TypeMSCHAPV2[] = "MSCHAPV2";
   const char kLayer2TypeMD5[] = "MD5";
   const char kLayer2TypeTTLSMSCHAPV2[] = TTLS_PREFIX + "MSCHAPV2";
   const char kLayer2TypeTTLSMSCHAP[] = TTLS_PREFIX + "MSCHAP";
   const char kLayer2TypeTTLSPAP[] = TTLS_PREFIX + "PAP";

 protected:
  std::string password_;
  std::string inner_protocol_;
};

#endif // PROXY_RPC_SECURITY_TYPES_H
