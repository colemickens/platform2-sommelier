// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libweave/src/privet/privet_handler.h"

#include <memory>
#include <set>
#include <string>
#include <utility>

#include <base/bind.h>
#include <base/location.h>
#include <base/stl_util.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/stringprintf.h>
#include <base/values.h>
#include <weave/enum_to_string.h>

#include "libweave/src/http_constants.h"
#include "libweave/src/privet/cloud_delegate.h"
#include "libweave/src/privet/constants.h"
#include "libweave/src/privet/device_delegate.h"
#include "libweave/src/privet/identity_delegate.h"
#include "libweave/src/privet/security_delegate.h"
#include "libweave/src/privet/wifi_delegate.h"
#include "libweave/src/string_utils.h"

namespace weave {
namespace privet {

namespace {

const char kInfoVersionKey[] = "version";
const char kInfoVersionValue[] = "3.0";

const char kNameKey[] = "name";
const char kDescrptionKey[] = "description";
const char kLocationKey[] = "location";

const char kGcdKey[] = "gcd";
const char kWifiKey[] = "wifi";
const char kStatusKey[] = "status";
const char kErrorKey[] = "error";
const char kCryptoKey[] = "crypto";
const char kStatusErrorValue[] = "error";

const char kInfoIdKey[] = "id";
const char kInfoServicesKey[] = "services";

const char kInfoEndpointsKey[] = "endpoints";
const char kInfoEndpointsHttpPortKey[] = "httpPort";
const char kInfoEndpointsHttpUpdatePortKey[] = "httpUpdatesPort";
const char kInfoEndpointsHttpsPortKey[] = "httpsPort";
const char kInfoEndpointsHttpsUpdatePortKey[] = "httpsUpdatesPort";

const char kInfoModelIdKey[] = "modelManifestId";
const char kInfoModelManifestKey[] = "basicModelManifest";
const char kInfoManifestUiDeviceKind[] = "uiDeviceKind";
const char kInfoManifestOemName[] = "oemName";
const char kInfoManifestModelName[] = "modelName";

const char kInfoAuthenticationKey[] = "authentication";

const char kInfoAuthAnonymousMaxScopeKey[] = "anonymousMaxScope";

const char kInfoWifiCapabilitiesKey[] = "capabilities";
const char kInfoWifiSsidKey[] = "ssid";
const char kInfoWifiHostedSsidKey[] = "hostedSsid";

const char kInfoUptimeKey[] = "uptime";

const char kPairingKey[] = "pairing";
const char kPairingSessionIdKey[] = "sessionId";
const char kPairingDeviceCommitmentKey[] = "deviceCommitment";
const char kPairingClientCommitmentKey[] = "clientCommitment";
const char kPairingFingerprintKey[] = "certFingerprint";
const char kPairingSignatureKey[] = "certSignature";

const char kAuthTypeAnonymousValue[] = "anonymous";
const char kAuthTypePairingValue[] = "pairing";

const char kAuthModeKey[] = "mode";
const char kAuthCodeKey[] = "authCode";
const char kAuthRequestedScopeKey[] = "requestedScope";
const char kAuthScopeAutoValue[] = "auto";

const char kAuthAccessTokenKey[] = "accessToken";
const char kAuthTokenTypeKey[] = "tokenType";
const char kAuthExpiresInKey[] = "expiresIn";
const char kAuthScopeKey[] = "scope";

const char kAuthorizationHeaderPrefix[] = "Privet";

const char kErrorCodeKey[] = "code";
const char kErrorMessageKey[] = "message";
const char kErrorDebugInfoKey[] = "debugInfo";

const char kSetupStartSsidKey[] = "ssid";
const char kSetupStartPassKey[] = "passphrase";
const char kSetupStartTicketIdKey[] = "ticketId";
const char kSetupStartUserKey[] = "user";

const char kFingerprintKey[] = "fingerprint";
const char kStateKey[] = "state";
const char kCommandsKey[] = "commands";
const char kCommandsIdKey[] = "id";

const char kInvalidParamValueFormat[] = "Invalid parameter: '%s'='%s'";

const int kAccessTokenExpirationSeconds = 3600;

// Threshold to reduce probability of expiration because of clock difference
// between device and client. Value is just a guess.
const int kAccessTokenExpirationThresholdSeconds = 300;

template <class Container>
std::unique_ptr<base::ListValue> ToValue(const Container& list) {
  std::unique_ptr<base::ListValue> value_list(new base::ListValue());
  for (const std::string& val : list)
    value_list->AppendString(val);
  return value_list;
}

struct {
  const char* const reason;
  int code;
} kReasonToCode[] = {
    {errors::kInvalidClientCommitment, http::kForbidden},
    {errors::kInvalidFormat, http::kBadRequest},
    {errors::kMissingAuthorization, http::kDenied},
    {errors::kInvalidAuthorization, http::kDenied},
    {errors::kInvalidAuthorizationScope, http::kForbidden},
    {errors::kAuthorizationExpired, http::kForbidden},
    {errors::kCommitmentMismatch, http::kForbidden},
    {errors::kUnknownSession, http::kNotFound},
    {errors::kInvalidAuthCode, http::kForbidden},
    {errors::kInvalidAuthMode, http::kBadRequest},
    {errors::kInvalidRequestedScope, http::kBadRequest},
    {errors::kAccessDenied, http::kForbidden},
    {errors::kInvalidParams, http::kBadRequest},
    {errors::kSetupUnavailable, http::kBadRequest},
    {errors::kDeviceBusy, http::kServiceUnavailable},
    {errors::kInvalidState, http::kInternalServerError},
    {errors::kNotFound, http::kNotFound},
    {errors::kNotImplemented, http::kNotSupported},
};

AuthScope AuthScopeFromString(const std::string& scope, AuthScope auto_scope) {
  if (scope == kAuthScopeAutoValue)
    return auto_scope;
  AuthScope scope_id = AuthScope::kNone;
  StringToEnum(scope, &scope_id);
  return scope_id;
}

std::string GetAuthTokenFromAuthHeader(const std::string& auth_header) {
  return SplitAtFirst(auth_header, " ", true).second;
}

std::unique_ptr<base::DictionaryValue> ErrorInfoToJson(const Error& error) {
  std::unique_ptr<base::DictionaryValue> output{new base::DictionaryValue};
  output->SetString(kErrorMessageKey, error.GetMessage());
  output->SetString(kErrorCodeKey, error.GetCode());
  return output;
}

// Creates JSON similar to GCD server error format.
std::unique_ptr<base::DictionaryValue> ErrorToJson(const Error& error) {
  std::unique_ptr<base::DictionaryValue> output{ErrorInfoToJson(error)};

  // Optional debug information.
  std::unique_ptr<base::ListValue> errors{new base::ListValue};
  for (const Error* it = &error; it; it = it->GetInnerError()) {
    std::unique_ptr<base::DictionaryValue> inner{ErrorInfoToJson(*it)};
    tracked_objects::Location location{it->GetLocation().function_name.c_str(),
                                       it->GetLocation().file_name.c_str(),
                                       it->GetLocation().line_number,
                                       nullptr};
    inner->SetString(kErrorDebugInfoKey, location.ToString());
    errors->Append(inner.release());
  }
  output->Set(kErrorDebugInfoKey, errors.release());
  return output;
}

template <class T>
void SetState(const T& state, base::DictionaryValue* parent) {
  if (!state.error()) {
    parent->SetString(kStatusKey, EnumToString(state.status()));
    return;
  }
  parent->SetString(kStatusKey, kStatusErrorValue);
  parent->Set(kErrorKey, ErrorToJson(*state.error()).release());
}

void ReturnError(const Error& error,
                 const PrivetHandler::RequestCallback& callback) {
  int code = http::kInternalServerError;
  for (const auto& it : kReasonToCode) {
    if (error.HasError(errors::kDomain, it.reason)) {
      code = it.code;
      break;
    }
  }
  std::unique_ptr<base::DictionaryValue> output{new base::DictionaryValue};
  output->Set(kErrorKey, ErrorToJson(error).release());
  callback.Run(code, *output);
}

void OnCommandRequestSucceeded(const PrivetHandler::RequestCallback& callback,
                               const base::DictionaryValue& output) {
  callback.Run(http::kOk, output);
}

void OnCommandRequestFailed(const PrivetHandler::RequestCallback& callback,
                            Error* error) {
  if (error->HasError("gcd", "unknown_command")) {
    ErrorPtr new_error = error->Clone();
    Error::AddTo(&new_error, FROM_HERE, errors::kDomain, errors::kNotFound,
                 "Unknown command ID");
    return ReturnError(*new_error, callback);
  }
  if (error->HasError("gcd", "access_denied")) {
    ErrorPtr new_error = error->Clone();
    Error::AddTo(&new_error, FROM_HERE, errors::kDomain, errors::kAccessDenied,
                 error->GetMessage());
    return ReturnError(*new_error, callback);
  }
  return ReturnError(*error, callback);
}

std::string GetDeviceKind(const std::string& manifest_id) {
  CHECK_EQ(5u, manifest_id.size());
  std::string kind = manifest_id.substr(0, 2);
  if (kind == "AC")
    return "accessPoint";
  if (kind == "AK")
    return "aggregator";
  if (kind == "AM")
    return "camera";
  if (kind == "AB")
    return "developmentBoard";
  if (kind == "AE")
    return "printer";
  if (kind == "AF")
    return "scanner";
  if (kind == "AD")
    return "speaker";
  if (kind == "AL")
    return "storage";
  if (kind == "AJ")
    return "toy";
  if (kind == "AA")
    return "vendor";
  if (kind == "AN")
    return "video";
  LOG(FATAL) << "Invalid model id: " << manifest_id;
  return std::string();
}

std::unique_ptr<base::DictionaryValue> CreateManifestSection(
    const std::string& model_id,
    const CloudDelegate& cloud) {
  std::unique_ptr<base::DictionaryValue> manifest(new base::DictionaryValue());
  manifest->SetString(kInfoManifestUiDeviceKind, GetDeviceKind(model_id));
  manifest->SetString(kInfoManifestOemName, cloud.GetOemName());
  manifest->SetString(kInfoManifestModelName, cloud.GetModelName());
  return manifest;
}

std::unique_ptr<base::DictionaryValue> CreateEndpointsSection(
    const DeviceDelegate& device) {
  std::unique_ptr<base::DictionaryValue> endpoints(new base::DictionaryValue());
  auto http_endpoint = device.GetHttpEnpoint();
  endpoints->SetInteger(kInfoEndpointsHttpPortKey, http_endpoint.first);
  endpoints->SetInteger(kInfoEndpointsHttpUpdatePortKey, http_endpoint.second);

  auto https_endpoint = device.GetHttpsEnpoint();
  endpoints->SetInteger(kInfoEndpointsHttpsPortKey, https_endpoint.first);
  endpoints->SetInteger(kInfoEndpointsHttpsUpdatePortKey,
                        https_endpoint.second);

  return endpoints;
}

std::unique_ptr<base::DictionaryValue> CreateInfoAuthSection(
    const SecurityDelegate& security,
    AuthScope anonymous_max_scope) {
  std::unique_ptr<base::DictionaryValue> auth(new base::DictionaryValue());

  auth->SetString(kInfoAuthAnonymousMaxScopeKey,
                  EnumToString(anonymous_max_scope));

  std::unique_ptr<base::ListValue> pairing_types(new base::ListValue());
  for (PairingType type : security.GetPairingTypes())
    pairing_types->AppendString(EnumToString(type));
  auth->Set(kPairingKey, pairing_types.release());

  std::unique_ptr<base::ListValue> auth_types(new base::ListValue());
  auth_types->AppendString(kAuthTypeAnonymousValue);
  auth_types->AppendString(kAuthTypePairingValue);

  // TODO(vitalybuka): Implement cloud auth.
  // if (cloud.GetConnectionState().IsStatusEqual(ConnectionState::kOnline)) {
  //   auth_types->AppendString(kAuthTypeCloudValue);
  // }
  auth->Set(kAuthModeKey, auth_types.release());

  std::unique_ptr<base::ListValue> crypto_types(new base::ListValue());
  for (CryptoType type : security.GetCryptoTypes())
    crypto_types->AppendString(EnumToString(type));
  auth->Set(kCryptoKey, crypto_types.release());

  return auth;
}

std::unique_ptr<base::DictionaryValue> CreateWifiSection(
    const WifiDelegate& wifi) {
  std::unique_ptr<base::DictionaryValue> result(new base::DictionaryValue());

  std::unique_ptr<base::ListValue> capabilities(new base::ListValue());
  for (WifiType type : wifi.GetTypes())
    capabilities->AppendString(EnumToString(type));
  result->Set(kInfoWifiCapabilitiesKey, capabilities.release());

  result->SetString(kInfoWifiSsidKey, wifi.GetCurrentlyConnectedSsid());

  std::string hosted_ssid = wifi.GetHostedSsid();
  const ConnectionState& state = wifi.GetConnectionState();
  if (!hosted_ssid.empty()) {
    DCHECK(!state.IsStatusEqual(ConnectionState::kDisabled));
    DCHECK(!state.IsStatusEqual(ConnectionState::kOnline));
    result->SetString(kInfoWifiHostedSsidKey, hosted_ssid);
  }
  SetState(state, result.get());
  return result;
}

std::unique_ptr<base::DictionaryValue> CreateGcdSection(
    const CloudDelegate& cloud) {
  std::unique_ptr<base::DictionaryValue> gcd(new base::DictionaryValue());
  gcd->SetString(kInfoIdKey, cloud.GetCloudId());
  SetState(cloud.GetConnectionState(), gcd.get());
  return gcd;
}

AuthScope GetAnonymousMaxScope(const CloudDelegate& cloud,
                               const WifiDelegate* wifi) {
  if (wifi && !wifi->GetHostedSsid().empty())
    return AuthScope::kNone;
  return cloud.GetAnonymousMaxScope();
}

}  // namespace

PrivetHandler::PrivetHandler(CloudDelegate* cloud,
                             DeviceDelegate* device,
                             SecurityDelegate* security,
                             WifiDelegate* wifi,
                             IdentityDelegate* identity)
    : cloud_(cloud),
      device_(device),
      security_(security),
      wifi_(wifi),
      identity_(identity) {
  CHECK(cloud_);
  CHECK(device_);
  CHECK(security_);
  cloud_observer_.Add(cloud_);

  AddHandler("/privet/info", &PrivetHandler::HandleInfo, AuthScope::kNone);
  AddHandler("/privet/v3/pairing/start", &PrivetHandler::HandlePairingStart,
             AuthScope::kNone);
  AddHandler("/privet/v3/pairing/confirm", &PrivetHandler::HandlePairingConfirm,
             AuthScope::kNone);
  AddHandler("/privet/v3/pairing/cancel", &PrivetHandler::HandlePairingCancel,
             AuthScope::kNone);
  AddHandler("/privet/v3/auth", &PrivetHandler::HandleAuth, AuthScope::kNone);
  AddHandler("/privet/v3/setup/start", &PrivetHandler::HandleSetupStart,
             AuthScope::kOwner);
  AddHandler("/privet/v3/setup/status", &PrivetHandler::HandleSetupStatus,
             AuthScope::kOwner);
  AddHandler("/privet/v3/state", &PrivetHandler::HandleState,
             AuthScope::kViewer);
  AddHandler("/privet/v3/commandDefs", &PrivetHandler::HandleCommandDefs,
             AuthScope::kViewer);
  AddHandler("/privet/v3/commands/execute",
             &PrivetHandler::HandleCommandsExecute, AuthScope::kViewer);
  AddHandler("/privet/v3/commands/status", &PrivetHandler::HandleCommandsStatus,
             AuthScope::kViewer);
  AddHandler("/privet/v3/commands/cancel", &PrivetHandler::HandleCommandsCancel,
             AuthScope::kViewer);
  AddHandler("/privet/v3/commands/list", &PrivetHandler::HandleCommandsList,
             AuthScope::kViewer);
}

PrivetHandler::~PrivetHandler() {
}

void PrivetHandler::OnCommandDefsChanged() {
  ++command_defs_fingerprint_;
}

void PrivetHandler::OnStateChanged() {
  ++state_fingerprint_;
}

void PrivetHandler::HandleRequest(const std::string& api,
                                  const std::string& auth_header,
                                  const base::DictionaryValue* input,
                                  const RequestCallback& callback) {
  ErrorPtr error;
  if (!input) {
    Error::AddTo(&error, FROM_HERE, errors::kDomain, errors::kInvalidFormat,
                 "Malformed JSON");
    return ReturnError(*error, callback);
  }
  auto handler = handlers_.find(api);
  if (handler == handlers_.end()) {
    Error::AddTo(&error, FROM_HERE, errors::kDomain, errors::kNotFound,
                 "Path not found");
    return ReturnError(*error, callback);
  }
  if (auth_header.empty()) {
    Error::AddTo(&error, FROM_HERE, errors::kDomain,
                 errors::kMissingAuthorization,
                 "Authorization header must not be empty");
    return ReturnError(*error, callback);
  }
  std::string token = GetAuthTokenFromAuthHeader(auth_header);
  if (token.empty()) {
    Error::AddToPrintf(&error, FROM_HERE, errors::kDomain,
                       errors::kInvalidAuthorization,
                       "Invalid authorization header: %s", auth_header.c_str());
    return ReturnError(*error, callback);
  }
  UserInfo user_info;
  if (token != kAuthTypeAnonymousValue) {
    base::Time time;
    user_info = security_->ParseAccessToken(token, &time);
    if (user_info.scope() == AuthScope::kNone) {
      Error::AddToPrintf(&error, FROM_HERE, errors::kDomain,
                         errors::kInvalidAuthorization,
                         "Invalid access token: %s", token.c_str());
      return ReturnError(*error, callback);
    }
    time += base::TimeDelta::FromSeconds(kAccessTokenExpirationSeconds);
    time +=
        base::TimeDelta::FromSeconds(kAccessTokenExpirationThresholdSeconds);
    if (time < base::Time::Now()) {
      Error::AddToPrintf(&error, FROM_HERE, errors::kDomain,
                         errors::kAuthorizationExpired, "Token expired: %s",
                         token.c_str());
      return ReturnError(*error, callback);
    }
  }

  if (handler->second.first > user_info.scope()) {
    Error::AddToPrintf(&error, FROM_HERE, errors::kDomain,
                       errors::kInvalidAuthorizationScope,
                       "Scope '%s' does not allow '%s'",
                       EnumToString(user_info.scope()).c_str(), api.c_str());
    return ReturnError(*error, callback);
  }
  (this->*handler->second.second)(*input, user_info, callback);
}

void PrivetHandler::AddHandler(const std::string& path,
                               ApiHandler handler,
                               AuthScope scope) {
  CHECK(handlers_.emplace(path, std::make_pair(scope, handler)).second);
}

void PrivetHandler::HandleInfo(const base::DictionaryValue&,
                               const UserInfo& user_info,
                               const RequestCallback& callback) {
  base::DictionaryValue output;

  ErrorPtr error;

  std::string name;
  std::string model_id;
  if (!cloud_->GetName(&name, &error) ||
      !cloud_->GetModelId(&model_id, &error)) {
    return ReturnError(*error, callback);
  }

  output.SetString(kInfoVersionKey, kInfoVersionValue);
  output.SetString(kInfoIdKey, identity_->GetId());
  output.SetString(kNameKey, name);

  std::string description{cloud_->GetDescription()};
  if (!description.empty())
    output.SetString(kDescrptionKey, description);

  std::string location{cloud_->GetLocation()};
  if (!location.empty())
    output.SetString(kLocationKey, location);

  output.SetString(kInfoModelIdKey, model_id);
  output.Set(kInfoModelManifestKey,
             CreateManifestSection(model_id, *cloud_).release());
  output.Set(kInfoServicesKey, ToValue(cloud_->GetServices()).release());

  output.Set(
      kInfoAuthenticationKey,
      CreateInfoAuthSection(*security_, GetAnonymousMaxScope(*cloud_, wifi_))
          .release());

  output.Set(kInfoEndpointsKey, CreateEndpointsSection(*device_).release());

  if (wifi_)
    output.Set(kWifiKey, CreateWifiSection(*wifi_).release());

  output.Set(kGcdKey, CreateGcdSection(*cloud_).release());

  output.SetInteger(kInfoUptimeKey, device_->GetUptime().InSeconds());

  callback.Run(http::kOk, output);
}

void PrivetHandler::HandlePairingStart(const base::DictionaryValue& input,
                                       const UserInfo& user_info,
                                       const RequestCallback& callback) {
  ErrorPtr error;

  std::string pairing_str;
  input.GetString(kPairingKey, &pairing_str);

  std::string crypto_str;
  input.GetString(kCryptoKey, &crypto_str);

  PairingType pairing;
  std::set<PairingType> modes = security_->GetPairingTypes();
  if (!StringToEnum(pairing_str, &pairing) || !ContainsKey(modes, pairing)) {
    Error::AddToPrintf(&error, FROM_HERE, errors::kDomain,
                       errors::kInvalidParams, kInvalidParamValueFormat,
                       kPairingKey, pairing_str.c_str());
    return ReturnError(*error, callback);
  }

  CryptoType crypto;
  std::set<CryptoType> cryptos = security_->GetCryptoTypes();
  if (!StringToEnum(crypto_str, &crypto) || !ContainsKey(cryptos, crypto)) {
    Error::AddToPrintf(&error, FROM_HERE, errors::kDomain,
                       errors::kInvalidParams, kInvalidParamValueFormat,
                       kCryptoKey, crypto_str.c_str());
    return ReturnError(*error, callback);
  }

  std::string id;
  std::string commitment;
  if (!security_->StartPairing(pairing, crypto, &id, &commitment, &error))
    return ReturnError(*error, callback);

  base::DictionaryValue output;
  output.SetString(kPairingSessionIdKey, id);
  output.SetString(kPairingDeviceCommitmentKey, commitment);
  callback.Run(http::kOk, output);
}

void PrivetHandler::HandlePairingConfirm(const base::DictionaryValue& input,
                                         const UserInfo& user_info,
                                         const RequestCallback& callback) {
  std::string id;
  input.GetString(kPairingSessionIdKey, &id);

  std::string commitment;
  input.GetString(kPairingClientCommitmentKey, &commitment);

  std::string fingerprint;
  std::string signature;
  ErrorPtr error;
  if (!security_->ConfirmPairing(id, commitment, &fingerprint, &signature,
                                 &error)) {
    return ReturnError(*error, callback);
  }

  base::DictionaryValue output;
  output.SetString(kPairingFingerprintKey, fingerprint);
  output.SetString(kPairingSignatureKey, signature);
  callback.Run(http::kOk, output);
}

void PrivetHandler::HandlePairingCancel(const base::DictionaryValue& input,
                                        const UserInfo& user_info,
                                        const RequestCallback& callback) {
  std::string id;
  input.GetString(kPairingSessionIdKey, &id);

  ErrorPtr error;
  if (!security_->CancelPairing(id, &error))
    return ReturnError(*error, callback);

  base::DictionaryValue output;
  callback.Run(http::kOk, output);
}

void PrivetHandler::HandleAuth(const base::DictionaryValue& input,
                               const UserInfo& user_info,
                               const RequestCallback& callback) {
  ErrorPtr error;

  std::string auth_code_type;
  input.GetString(kAuthModeKey, &auth_code_type);

  std::string auth_code;
  input.GetString(kAuthCodeKey, &auth_code);

  AuthScope max_auth_scope = AuthScope::kNone;
  if (auth_code_type == kAuthTypeAnonymousValue) {
    max_auth_scope = GetAnonymousMaxScope(*cloud_, wifi_);
  } else if (auth_code_type == kAuthTypePairingValue) {
    if (!security_->IsValidPairingCode(auth_code)) {
      Error::AddToPrintf(&error, FROM_HERE, errors::kDomain,
                         errors::kInvalidAuthCode, kInvalidParamValueFormat,
                         kAuthCodeKey, auth_code.c_str());
      return ReturnError(*error, callback);
    }
    max_auth_scope = AuthScope::kOwner;
  } else {
    Error::AddToPrintf(&error, FROM_HERE, errors::kDomain,
                       errors::kInvalidAuthMode, kInvalidParamValueFormat,
                       kAuthModeKey, auth_code_type.c_str());
    return ReturnError(*error, callback);
  }

  std::string requested_scope;
  input.GetString(kAuthRequestedScopeKey, &requested_scope);

  AuthScope requested_auth_scope =
      AuthScopeFromString(requested_scope, max_auth_scope);
  if (requested_auth_scope == AuthScope::kNone) {
    Error::AddToPrintf(&error, FROM_HERE, errors::kDomain,
                       errors::kInvalidRequestedScope, kInvalidParamValueFormat,
                       kAuthRequestedScopeKey, requested_scope.c_str());
    return ReturnError(*error, callback);
  }

  if (requested_auth_scope > max_auth_scope) {
    Error::AddToPrintf(&error, FROM_HERE, errors::kDomain,
                       errors::kAccessDenied, "Scope '%s' is not allowed",
                       EnumToString(requested_auth_scope).c_str());
    return ReturnError(*error, callback);
  }

  base::DictionaryValue output;
  output.SetString(
      kAuthAccessTokenKey,
      security_->CreateAccessToken(
          UserInfo{requested_auth_scope, ++last_user_id_}, base::Time::Now()));
  output.SetString(kAuthTokenTypeKey, kAuthorizationHeaderPrefix);
  output.SetInteger(kAuthExpiresInKey, kAccessTokenExpirationSeconds);
  output.SetString(kAuthScopeKey, EnumToString(requested_auth_scope));
  callback.Run(http::kOk, output);
}

void PrivetHandler::HandleSetupStart(const base::DictionaryValue& input,
                                     const UserInfo& user_info,
                                     const RequestCallback& callback) {
  std::string name;
  ErrorPtr error;
  if (!cloud_->GetName(&name, &error))
    return ReturnError(*error, callback);
  input.GetString(kNameKey, &name);

  std::string description{cloud_->GetDescription()};
  input.GetString(kDescrptionKey, &description);

  std::string location{cloud_->GetLocation()};
  input.GetString(kLocationKey, &location);

  std::string ssid;
  std::string passphrase;
  std::string ticket;
  std::string user;

  const base::DictionaryValue* wifi = nullptr;
  if (input.GetDictionary(kWifiKey, &wifi)) {
    if (!wifi_ || wifi_->GetTypes().empty()) {
      Error::AddTo(&error, FROM_HERE, errors::kDomain,
                   errors::kSetupUnavailable, "WiFi setup unavailible");
      return ReturnError(*error, callback);
    }
    wifi->GetString(kSetupStartSsidKey, &ssid);
    if (ssid.empty()) {
      Error::AddToPrintf(&error, FROM_HERE, errors::kDomain,
                         errors::kInvalidParams, kInvalidParamValueFormat,
                         kSetupStartSsidKey, "");
      return ReturnError(*error, callback);
    }
    wifi->GetString(kSetupStartPassKey, &passphrase);
  }

  const base::DictionaryValue* registration = nullptr;
  if (input.GetDictionary(kGcdKey, &registration)) {
    registration->GetString(kSetupStartTicketIdKey, &ticket);
    if (ticket.empty()) {
      Error::AddToPrintf(&error, FROM_HERE, errors::kDomain,
                         errors::kInvalidParams, kInvalidParamValueFormat,
                         kSetupStartTicketIdKey, "");
      return ReturnError(*error, callback);
    }
    registration->GetString(kSetupStartUserKey, &user);
  }

  cloud_->UpdateDeviceInfo(name, description, location,
                           base::Bind(&PrivetHandler::OnUpdateDeviceInfoDone,
                                      weak_ptr_factory_.GetWeakPtr(), ssid,
                                      passphrase, ticket, user, callback),
                           base::Bind(&OnCommandRequestFailed, callback));
}

void PrivetHandler::OnUpdateDeviceInfoDone(
    const std::string& ssid,
    const std::string& passphrase,
    const std::string& ticket,
    const std::string& user,
    const RequestCallback& callback) const {
  ErrorPtr error;

  if (!ssid.empty() && !wifi_->ConfigureCredentials(ssid, passphrase, &error))
    return ReturnError(*error, callback);

  if (!ticket.empty() && !cloud_->Setup(ticket, user, &error))
    return ReturnError(*error, callback);

  ReplyWithSetupStatus(callback);
}

void PrivetHandler::HandleSetupStatus(const base::DictionaryValue&,
                                      const UserInfo& user_info,
                                      const RequestCallback& callback) {
  ReplyWithSetupStatus(callback);
}

void PrivetHandler::ReplyWithSetupStatus(
    const RequestCallback& callback) const {
  base::DictionaryValue output;

  const SetupState& state = cloud_->GetSetupState();
  if (!state.IsStatusEqual(SetupState::kNone)) {
    base::DictionaryValue* gcd = new base::DictionaryValue;
    output.Set(kGcdKey, gcd);
    SetState(state, gcd);
    if (state.IsStatusEqual(SetupState::kSuccess))
      gcd->SetString(kInfoIdKey, cloud_->GetCloudId());
  }

  if (wifi_) {
    const SetupState& state = wifi_->GetSetupState();
    if (!state.IsStatusEqual(SetupState::kNone)) {
      base::DictionaryValue* wifi = new base::DictionaryValue;
      output.Set(kWifiKey, wifi);
      SetState(state, wifi);
      if (state.IsStatusEqual(SetupState::kSuccess))
        wifi->SetString(kInfoWifiSsidKey, wifi_->GetCurrentlyConnectedSsid());
    }
  }

  callback.Run(http::kOk, output);
}

void PrivetHandler::HandleState(const base::DictionaryValue& input,
                                const UserInfo& user_info,
                                const RequestCallback& callback) {
  base::DictionaryValue output;
  base::DictionaryValue* defs = cloud_->GetState().DeepCopy();
  output.Set(kStateKey, defs);
  output.SetString(kFingerprintKey, base::IntToString(state_fingerprint_));

  callback.Run(http::kOk, output);
}

void PrivetHandler::HandleCommandDefs(const base::DictionaryValue& input,
                                      const UserInfo& user_info,
                                      const RequestCallback& callback) {
  base::DictionaryValue output;
  base::DictionaryValue* defs = cloud_->GetCommandDef().DeepCopy();
  output.Set(kCommandsKey, defs);
  output.SetString(kFingerprintKey,
                   base::IntToString(command_defs_fingerprint_));

  callback.Run(http::kOk, output);
}

void PrivetHandler::HandleCommandsExecute(const base::DictionaryValue& input,
                                          const UserInfo& user_info,
                                          const RequestCallback& callback) {
  cloud_->AddCommand(input, user_info,
                     base::Bind(&OnCommandRequestSucceeded, callback),
                     base::Bind(&OnCommandRequestFailed, callback));
}

void PrivetHandler::HandleCommandsStatus(const base::DictionaryValue& input,
                                         const UserInfo& user_info,
                                         const RequestCallback& callback) {
  std::string id;
  if (!input.GetString(kCommandsIdKey, &id)) {
    ErrorPtr error;
    Error::AddToPrintf(&error, FROM_HERE, errors::kDomain,
                       errors::kInvalidParams, kInvalidParamValueFormat,
                       kCommandsIdKey, id.c_str());
    return ReturnError(*error, callback);
  }
  cloud_->GetCommand(id, user_info,
                     base::Bind(&OnCommandRequestSucceeded, callback),
                     base::Bind(&OnCommandRequestFailed, callback));
}

void PrivetHandler::HandleCommandsList(const base::DictionaryValue& input,
                                       const UserInfo& user_info,
                                       const RequestCallback& callback) {
  cloud_->ListCommands(user_info,
                       base::Bind(&OnCommandRequestSucceeded, callback),
                       base::Bind(&OnCommandRequestFailed, callback));
}

void PrivetHandler::HandleCommandsCancel(const base::DictionaryValue& input,
                                         const UserInfo& user_info,
                                         const RequestCallback& callback) {
  std::string id;
  if (!input.GetString(kCommandsIdKey, &id)) {
    ErrorPtr error;
    Error::AddToPrintf(&error, FROM_HERE, errors::kDomain,
                       errors::kInvalidParams, kInvalidParamValueFormat,
                       kCommandsIdKey, id.c_str());
    return ReturnError(*error, callback);
  }
  cloud_->CancelCommand(id, user_info,
                        base::Bind(&OnCommandRequestSucceeded, callback),
                        base::Bind(&OnCommandRequestFailed, callback));
}

}  // namespace privet
}  // namespace weave
