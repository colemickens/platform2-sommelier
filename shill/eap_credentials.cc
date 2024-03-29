// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/eap_credentials.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <base/strings/string_tokenizer.h>
#include <base/strings/string_util.h>
#include <base/json/json_string_value_serializer.h>

#include <chromeos/dbus/service_constants.h>
#include <libpasswordprovider/password.h>
#include <libpasswordprovider/password_provider.h>

#include "shill/certificate_file.h"
#include "shill/key_value_store.h"
#include "shill/logging.h"
#include "shill/metrics.h"
#include "shill/property_accessor.h"
#include "shill/property_store.h"
#include "shill/service.h"
#include "shill/store_interface.h"
#include "shill/supplicant/wpa_supplicant.h"

using base::FilePath;
using std::string;
using std::vector;

namespace shill {

namespace Logging {
static auto kModuleLogScope = ScopeLogger::kService;
static string ObjectID(const EapCredentials* e) {
  return "(eap_credentials)";
}
}  // namespace Logging

const char EapCredentials::kStorageEapAnonymousIdentity[] =
    "EAP.AnonymousIdentity";
const char EapCredentials::kStorageEapCACertID[] = "EAP.CACertID";
const char EapCredentials::kStorageEapCACertPEM[] = "EAP.CACertPEM";
const char EapCredentials::kStorageEapCertID[] = "EAP.CertID";
const char EapCredentials::kStorageEapEap[] = "EAP.EAP";
const char EapCredentials::kStorageEapIdentity[] = "EAP.Identity";
const char EapCredentials::kStorageEapInnerEap[] = "EAP.InnerEAP";
const char EapCredentials::kStorageEapTLSVersionMax[] = "EAP.TLSVersionMax";
const char EapCredentials::kStorageEapKeyID[] = "EAP.KeyID";
const char EapCredentials::kStorageEapKeyManagement[] = "EAP.KeyMgmt";
const char EapCredentials::kStorageEapPin[] = "EAP.PIN";
const char EapCredentials::kStorageEapPassword[] = "EAP.Password";
const char EapCredentials::kStorageEapSubjectMatch[] = "EAP.SubjectMatch";
const char EapCredentials::kStorageEapUseProactiveKeyCaching[] =
    "EAP.UseProactiveKeyCaching";
const char EapCredentials::kStorageEapUseSystemCAs[] = "EAP.UseSystemCAs";
const char EapCredentials::kStorageEapUseLoginPassword[] =
    "EAP.UseLoginPassword";
constexpr char kStorageEapSubjectAlternativeNameMatch[] =
    "EAP.SubjectAlternativeNameMatch";

EapCredentials::EapCredentials()
    : use_system_cas_(true),
      use_proactive_key_caching_(false),
      use_login_password_(false),
      password_provider_(
          std::make_unique<password_provider::PasswordProvider>()) {}

EapCredentials::~EapCredentials() = default;

// static
void EapCredentials::PopulateSupplicantProperties(
    CertificateFile* certificate_file, KeyValueStore* params) const {
  string ca_cert;
  if (!ca_cert_pem_.empty()) {
    FilePath certfile = certificate_file->CreatePEMFromStrings(ca_cert_pem_);
    if (certfile.empty()) {
      LOG(ERROR) << "Unable to extract PEM certificate.";
    } else {
      ca_cert = certfile.value();
    }
  }

  using KeyVal = std::pair<const char*, const char*>;
  vector<KeyVal> propertyvals = {
      // Authentication properties.
      KeyVal(WPASupplicant::kNetworkPropertyEapAnonymousIdentity,
             anonymous_identity_.c_str()),
      KeyVal(WPASupplicant::kNetworkPropertyEapIdentity, identity_.c_str()),

      // Non-authentication properties.
      KeyVal(WPASupplicant::kNetworkPropertyEapCaCert, ca_cert.c_str()),
      KeyVal(WPASupplicant::kNetworkPropertyEapCaCertId, ca_cert_id_.c_str()),
      KeyVal(WPASupplicant::kNetworkPropertyEapEap, eap_.c_str()),
      KeyVal(WPASupplicant::kNetworkPropertyEapInnerEap, inner_eap_.c_str()),
      KeyVal(WPASupplicant::kNetworkPropertyEapSubjectMatch,
             subject_match_.c_str()),
  };
  base::Optional<string> altsubject_match =
      TranslateSubjectAlternativeNameMatch(
          subject_alternative_name_match_list_);
  if (altsubject_match.has_value()) {
    propertyvals.push_back(
        KeyVal(WPASupplicant::kNetworkPropertyEapSubjectAlternativeNameMatch,
               altsubject_match.value().c_str()));
  }
  if (use_system_cas_) {
    propertyvals.push_back(
        KeyVal(WPASupplicant::kNetworkPropertyCaPath, WPASupplicant::kCaPath));
  } else if (ca_cert.empty()) {
    LOG(WARNING) << __func__ << ": No certificate authorities are configured."
                 << " Server certificates will be accepted"
                 << " unconditionally.";
  }

  if (ClientAuthenticationUsesCryptoToken()) {
    propertyvals.push_back(
        KeyVal(WPASupplicant::kNetworkPropertyEapCertId, cert_id_.c_str()));
    propertyvals.push_back(
        KeyVal(WPASupplicant::kNetworkPropertyEapKeyId, key_id_.c_str()));
  }

  if (ClientAuthenticationUsesCryptoToken() || !ca_cert_id_.empty()) {
    propertyvals.push_back(
        KeyVal(WPASupplicant::kNetworkPropertyEapPin, pin_.c_str()));
    propertyvals.push_back(KeyVal(WPASupplicant::kNetworkPropertyEngineId,
                                  WPASupplicant::kEnginePKCS11));
    // We can't use the propertyvals vector for this since this argument
    // is a uint32_t, not a string.
    params->SetUint(WPASupplicant::kNetworkPropertyEngine,
                    WPASupplicant::kDefaultEngine);
  }

  if (use_proactive_key_caching_) {
    params->SetUint(WPASupplicant::kNetworkPropertyEapProactiveKeyCaching,
                    WPASupplicant::kProactiveKeyCachingEnabled);
  } else {
    params->SetUint(WPASupplicant::kNetworkPropertyEapProactiveKeyCaching,
                    WPASupplicant::kProactiveKeyCachingDisabled);
  }

  if (tls_version_max_ == kEapTLSVersion1p0) {
    params->SetString(WPASupplicant::kNetworkPropertyEapOuterEap,
                      string(WPASupplicant::kFlagDisableEapTLS1p1) + " " +
                          string(WPASupplicant::kFlagDisableEapTLS1p2));
  } else if (tls_version_max_ == kEapTLSVersion1p1) {
    params->SetString(WPASupplicant::kNetworkPropertyEapOuterEap,
                      WPASupplicant::kFlagDisableEapTLS1p2);
  }

  if (use_login_password_) {
    std::unique_ptr<password_provider::Password> password =
        password_provider_->GetPassword();
    if (password == nullptr || password->size() == 0) {
      LOG(WARNING) << "Unable to retrieve user password";
    } else {
      params->SetString(WPASupplicant::kNetworkPropertyEapCaPassword,
                        std::string(password->GetRaw(), password->size()));
    }
  } else {
    if (!password_.empty()) {
      params->SetString(WPASupplicant::kNetworkPropertyEapCaPassword,
                        password_);
    }
  }

  for (const auto& keyval : propertyvals) {
    if (strlen(keyval.second) > 0) {
      params->SetString(keyval.first, keyval.second);
    }
  }
}

void EapCredentials::InitPropertyStore(PropertyStore* store) {
  // Authentication properties.
  store->RegisterString(kEapAnonymousIdentityProperty, &anonymous_identity_);
  store->RegisterString(kEapCertIdProperty, &cert_id_);
  store->RegisterString(kEapIdentityProperty, &identity_);
  store->RegisterString(kEapKeyIdProperty, &key_id_);
  HelpRegisterDerivedString(store, kEapKeyMgmtProperty,
                            &EapCredentials::GetKeyManagement,
                            &EapCredentials::SetKeyManagement);
  HelpRegisterWriteOnlyDerivedString(store, kEapPasswordProperty,
                                     &EapCredentials::SetEapPassword, nullptr,
                                     &password_);
  store->RegisterString(kEapPinProperty, &pin_);
  store->RegisterBool(kEapUseLoginPasswordProperty, &use_login_password_);

  // Non-authentication properties.
  store->RegisterStrings(kEapCaCertPemProperty, &ca_cert_pem_);
  store->RegisterString(kEapCaCertIdProperty, &ca_cert_id_);
  store->RegisterString(kEapMethodProperty, &eap_);
  store->RegisterString(kEapPhase2AuthProperty, &inner_eap_);
  store->RegisterString(kEapTLSVersionMaxProperty, &tls_version_max_);
  store->RegisterString(kEapSubjectMatchProperty, &subject_match_);
  store->RegisterStrings(kEapSubjectAlternativeNameMatchProperty,
                         &subject_alternative_name_match_list_);
  store->RegisterBool(kEapUseProactiveKeyCachingProperty,
                      &use_proactive_key_caching_);
  store->RegisterBool(kEapUseSystemCasProperty, &use_system_cas_);
}

// static
bool EapCredentials::IsEapAuthenticationProperty(const string property) {
  return property == kEapAnonymousIdentityProperty ||
         property == kEapCertIdProperty || property == kEapIdentityProperty ||
         property == kEapKeyIdProperty || property == kEapKeyMgmtProperty ||
         property == kEapPasswordProperty || property == kEapPinProperty ||
         property == kEapUseLoginPasswordProperty;
}

bool EapCredentials::IsConnectable() const {
  // Identity is required.
  if (identity_.empty()) {
    SLOG(this, 2) << "Not connectable: Identity is empty.";
    return false;
  }

  if (!cert_id_.empty()) {
    // If a client certificate is being used, we must have a private key.
    if (key_id_.empty()) {
      SLOG(this, 2)
          << "Not connectable: Client certificate but no private key.";
      return false;
    }
  }
  if (!cert_id_.empty() || !key_id_.empty() || !ca_cert_id_.empty()) {
    // If PKCS#11 data is needed, a PIN is required.
    if (pin_.empty()) {
      SLOG(this, 2) << "Not connectable: PKCS#11 data but no PIN.";
      return false;
    }
  }

  // For EAP-TLS, a client certificate is required.
  if (eap_.empty() || eap_ == kEapMethodTLS) {
    if (!cert_id_.empty() && !key_id_.empty()) {
      SLOG(this, 2) << "Connectable: EAP-TLS with a client cert and key.";
      return true;
    }
  }

  // For EAP types other than TLS (e.g. EAP-TTLS or EAP-PEAP, password is the
  // minimum requirement), at least an identity + password is required.
  if (eap_.empty() || eap_ != kEapMethodTLS) {
    if (!password_.empty()) {
      SLOG(this, 2) << "Connectable. !EAP-TLS and has a password.";
      return true;
    }
  }

  SLOG(this, 2) << "Not connectable: No suitable EAP configuration was found.";
  return false;
}

bool EapCredentials::IsConnectableUsingPassphrase() const {
  return !identity_.empty() && !password_.empty();
}

void EapCredentials::Load(StoreInterface* storage, const string& id) {
  // Authentication properties.
  storage->GetCryptedString(id, kStorageEapAnonymousIdentity,
                            &anonymous_identity_);
  storage->GetString(id, kStorageEapCertID, &cert_id_);
  storage->GetCryptedString(id, kStorageEapIdentity, &identity_);
  storage->GetString(id, kStorageEapKeyID, &key_id_);
  string key_management;
  storage->GetString(id, kStorageEapKeyManagement, &key_management);
  SetKeyManagement(key_management, nullptr);
  storage->GetCryptedString(id, kStorageEapPassword, &password_);
  storage->GetString(id, kStorageEapPin, &pin_);
  storage->GetBool(id, kStorageEapUseLoginPassword, &use_login_password_);

  // Non-authentication properties.
  storage->GetString(id, kStorageEapCACertID, &ca_cert_id_);
  storage->GetStringList(id, kStorageEapCACertPEM, &ca_cert_pem_);
  storage->GetString(id, kStorageEapEap, &eap_);
  storage->GetString(id, kStorageEapInnerEap, &inner_eap_);
  storage->GetString(id, kStorageEapTLSVersionMax, &tls_version_max_);
  storage->GetString(id, kStorageEapSubjectMatch, &subject_match_);
  storage->GetStringList(id, kStorageEapSubjectAlternativeNameMatch,
                         &subject_alternative_name_match_list_);
  storage->GetBool(id, kStorageEapUseProactiveKeyCaching,
                   &use_proactive_key_caching_);
  storage->GetBool(id, kStorageEapUseSystemCAs, &use_system_cas_);
}

void EapCredentials::OutputConnectionMetrics(Metrics* metrics,
                                             Technology technology) const {
  Metrics::EapOuterProtocol outer_protocol =
      Metrics::EapOuterProtocolStringToEnum(eap_);
  metrics->SendEnumToUMA(
      metrics->GetFullMetricName(Metrics::kMetricNetworkEapOuterProtocolSuffix,
                                 technology),
      outer_protocol, Metrics::kMetricNetworkEapOuterProtocolMax);

  Metrics::EapInnerProtocol inner_protocol =
      Metrics::EapInnerProtocolStringToEnum(inner_eap_);
  metrics->SendEnumToUMA(
      metrics->GetFullMetricName(Metrics::kMetricNetworkEapInnerProtocolSuffix,
                                 technology),
      inner_protocol, Metrics::kMetricNetworkEapInnerProtocolMax);
}

void EapCredentials::Save(StoreInterface* storage,
                          const string& id,
                          bool save_credentials) const {
  // Authentication properties.
  Service::SaveString(storage, id, kStorageEapAnonymousIdentity,
                      anonymous_identity_, true, save_credentials);
  Service::SaveString(storage, id, kStorageEapCertID, cert_id_, false,
                      save_credentials);
  Service::SaveString(storage, id, kStorageEapIdentity, identity_, true,
                      save_credentials);
  Service::SaveString(storage, id, kStorageEapKeyID, key_id_, false,
                      save_credentials);
  Service::SaveString(storage, id, kStorageEapKeyManagement, key_management_,
                      false, true);
  Service::SaveString(storage, id, kStorageEapPassword, password_, true,
                      save_credentials);
  Service::SaveString(storage, id, kStorageEapPin, pin_, false,
                      save_credentials);
  storage->SetBool(id, kStorageEapUseLoginPassword, use_login_password_);

  // Non-authentication properties.
  Service::SaveString(storage, id, kStorageEapCACertID, ca_cert_id_, false,
                      true);
  if (ca_cert_pem_.empty()) {
    storage->DeleteKey(id, kStorageEapCACertPEM);
  } else {
    storage->SetStringList(id, kStorageEapCACertPEM, ca_cert_pem_);
  }
  Service::SaveString(storage, id, kStorageEapEap, eap_, false, true);
  Service::SaveString(storage, id, kStorageEapInnerEap, inner_eap_, false,
                      true);
  Service::SaveString(storage, id, kStorageEapTLSVersionMax, tls_version_max_,
                      false, true);
  Service::SaveString(storage, id, kStorageEapSubjectMatch, subject_match_,
                      false, true);
  storage->SetStringList(id, kStorageEapSubjectAlternativeNameMatch,
                         subject_alternative_name_match_list_);
  storage->SetBool(id, kStorageEapUseProactiveKeyCaching,
                   use_proactive_key_caching_);
  storage->SetBool(id, kStorageEapUseSystemCAs, use_system_cas_);
}

void EapCredentials::Reset() {
  // Authentication properties.
  anonymous_identity_ = "";
  cert_id_ = "";
  identity_ = "";
  key_id_ = "";
  // Do not reset key_management_, since it should never be emptied.
  password_ = "";
  pin_ = "";
  use_login_password_ = false;

  // Non-authentication properties.
  ca_cert_id_ = "";
  ca_cert_pem_.clear();
  eap_ = "";
  inner_eap_ = "";
  subject_match_ = "";
  subject_alternative_name_match_list_.clear();
  use_system_cas_ = true;
  use_proactive_key_caching_ = false;
}

bool EapCredentials::SetEapPassword(const string& password, Error* /*error*/) {
  if (use_login_password_) {
    LOG(WARNING) << "Setting EAP password for configuration requiring the "
                    "user's login password";
    return false;
  }

  if (password_ == password) {
    return false;
  }
  password_ = password;
  return true;
}

string EapCredentials::GetKeyManagement(Error* /*error*/) {
  return key_management_;
}

bool EapCredentials::SetKeyManagement(const std::string& key_management,
                                      Error* /*error*/) {
  if (key_management.empty()) {
    return false;
  }
  if (key_management_ == key_management) {
    return false;
  }
  key_management_ = key_management;
  return true;
}

bool EapCredentials::ClientAuthenticationUsesCryptoToken() const {
  return (eap_.empty() || eap_ == kEapMethodTLS ||
          inner_eap_ == kEapMethodTLS) &&
         (!cert_id_.empty() || !key_id_.empty());
}

void EapCredentials::HelpRegisterDerivedString(
    PropertyStore* store,
    const string& name,
    string (EapCredentials::*get)(Error* error),
    bool (EapCredentials::*set)(const string&, Error*)) {
  store->RegisterDerivedString(
      name, StringAccessor(
                new CustomAccessor<EapCredentials, string>(this, get, set)));
}

void EapCredentials::HelpRegisterWriteOnlyDerivedString(
    PropertyStore* store,
    const string& name,
    bool (EapCredentials::*set)(const string&, Error*),
    void (EapCredentials::*clear)(Error* error),
    const string* default_value) {
  store->RegisterDerivedString(
      name, StringAccessor(new CustomWriteOnlyAccessor<EapCredentials, string>(
                this, set, clear, default_value)));
}

// static
bool EapCredentials::ValidSubjectAlternativeNameMatchType(
    const std::string& type) {
  return type == kEapSubjectAlternativeNameMatchTypeEmail ||
         type == kEapSubjectAlternativeNameMatchTypeDNS ||
         type == kEapSubjectAlternativeNameMatchTypeURI;
}

// static
base::Optional<string> EapCredentials::TranslateSubjectAlternativeNameMatch(
    const vector<string>& subject_alternative_name_match_list) {
  vector<string> entries;
  for (const auto& subject_alternative_name_match :
       subject_alternative_name_match_list) {
    JSONStringValueDeserializer deserializer(subject_alternative_name_match);
    int error_code;
    string error_message;
    std::unique_ptr<base::Value> deserialized_value =
        deserializer.Deserialize(&error_code, &error_message);

    base::DictionaryValue* dict = nullptr;
    if (!deserialized_value || !deserialized_value->GetAsDictionary(&dict)) {
      LOG(ERROR)
          << "Could not deserialize a subject alternative name match. Error "
          << error_code << ": " << error_message;
      return base::nullopt;
    }
    string type;
    if (!dict->GetString(kEapSubjectAlternativeNameMatchTypeProperty, &type)) {
      LOG(ERROR) << "Could not find "
                 << kEapSubjectAlternativeNameMatchTypeProperty
                 << " of a subject alternative name match.";
      return base::nullopt;
    }
    if (!ValidSubjectAlternativeNameMatchType(type)) {
      LOG(ERROR) << "Subject alternative name match type: \"" << type
                 << "\" is not supported.";
      return base::nullopt;
    }
    string value;
    if (!dict->GetString(kEapSubjectAlternativeNameMatchValueProperty,
                         &value)) {
      LOG(ERROR) << "Could not find "
                 << kEapSubjectAlternativeNameMatchValueProperty
                 << " of a subject alternative name match.";
      return base::nullopt;
    }
    string translated_entry = type + ":" + value;
    entries.push_back(translated_entry);
  }
  return base::JoinString(entries, ";");
}

}  // namespace shill
